/*
	Copyright (C) 2022 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License version 2 as published by the
	Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place, Suite 330, Boston, MA 02111-1307 USA

*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <json-c/json.h>

#include "debug.h"
#include "i3.h"

/* util message header macros */
#define I3_HDR_MAGIC               ("i3-ipc")
#define I3_HDR_MAGIC_LENGTH        (sizeof(I3_HDR_MAGIC) - 1)
#define I3_HDR_SIZE_OFFSET         (I3_HDR_MAGIC_LENGTH)
#define I3_HDR_TYPE_OFFSET         (I3_HDR_MAGIC_LENGTH + sizeof(int32_t))
#define I3_HDR_SIZE                (I3_HDR_MAGIC_LENGTH + sizeof(int32_t) * 2)

/* event mask bit (highest bit set) */
#define I3_EVENT_MASK_BIT          (1 << (sizeof(int32_t) * 8 - 1))

/* message types */
#define I3_MSG_TYPE_COMMAND        (0)
#define I3_MSG_TYPE_SUBSCRIBE      (2)
#define I3_MSG_TYPE_WINDOW_EVENT   (3 | I3_EVENT_MASK_BIT)

/* unix socket max path length */
#define SUN_MAX_PATH_LENGTH        (sizeof(((struct sockaddr_un *)(0))->sun_path))

typedef struct i3_incoming_message_header i3_incoming_message_header_t;

struct i3_incoming_message_header {
	char magic[I3_HDR_MAGIC_LENGTH];
	int32_t size;
	int32_t type;
};

static char *
i3_get_socket_path(void)
{
	int fd[2];
	pid_t pid;
	char *path;
	char *argv[3];
	siginfo_t siginfo;
	ssize_t read_count;
	size_t left_to_read;
	size_t total_read_count;

	if (pipe(fd) == -1) {
		dief("pipe failed: %s", strerror(errno));
	}

	if ((pid = fork()) == -1) {
		dief("fork failed: %s", strerror(errno));
	}

	if (pid == 0) {
		if (close(fd[0]) == -1) {
			dief("close failed: %s", strerror(errno));
		}

		dup2(fd[1], STDOUT_FILENO);

		argv[0] = "i3";
		argv[1] = "--get-socketpath";
		argv[2] = NULL;

		if (execvp(argv[0], argv) == -1) {
			exit(127);
		}
	}

	if (close(fd[1]) == -1) {
		dief("close failed: %s", strerror(errno));
	}

	if (NULL == (path = malloc(SUN_MAX_PATH_LENGTH))) {
		die("error while calling malloc, no memory available");
	}

	read_count = -1;
	total_read_count = 0;
	left_to_read = SUN_MAX_PATH_LENGTH;

	while (read_count != 0) {
		if ((read_count = read(fd[0], path + total_read_count, left_to_read)) == -1) {
			dief("read failed: %s", strerror(errno));
		}

		total_read_count += read_count;
		left_to_read -= read_count;
	}

	if (total_read_count > 0) {
		path[total_read_count-1] = '\0';
	}

	memset(&siginfo, 0, sizeof(siginfo));

	if (waitid(P_PID, pid, &siginfo, WEXITED) == -1) {
		dief("waitid failed: %s", strerror(errno));
	}

	switch (siginfo.si_code) {
		case CLD_EXITED:
			if (siginfo.si_status != 0) {
				dief("i3 --get-socketpath failed with exit code: %d",
						siginfo.si_status);
			}
			break;
		case CLD_KILLED:
		case CLD_DUMPED:
			die("i3 --get-socketpath failed");
			break;
	}

	return path;
}

extern i3_connection_t
i3_connect(void)
{
	i3_connection_t conn;
	char *sock_path;
	struct sockaddr_un sock_addr;

	sock_path = i3_get_socket_path();
	conn = socket(AF_UNIX, SOCK_STREAM, 0);

	/* zero the struct, no need to set sock_addr.sun_path[strlen(sock_path)] to '\0' */
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sun_family = AF_UNIX;
	memcpy(&sock_addr.sun_path, sock_path, strlen(sock_path));

	if (connect(conn, (struct sockaddr *)(&sock_addr), sizeof(sock_addr)) != 0) {
		free(sock_path);
		dief("failed to connect to unix socket: %s", strerror(errno));
	}

	free(sock_path);

	return conn;
}

static void
i3_send(i3_connection_t conn, int32_t type, const char *payload)
{
	uint8_t *message;
	int32_t payload_length;
	size_t message_length, position;

	position = 0;
	payload_length = (int32_t)(strlen(payload));
	message_length = I3_HDR_MAGIC_LENGTH + sizeof(int32_t) * 2 + payload_length;

	if (NULL == (message = malloc(message_length))) {
		die("error while calling malloc, no memory available");
	}

	memcpy(message + position, I3_HDR_MAGIC, I3_HDR_MAGIC_LENGTH);
	position += I3_HDR_MAGIC_LENGTH;

	memcpy(message + position, &payload_length, sizeof(int32_t));
	position += sizeof(int32_t);

	memcpy(message + position, &type, sizeof(int32_t));
	position += sizeof(int32_t);

	memcpy(message + position, payload, payload_length);

	if (write(conn, message, message_length) == -1) {
		dief("write failed: %s", strerror(errno));
	}

	free(message);
}

static i3_incoming_message_header_t *
i3_get_incoming_message_header(i3_connection_t conn)
{
	uint8_t buff[I3_HDR_SIZE];
	ssize_t read_count;
	size_t total_read_count, left_to_read;
	i3_incoming_message_header_t *hdr;

	total_read_count = 0;

	/* with tcc sizeof(i3_incoming_message_header_t) returns 16 even */
	/* with __attribute__((packed)), so this is a workaround to make */
	/* the binary produced by tcc work */
	left_to_read = I3_HDR_SIZE;

	if (NULL == (hdr = malloc(sizeof(i3_incoming_message_header_t)))) {
		die("error while calling malloc, no memory available");
	}

	while (left_to_read > 0) {
		if ((read_count = read(conn, &buff[total_read_count], left_to_read)) == -1) {
			dief("error while reading from socket: %s", strerror(errno));
		}

		/* found EOF before end */
		if (read_count == 0) {
			dief("data truncated, expected: %zu, received: %zu", left_to_read +
					total_read_count, total_read_count);
		}

		total_read_count += read_count;
		left_to_read -= read_count;
	}

	memcpy(hdr->magic, buff, I3_HDR_MAGIC_LENGTH);

	hdr->size = *((int32_t *)(&buff[I3_HDR_SIZE_OFFSET]));
	hdr->type = *((int32_t *)(&buff[I3_HDR_TYPE_OFFSET]));

	return hdr;
}

static uint8_t *
i3_get_incoming_message(i3_connection_t conn, int32_t type)
{
	ssize_t read_count;
	size_t total_read_count, left_to_read;
	i3_incoming_message_header_t *header;
	uint8_t *message;

	header = i3_get_incoming_message_header(conn);

	if (strncmp(header->magic, I3_HDR_MAGIC, I3_HDR_MAGIC_LENGTH) != 0) {
		die("corrupted i3 message");
	}

	if (header->type != type) {
		dief("invalid message type, expected: %d, received: %d", type,
				header->type);
	}

	total_read_count = 0;
	left_to_read = header->size;
	free(header);

	if (NULL == (message = malloc(left_to_read + 1))) {
		die("error while calling malloc, no memory available");
	}

	while (left_to_read > 0) {
		if ((read_count = read(conn, message + total_read_count, left_to_read)) == -1) {
			dief("error while reading from socket: %s", strerror(errno));
		}

		/* found EOF before end */
		if (read_count == 0) {
			dief("data truncated, expected: %zu, received: %zu", left_to_read +
					total_read_count, total_read_count);
		}

		total_read_count += read_count;
		left_to_read -= read_count;
	}

	message[total_read_count] = '\0';

	return message;
}

extern void
i3_run_command(i3_connection_t conn, const char *cmd)
{
	uint8_t *raw_reply;
	struct json_object *root, *reply, *success;

	i3_send(conn, I3_MSG_TYPE_COMMAND, cmd);
	raw_reply = i3_get_incoming_message(conn, I3_MSG_TYPE_COMMAND);
	root = json_tokener_parse((char *)(raw_reply));

	free(raw_reply);

	reply = json_object_array_get_idx(root, 0);
	json_object_object_get_ex(reply, "success", &success);

	if (!(json_object_get_boolean(success))) {
		dief("failed to run command: %s", cmd);
	}

	json_object_put(root);
}

extern void
i3_subscribe_to_window_events(i3_connection_t conn)
{
	uint8_t *raw_reply;
	struct json_object *reply, *success;

	i3_send(conn, I3_MSG_TYPE_SUBSCRIBE, "[ \"window\" ]");
	raw_reply = i3_get_incoming_message(conn, I3_MSG_TYPE_SUBSCRIBE);
	reply = json_tokener_parse((char *)(raw_reply));

	free(raw_reply);

	json_object_object_get_ex(reply, "success", &success);

	if (!(json_object_get_boolean(success))) {
		die("failed to subscribe to window events");
	}

	json_object_put(reply);
}

static int32_t
i3_wev_from_str(const char *str)
{
	if (strcmp(str, "new") == 0) 	return I3_WEVCH_NEW;
	if (strcmp(str, "focus") == 0) 	return I3_WEVCH_FOCUS;
	if (strcmp(str, "move") == 0) 	return I3_WEVCH_MOVE;

	return 0;
}

extern i3_window_event_t *
i3_wait_for_window_event(i3_connection_t conn)
{

	/*
		window_event
			├ change: str
			└ container
				┊
				├ ...
				┊
				└ window_rect
					├ width: number
					└ height: number

	*/

	uint8_t *raw_event;
	struct json_object *root, *change, *container, *window_rect, *width, *height;
	i3_window_event_t *ev;

	/* parse the full event json */
	raw_event = i3_get_incoming_message(conn, I3_MSG_TYPE_WINDOW_EVENT);
	root = json_tokener_parse((char *)(raw_event));
	free(raw_event);

	json_object_object_get_ex(root, "change", &change);
	json_object_object_get_ex(root, "container", &container);

	json_object_object_get_ex(container, "window_rect", &window_rect);
	json_object_object_get_ex(window_rect, "width", &width);
	json_object_object_get_ex(window_rect, "height", &height);

	if (NULL == (ev = malloc(sizeof(i3_window_event_t)))) {
		die("error while calling malloc, no memory available");
	}

	ev->change = i3_wev_from_str(json_object_get_string(change));
	ev->width = json_object_get_int(width);
	ev->height = json_object_get_int(height);

	json_object_put(root);

	return ev;
}

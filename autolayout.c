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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>

#define I3_MAGIC "i3-ipc"
#define I3_MAGIC_LENGTH (sizeof(I3_MAGIC) - 1)
#define I3_MSG_TYPE_COMMAND 0
#define I3_MSG_TYPE_SUBSCRIBE 2
#define I3_WINDOW_EVENT 3
#define I3_EVENT_MASK_BIT (1 << (sizeof(int32_t) * 8 - 1))
#define SUN_MAX_PATH_LENGTH sizeof(((struct sockaddr_un *)0)->sun_path)
#define MATCH_OPT(arg, sh, lo) ((strcmp((arg), (sh)) == 0) || (strcmp((arg), (lo)) == 0))

typedef struct __attribute__((packed)) {
	char magic[6];
	int32_t size;
	int32_t type;
} i3_incoming_message_header;


static void
usage(void) {
	puts("Usage: autolayout [ -hd ]");
	puts("Options are:");
	puts("     -d | --daemon                  run in the background");
	puts("     -h | --help                    display this message and exit");
	exit(0);
}

static void
die(const char *err) {
	fprintf(stderr, "autolayout: %s\n", err);
	exit(1);
}

static void
dief(const char *err, ...) {
	va_list list;
	fputs("autolayout: ", stderr);
	va_start(list, err);
	vfprintf(stderr, err, list);
	va_end(list);
	fputs("\n", stderr);
	exit(1);
}

static char *
i3_get_socket_path(void) {
	size_t index = 0;
	int chstatus;
	char *path, current;
	FILE *f;

	if ((path = malloc(SUN_MAX_PATH_LENGTH)) == NULL)
		die("error while calling malloc()");

	if ((f = popen("i3 --get-socketpath 2>/dev/null", "r")) == NULL)
		die("error while calling popen()");

	while ((current = fgetc(f)) != EOF && current != '\n') {
		if (index == (SUN_MAX_PATH_LENGTH - 1))
			dief("invalid unix socket path, max length supported: %zu", SUN_MAX_PATH_LENGTH);
		path[index++] = current;
	}

	path[index] = '\0';

	if ((chstatus = pclose(f)) == -1) {
		die("error while calling pclose()");
	}

	switch (WEXITSTATUS(chstatus)) {
		case 0:
			return path;
		case 127:
			die("i3 isn't installed on your computer");
			break;
		default:
			die("i3 isn't running");
			break;
	}

	/* unreachable */
	return NULL;
}

static void
i3_connect(int *sockfd) {
	char *sock_path = i3_get_socket_path();
	struct sockaddr_un sock_addr;

	*sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sun_family = AF_UNIX;
    memcpy(&sock_addr.sun_path, sock_path, strlen(sock_path));

	if (connect(*sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) != 0) {
		free(sock_path);
		die("error while calling connect()");
	}

	free(sock_path);
}

static void
i3_send(int sockfd, int32_t type, const char *payload) {
	unsigned char *message;
	int32_t payload_length;
	size_t message_length, position;

	position = 0;
	payload_length = (int32_t)(strlen(payload));
	message_length = I3_MAGIC_LENGTH + sizeof(int32_t) * 2 + payload_length;

	if ((message = malloc(message_length)) == NULL) {
		die("error while calling malloc()");
	}

	memcpy(message + position, I3_MAGIC, I3_MAGIC_LENGTH);
	position += I3_MAGIC_LENGTH;

	memcpy(message + position, &payload_length, sizeof(int32_t));
	position += sizeof(int32_t);

	memcpy(message + position, &type, sizeof(int32_t));
	position += sizeof(int32_t);

	memcpy(message + position, payload, payload_length);

	write(sockfd, message, message_length);

	free(message);
}

static i3_incoming_message_header *
i3_get_incoming_message_header(int sockfd) {
	unsigned char *header;
	size_t read_count, total_read_count, left_to_read;

	total_read_count = 0;
	left_to_read = sizeof(i3_incoming_message_header);

	if ((header = malloc(left_to_read)) == NULL) {
		die("error while calling malloc()");
	}

	while (left_to_read > 0) {
		if ((read_count = read(sockfd, header + total_read_count, left_to_read)) == -1)
			die("error while calling read()");

		/* found EOF before end */
		if (read_count == 0)
			dief("data truncated, expected: %zu, received: %zu", left_to_read + total_read_count, total_read_count);

		total_read_count += read_count;
		left_to_read -= read_count;
	}

	return (i3_incoming_message_header *)(header);
}

static unsigned char *
i3_get_incoming_message(int sockfd, int32_t type) {
	size_t read_count, total_read_count, left_to_read;
	i3_incoming_message_header *header;
	unsigned char *message;

	header = i3_get_incoming_message_header(sockfd);

	if (strncmp(header->magic, I3_MAGIC, I3_MAGIC_LENGTH) != 0) {
		die("received incorrect magic string");
	}

	if (header->type != type) {
		dief("invalid message type, expected: %d, received: %d", type, header->type);
	}

	total_read_count = 0;
	left_to_read = header->size;
	free(header);

	if ((message = malloc(left_to_read + 1)) == NULL) {
		die("error while calling malloc()");
	}

	while (left_to_read > 0) {
		if ((read_count = read(sockfd, message + total_read_count, left_to_read)) == -1)
			die("error while calling read()");

		/* found EOF before end */
		if (read_count == 0)
			dief("data truncated, expected: %zu, received: %zu", left_to_read + total_read_count, total_read_count);

		total_read_count += read_count;
		left_to_read -= read_count;
	}

	message[total_read_count] = '\0';

	return message;
}

static void
i3_run_command(int sockfd, const char *cmd) {
	unsigned char *message;

	i3_send(sockfd, I3_MSG_TYPE_COMMAND, cmd);
	message = i3_get_incoming_message(sockfd, I3_MSG_TYPE_COMMAND);

	if (strncmp((char *)(message), "[{\"success\":true}]", 18) != 0) {
		dief("failed to run command: %s", cmd);
	}

	free(message);
}

static void
i3_subscribe_to_window_events(int sockfd) {
	unsigned char *message;

	i3_send(sockfd, I3_MSG_TYPE_SUBSCRIBE, "[ \"window\" ]");
	message = i3_get_incoming_message(sockfd, I3_MSG_TYPE_SUBSCRIBE);

	if (strncmp((char *)(message), "{\"success\":true}", 16) != 0) {
		die("failed to subscribe to window events");
	}

	free(message);
}

static char *
i3_get_window_event(int sockfd) {
	return (char *)(i3_get_incoming_message(sockfd, I3_EVENT_MASK_BIT | I3_WINDOW_EVENT));
}

static void
daemonize(void) {
	switch (fork()) {
		case -1:
			die("fork failed");
			break;
		case 0:
			break;
		default:
			exit(0);
			break;
	}
}

int
main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (MATCH_OPT(argv[i], "-d", "--daemon")) daemonize();
		else if (MATCH_OPT(argv[i], "-h", "--help")) usage();
		else dief("invalid option %s", argv[i]);
	}

	char *raw_event;
	int cmd_sockfd, evt_sockfd;

	i3_connect(&cmd_sockfd);
	i3_connect(&evt_sockfd);

	i3_subscribe_to_window_events(evt_sockfd);

	while ((raw_event = i3_get_window_event(evt_sockfd))) {
		if (strstr(raw_event, "\"change\":\"focus\"")) {
			char *pos = strstr(raw_event, "window_rect");

			int width = atoi(strstr(pos, "width") + 7);
			int height = atoi(strstr(pos, "height") + 8);

			if (width > height) i3_run_command(cmd_sockfd, "split h");
			else i3_run_command(cmd_sockfd, "split v");
		}

		free(raw_event);
	}

	return 0;
}

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <json-c/json.h>

#include "numdef.h"
#include "debug.h"
#include "i3.h"

#define I3_MAGIC 				("i3-ipc")
#define I3_MAGIC_LENGTH 		(sizeof(I3_MAGIC) - 1)
#define I3_MSG_TYPE_COMMAND 	(0)
#define I3_MSG_TYPE_SUBSCRIBE 	(2)
#define I3_WINDOW_EVENT 		(3)
#define I3_EVENT_MASK_BIT 		(1 << (sizeof(i32) * 8 - 1))
#define SUN_MAX_PATH_LENGTH 	(sizeof(((struct sockaddr_un *)(0))->sun_path))

struct __attribute__((packed)) i3_incoming_message_header {
	char magic[6];
	i32 size;
	i32 type;
};

typedef struct i3_incoming_message_header i3_incoming_message_header_t;

static char *
i3_get_socket_path(void) {
	size_t index = 0;
	int chstatus;
	char *path, current;
	FILE *f;

	if (!(path = malloc(SUN_MAX_PATH_LENGTH))) {
		die("error while calling malloc, no memory available");
	}

	if (!(f = popen("i3 --get-socketpath 2>/dev/null", "r"))) {
		dief("failed to open pipe: %s", strerror(errno));
	}

	while ((current = fgetc(f)) != EOF && current != '\n') {
		if (index == (SUN_MAX_PATH_LENGTH - 1)) {
			dief("invalid unix socket path, max length supported: %zu",
					SUN_MAX_PATH_LENGTH);
		}
		path[index++] = current;
	}

	path[index] = '\0';

	if ((chstatus = pclose(f)) == -1) {
		dief("failed to close pipe: %s", strerror(errno));
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

extern i3_connection_t
i3_connect(void) {
	i3_connection_t conn;
	char *sock_path;
	struct sockaddr_un sock_addr;

	sock_path = i3_get_socket_path();
	conn = socket(AF_UNIX, SOCK_STREAM, 0);

	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sun_family = AF_UNIX;
	memcpy(&sock_addr.sun_path, sock_path, strlen(sock_path));

	if (connect(conn, (struct sockaddr*)(&sock_addr), sizeof(sock_addr)) != 0) {
		free(sock_path);
		dief("failed to connect to unix socket: %s", strerror(errno));
	}

	free(sock_path);

	return conn;
}

extern void
i3_send(i3_connection_t conn, i32 type, const char *payload) {
	u8 *message;
	i32 payload_length;
	size_t message_length, position;

	position = 0;
	payload_length = (i32)(strlen(payload));
	message_length = I3_MAGIC_LENGTH + sizeof(i32) * 2 + payload_length;

	if (!(message = malloc(message_length))) {
		die("error while calling malloc, no memory available");
	}

	memcpy(message + position, I3_MAGIC, I3_MAGIC_LENGTH);
	position += I3_MAGIC_LENGTH;

	memcpy(message + position, &payload_length, sizeof(i32));
	position += sizeof(i32);

	memcpy(message + position, &type, sizeof(i32));
	position += sizeof(i32);

	memcpy(message + position, payload, payload_length);

	write(conn, message, message_length);

	free(message);
}

static i3_incoming_message_header_t *
i3_get_incoming_message_header(i3_connection_t conn) {
	u8 *header;
	ssize_t read_count;
	size_t total_read_count, left_to_read;

	total_read_count = 0;
	left_to_read = sizeof(i3_incoming_message_header_t);

	if (!(header = malloc(left_to_read))) {
		die("error while calling malloc, no memory available");
	}

	while (left_to_read > 0) {
		if ((read_count = read(conn, header + total_read_count, left_to_read)) == -1) {
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

	return (i3_incoming_message_header_t *)(header);
}

static u8 *
i3_get_incoming_message(i3_connection_t conn, i32 type) {
	ssize_t read_count;
	size_t total_read_count, left_to_read;
	i3_incoming_message_header_t *header;
	u8 *message;

	header = i3_get_incoming_message_header(conn);

	if (strncmp(header->magic, I3_MAGIC, I3_MAGIC_LENGTH) != 0) {
		die("corrupted i3 message");
	}

	if (header->type != type) {
		dief("invalid message type, expected: %d, received: %d", type, header->type);
	}

	total_read_count = 0;
	left_to_read = header->size;
	free(header);

	if (!(message = malloc(left_to_read + 1))) {
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
i3_run_command(i3_connection_t conn, const char *cmd) {
	u8 *message;

	i3_send(conn, I3_MSG_TYPE_COMMAND, cmd);
	message = i3_get_incoming_message(conn, I3_MSG_TYPE_COMMAND);

	if (strncmp((char *)(message), "[{\"success\":true}]", 18) != 0) {
		dief("failed to run command: %s", cmd);
	}

	free(message);
}

extern void
i3_subscribe_to_window_events(i3_connection_t conn) {
	u8 *message;

	i3_send(conn, I3_MSG_TYPE_SUBSCRIBE, "[ \"window\" ]");
	message = i3_get_incoming_message(conn, I3_MSG_TYPE_SUBSCRIBE);

	if (strncmp((char *)(message), "{\"success\":true}", 16) != 0) {
		die("failed to subscribe to window events");
	}

	free(message);
}

static i32
i3_wev_from_str(const char *str) {
	if (strcmp(str, "new") == 0) 	return I3_WEVCH_NEW;
	if (strcmp(str, "focus") == 0) 	return I3_WEVCH_FOCUS;
	if (strcmp(str, "move") == 0) 	return I3_WEVCH_MOVE;

	return 0;
}

extern i3_window_event_t *
i3_wait_for_window_event(i3_connection_t conn) {

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

	u8 *raw_event;
	struct json_object *root, *change, *container, *window_rect, *width, *height;
	i3_window_event_t *ev;

	/* parse the full event json */
	raw_event = i3_get_incoming_message(conn, I3_EVENT_MASK_BIT | I3_WINDOW_EVENT);
	root = json_tokener_parse((char *)(raw_event));
	free(raw_event);

	json_object_object_get_ex(root, "change", &change);
	json_object_object_get_ex(root, "container", &container);

	json_object_object_get_ex(container, "window_rect", &window_rect);
	json_object_object_get_ex(window_rect, "width", &width);
	json_object_object_get_ex(window_rect, "height", &height);

	if(!(ev = malloc(sizeof(i3_window_event_t)))) {
		die("error while calling malloc, no memory available");
	}

	ev->change = i3_wev_from_str(json_object_get_string(change));
	ev->width = json_object_get_int(width);
	ev->height = json_object_get_int(height);

	json_object_put(root);

	return ev;
}

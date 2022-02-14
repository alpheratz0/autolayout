#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

#define I3_MAGIC "i3-ipc"
#define I3_MAGIC_LENGTH (sizeof(I3_MAGIC) - 1)
#define I3_MSG_TYPE_COMMAND 0
#define I3_MSG_TYPE_SUBSCRIBE 2
#define I3_EVENT_MASK_BIT (1 << (sizeof(int) * 8 - 1))
#define SUN_MAX_PATH_LENGTH sizeof(((struct sockaddr_un *)0)->sun_path)

typedef struct __attribute__((packed)) {
	char magic[6];
	int size;
	int type;
} i3_incoming_message_header;

extern void
die(const char *err) {
	fprintf(stderr, "autolayout: %s\n", err);
	exit(1);
}

extern void
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
	char *path, current;
	FILE *f;

	if ((path = malloc(SUN_MAX_PATH_LENGTH)) == NULL)
		die("error while calling malloc()");

	if ((f = popen("i3 --get-socketpath 2>/dev/null", "r")) == NULL)
		die("error while calling popen()");

	while ((current = fgetc(f)) != EOF && current != '\n') {
		if (index == (SUN_MAX_PATH_LENGTH - 1))
			die("invalid socket path length");
		path[index++] = current;
	}

	path[index] = '\0';

	switch (pclose(f)) {
		case 0:
			return path;
		case -1:
			die("error while calling pclose()");
			break;
		default:
			die("i3 --get-socket-path returned non-zero exit status code");
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
i3_send(int sockfd, int type, const char *payload) {
	unsigned char *message;
	size_t payload_length, message_length;
	size_t position = 0;

	payload_length = strlen(payload);
	message_length = I3_MAGIC_LENGTH + sizeof(int) * 2 + payload_length;

	if ((message = malloc(message_length)) == NULL) {
		die("error while calling malloc()");
	}

	memcpy(message + position, I3_MAGIC, I3_MAGIC_LENGTH);
	position += I3_MAGIC_LENGTH;

	memcpy(message + position, &payload_length, sizeof(int));
	position += sizeof(int);

	memcpy(message + position, &type, sizeof(int));
	position += sizeof(int);

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
		if ((read_count = read(sockfd, header + total_read_count, left_to_read)) == EOF)
			break;
		total_read_count += read_count;
		left_to_read -= read_count;
	}

	if (left_to_read > 0) {
		die("error while reading incoming message header, data truncated");
	}

	return (i3_incoming_message_header *)(header);
}

static void
i3_run_command(int sockfd, const char *cmd) {
	size_t read_count, total_read_count, left_to_read;
	i3_incoming_message_header *header;
	unsigned char *message;

	i3_send(sockfd, I3_MSG_TYPE_COMMAND, cmd);
	header = i3_get_incoming_message_header(sockfd);

	if (strncmp(header->magic, I3_MAGIC, I3_MAGIC_LENGTH) != 0) {
		die("bad magic string");
	}

	if (header->type != I3_MSG_TYPE_COMMAND) {
		die("bad response type");
	}

	total_read_count = 0;
	left_to_read = header->size;
	free(header);

	if ((message = malloc(left_to_read)) == NULL) {
		die("error while calling malloc()");
	}

	while (left_to_read > 0) {
		if ((read_count = read(sockfd, message + total_read_count, left_to_read)) == EOF)
			break;
		total_read_count += read_count;
		left_to_read -= read_count;
	}

	if (left_to_read > 0) {
		die("error while reading message, data truncated");
	}

	if (strncmp((char *)(message), "[{\"success\":true}]", 18) != 0) {
		die("couldn't run command");
	}

	free(message);
}

static void
i3_subscribe_to_window_events(int sockfd) {
	size_t read_count, total_read_count, left_to_read;
	i3_incoming_message_header *header;
	unsigned char *message;

	i3_send(sockfd, I3_MSG_TYPE_SUBSCRIBE, "[ \"window\" ]");
	header = i3_get_incoming_message_header(sockfd);

	if (strncmp(header->magic, I3_MAGIC, I3_MAGIC_LENGTH) != 0) {
		die("bad magic string");
	}

	if (header->type != I3_MSG_TYPE_SUBSCRIBE) {
		die("bad response type");
	}

	total_read_count = 0;
	left_to_read = header->size;
	free(header);

	if ((message = malloc(left_to_read)) == NULL) {
		die("error while calling malloc()");
	}

	while (left_to_read > 0) {
		if ((read_count = read(sockfd, message + total_read_count, left_to_read)) == EOF)
			break;
		total_read_count += read_count;
		left_to_read -= read_count;
	}

	if (left_to_read > 0) {
		die("error while reading message, data truncated");
	}

	if (strncmp((char *)(message), "{\"success\":true}", 16) != 0) {
		die("couldn't subscribe to window events");
	}

	free(message);
}

static char *
i3_get_window_event(int sockfd) {
	size_t read_count, total_read_count, left_to_read;
	i3_incoming_message_header *header;
	unsigned char *message;

	header = i3_get_incoming_message_header(sockfd);

	if (strncmp(header->magic, I3_MAGIC, I3_MAGIC_LENGTH) != 0) {
		die("bad magic string");
	}

	if ((header->type & I3_EVENT_MASK_BIT) == 0) {
		die("bad response type");
	}

	total_read_count = 0;
	left_to_read = header->size;
	free(header);

	if ((message = malloc(left_to_read + 1)) == NULL) {
		die("error while calling malloc()");
	}

	while (left_to_read > 0) {
		if ((read_count = read(sockfd, message + total_read_count, left_to_read)) == EOF)
			break;
		total_read_count += read_count;
		left_to_read -= read_count;
	}

	if (left_to_read > 0) {
		die("error while reading message, data truncated");
	}

	message[total_read_count] = '\0';

	return (char *)(message);
}

int
main(int argc, char **argv) {
	if (argc > 1 && strcmp(argv[1], "--daemon") == 0) {
		switch (fork()) {
			case -1:
				die("fork failed");
				break;
			case 0:
				exit(0);
				break;
			default:
				break;
		}
	}

	char *message;
	int cmd_sockfd, evt_sockfd;

	i3_connect(&cmd_sockfd);
	i3_connect(&evt_sockfd);

	i3_subscribe_to_window_events(evt_sockfd);

	while ((message = i3_get_window_event(evt_sockfd))) {
		if (strstr(message, "\"change\":\"focus\"")) {
			char *pos = strstr(message, "window_rect");

			int width = atoi(strstr(pos, "width") + 7);
			int height = atoi(strstr(pos, "height") + 8);

			if (width > height) i3_run_command(cmd_sockfd, "split h");
			else i3_run_command(cmd_sockfd, "split v");
		}

		free(message);
	}
	
	return 0;
}

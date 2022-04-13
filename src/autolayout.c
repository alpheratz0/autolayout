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

	 _____________________________
	( auto layout > manual layout )
	 -----------------------------
	    o               ,-----._
	  .  o         .  ,'        `-.__,------._
	 //   o      __\\'                        `-.
	((    _____-'___))                           |
	 `:='/     (alf_/                            |
	 `.=|      |='                               |
	    |)   O |                                  \
	    |      |                               /\  \
	    |     /                          .    /  \  \
	    |    .-..__            ___   .--' \  |\   \  |
	   |o o  |     ``--.___.  /   `-'      \  \\   \ |
	    `--''        '  .' / /             |  | |   | \
	                 |  | / /              |  | |   mmm
	                 |  ||  |              | /| |
	                 ( .' \ \              || | |
	                 | |   \ \            // / /
	                 | |    \ \          || |_|
	                /  |    |_/         /_|
	               /__/

*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/types.h>

#include "i3.h"
#include "debug.h"

static bool
match_opt(const char *in, const char *sh, const char *lo) {
	return (strcmp(in, sh) == 0) ||
		   (strcmp(in, lo) == 0);
}

static void
usage(void) {
	puts("Usage: autolayout [ -hdv ]");
	puts("Options are:");
	puts("     -d | --daemon                  run in the background");
	puts("     -h | --help                    display this message and exit");
	puts("     -v | --version                 display program version");
	exit(0);
}

static void
version(void) {
	puts("autolayout version "VERSION);
	exit(0);
}

static void
daemonize(void) {
	int dnfd;
	sigset_t ss;
	struct rlimit limits = { 0 };

	getrlimit(RLIMIT_NOFILE, &limits);

	while(--limits.rlim_max > 2) {
		close((int)(limits.rlim_max));
	}

	for (int s = 1; s < _NSIG; ++s) {
		signal(s, SIG_DFL);
	}

	sigemptyset(&ss);
	sigprocmask(SIG_SETMASK, &ss, NULL);

	switch(fork()) {
		case -1:
			dief("fork failed: %s", strerror(errno));
			break;
		case 0:
			if(setsid() == -1) {
				dief("setsid failed: %s", strerror(errno));
			}
			break;
		default:
			exit(0);
			break;
	}

	switch(fork()) {
		case -1:
			dief("fork failed: %s", strerror(errno));
			break;
		case 0:
			umask(0);
			chdir("/");

			if((dnfd = open("/dev/null", O_RDWR)) == -1) {
				dief("open failed: %s", strerror(errno));
			}

			dup2(dnfd, STDIN_FILENO);
			dup2(dnfd, STDOUT_FILENO);
			dup2(dnfd, STDERR_FILENO);
			break;
		default:
			exit(0);
			break;
	}
}

int
main(int argc, char **argv) {
	/* skip program name */
	--argc; ++argv;

	if (argc > 0) {
		if (match_opt(*argv, "-d", "--daemon")) daemonize();
		else if (match_opt(*argv, "-h", "--help")) usage();
		else if (match_opt(*argv, "-v", "--version")) version();
		else dief("invalid option %s", *argv);
	}

	i3_window_event_t *ev;

	/* create two different connections (to prevent race conditions), */
	/* one for the commands and other for the events */
	/* https://i3wm.org/docs/ipc.html#events */
	i3_connection_t ccmd, cevt;

	ccmd = i3_connect();
	cevt = i3_connect();

	i3_subscribe_to_window_events(cevt);

	while ((ev = i3_wait_for_window_event(cevt))) {
		if (ev->change & (I3_WEVCH_FOCUS | I3_WEVCH_NEW | I3_WEVCH_MOVE)) {
			i3_run_command(
				ccmd,
				ev->width > ev->height ?
					"split h" :
					"split v"
			);
		}
		free(ev);
	}

	return 0;
}
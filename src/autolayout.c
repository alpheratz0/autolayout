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

#define _XOPEN_SOURCE 500

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "i3.h"
#include "debug.h"

static void
usage(void)
{
	puts("usage: autolayout [-bhv]");
	exit(0);
}

static void
version(void)
{
	puts("autolayout version "VERSION);
	exit(0);
}

static void
fork_keep_child(void)
{
	switch (fork()) {
		case 0: break;
		case -1: die("fork failed: %s", strerror(errno)); break;
		default: exit(0); break;
	}
}

static void
daemonize(void)
{
	fork_keep_child();
	setsid();
	fork_keep_child();
	umask(0);
	chdir("/");
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}

int
main(int argc, char **argv)
{
	/* create two different connections (to prevent race conditions), */
	/* one for the commands and other for the events */
	/* https://i3wm.org/docs/ipc.html#events */
	i3_connection ccmd, cevt;

	struct i3_window_event ev;

	while (++argv, --argc > 0) {
		if ((*argv)[0] == '-' && (*argv)[1] != '\0' && (*argv)[2] == '\0') {
			switch ((*argv)[1]) {
				case 'h': usage(); break;
				case 'v': version(); break;
				case 'b': daemonize(); break;
				default: die("invalid option %s", *argv); break;
			}
		} else {
			die("unexpected argument: %s", *argv);
		}
	}

	ccmd = i3_connect();
	cevt = i3_connect();

	i3_subscribe_to_window_events(cevt);

	while (1) {
		i3_wait_for_window_event(cevt, &ev);

		if (ev.change & (I3_WEVCH_FOCUS | I3_WEVCH_NEW | I3_WEVCH_MOVE)) {
			i3_run_command(
				ccmd,
				ev.width > ev.height ?
					"split h" :
					"split v"
			);
		}
	}

	/* UNREACHABLE */
	return 0;
}

/*
	Copyright (C) 2022-2023 <alpheratz99@protonmail.com>

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License version 2 as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 59
	Temple Place, Suite 330, Boston, MA 02111-1307 USA

*/

#ifndef __AUTOLAYOUT_I3_H__
#define __AUTOLAYOUT_I3_H__

#include <stdint.h>

typedef int i3_connection;

enum i3_window_event_change {
	I3_WEVCH_UNKNOWN =      0,
	I3_WEVCH_NEW     = 1 << 0,
	I3_WEVCH_FOCUS   = 1 << 1,
	I3_WEVCH_MOVE    = 1 << 2
};

struct i3_window_event {
	enum i3_window_event_change change;
	struct {
		int32_t w;
		int32_t h;
	} size;
};

extern i3_connection
i3_connect(void);

extern void
i3_run_command(i3_connection conn, const char *cmd);

extern void
i3_subscribe_to_window_events(i3_connection conn);

extern void
i3_wait_for_window_event(i3_connection conn, struct i3_window_event *ev);

#endif

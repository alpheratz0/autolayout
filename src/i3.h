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

#ifndef __AUTOLAYOUT_I3_H__
#define __AUTOLAYOUT_I3_H__

#include <stdint.h>

#define I3_WEVCH_NEW                ((1) << 0)
#define I3_WEVCH_FOCUS              ((1) << 1)
#define I3_WEVCH_MOVE               ((1) << 2)

typedef int i3_connection;

struct i3_window_event {
	int32_t change;
	int32_t width;
	int32_t height;
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

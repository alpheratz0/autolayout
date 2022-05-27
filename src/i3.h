#ifndef __AUTOLAYOUT_I3_H__
#define __AUTOLAYOUT_I3_H__

#include <stdint.h>

#define I3_WEVCH_NEW                ((1) << 0)
#define I3_WEVCH_FOCUS              ((1) << 1)
#define I3_WEVCH_MOVE               ((1) << 2)

typedef int i3_connection_t;
typedef struct i3_window_event i3_window_event_t;

struct i3_window_event {
	int32_t change;
	int32_t width;
	int32_t height;
};

extern i3_connection_t
i3_connect(void);

extern void
i3_run_command(i3_connection_t conn, const char *cmd);

extern void
i3_subscribe_to_window_events(i3_connection_t conn);

extern i3_window_event_t *
i3_wait_for_window_event(i3_connection_t conn);

#endif

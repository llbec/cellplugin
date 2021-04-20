#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool vcamera_start(void *p, int w, int h);
extern void vcamera_frame_event();
extern void vcamera_stop();

#ifdef __cplusplus
}
#endif

/*
 * Task Switcher (Alt+Tab) for uOS(m) Desktop
 * Overlay thumbnails of open windows
 */

#ifndef _TASK_SWITCHER_H_
#define _TASK_SWITCHER_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/window_manager/window_mgr.h"
#include "../ui/anim_window.h"
#include "overview.h"

#define TS_MAX_WINDOWS    32
#define TS_OVERLAY_ALPHA  220
#define TS_THUMB_WIDTH    240
#define TS_THUMB_HEIGHT   320

typedef enum {
    TS_DIR_NEXT,
    TS_DIR_PREV
} ts_direction_t;

typedef struct {
    window_t *window;
    int thumb_x, thumb_y;
    int thumb_w, thumb_h;
    int focused;
    int hovered;
    char title[64];
    anim_window_data_t anim_data;
} ts_window_t;

typedef struct {
    ts_window_t windows[TS_MAX_WINDOWS];
    int count;
    int visible;
    int active_idx;
    int hovered_idx;
    int overlay_x, overlay_y;
    int overlay_w, overlay_h;
    int switching;
    uint64_t switch_timer;
    surface_t *bg_surface;
    surface_t *thumb_surfaces[TS_MAX_WINDOWS];
} task_switcher_t;

int ts_init(void);
void ts_shutdown(void);
void ts_track_window(window_t *window);
void ts_untrack_window(window_t *window);
void ts_show(void);
void ts_hide(void);
void ts_next(void);
void ts_prev(void);
int ts_activate(void);
int ts_handle_touch(int x, int y, int action);
void ts_render(void);
void ts_update(void);
task_switcher_t *ts_get_instance(void);

#endif /* _TASK_SWITCHER_H_ */

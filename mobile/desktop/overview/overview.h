/*
 * Overview / App Switcher for uOS(m) Desktop
 * Grid of window thumbnails, search, workspace indicators
 */

#ifndef _OVERVIEW_H_
#define _OVERVIEW_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/window_manager/window_mgr.h"

#define OVERVIEW_MAX_WINDOWS  32
#define OVERVIEW_SEARCH_WIDTH 400
#define OVERVIEW_THUMB_WIDTH  280
#define OVERVIEW_THUMB_HEIGHT 400
#define OVERVIEW_WS_INDICATOR_H 40

typedef struct {
    window_t *window;
    int thumb_x, thumb_y;
    int thumb_width, thumb_height;
    int focused;
    int hovered;
    char title[64];
} overview_win_t;

typedef struct {
    overview_win_t windows[OVERVIEW_MAX_WINDOWS];
    int window_count;
    int cols, rows;
    int grid_x, grid_y;
    int grid_width, grid_height;
    int search_x, search_y, search_w, search_h;
    int ws_bar_x, ws_bar_y, ws_bar_w, ws_bar_h;
    char search_text[128];
    int active_workspace;
    int workspace_count;
    int visible;
    int active_idx;
    int hovered_idx;
    surface_t *bg_surface;
    surface_t *thumb_surfaces[OVERVIEW_MAX_WINDOWS];
} overview_t;

int overview_init(void);
void overview_shutdown(void);
void overview_show(void);
void overview_hide(void);
void overview_render(layer_t *ui_layer);
void overview_update(void);
int overview_handle_touch(int x, int y, int action);
void overview_filter(const char *text);
void overview_close_window(int idx);
void overview_activate_window(int idx);
void overview_set_workspace(int idx);
void overview_set_workspace_count(int count);
void overview_refresh_windows(void);
overview_t *overview_get_instance(void);

#endif /* _OVERVIEW_H_ */

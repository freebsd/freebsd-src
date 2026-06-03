/*
 * Virtual Desktop Manager for uOS(m)
 * Multiple workspaces with independent window sets
 */

#ifndef _WORKSPACE_H_
#define _WORKSPACE_H_

#include <stdint.h>
#include "../ui/window_manager/window_mgr.h"

#define WS_MAX_COUNT        8
#define WS_NAME_MAX         32
#define WS_INDICATOR_HEIGHT 36
#define WS_INDICATOR_WIDTH  36
#define WS_INDICATOR_SPACING 8

typedef enum {
    WS_LAYOUT_TILED,
    WS_LAYOUT_FLOATING,
    WS_LAYOUT_STACKED
} ws_layout_t;

typedef struct {
    char name[WS_NAME_MAX];
    window_t *windows[MAX_WINDOWS];
    int window_count;
    int id;
    ws_layout_t layout;
} workspace_t;

typedef struct {
    workspace_t workspaces[WS_MAX_COUNT];
    int count;
    int current_idx;
    int initialized;
    int switching;
    int switch_from;
    int switch_to;
    int switch_direction;
    uint64_t switch_start;
    int indicator_x, indicator_y;
} workspace_mgr_t;

int ws_init(int count);
void ws_shutdown(void);
int ws_set_current(int idx);
int ws_get_current(void);
workspace_t *ws_get_workspace(int idx);
int ws_add_window(window_t *window, int ws_idx);
int ws_remove_window(window_t *window, int ws_idx);
int ws_move_window(window_t *window, int from_ws, int to_ws);
void ws_switch_next(void);
void ws_switch_prev(void);
void ws_render_indicators(layer_t *layer, int screen_w, int screen_h);
int ws_handle_touch_indicators(int x, int y);
workspace_mgr_t *ws_get_instance(void);
void ws_set_layout(int ws_idx, ws_layout_t layout);
void ws_set_name(int ws_idx, const char *name);

#endif /* _WORKSPACE_H_ */

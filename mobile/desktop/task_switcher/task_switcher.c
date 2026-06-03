/*
 * Task Switcher (Alt+Tab) Implementation
 */

#include "task_switcher.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/window_manager/window_mgr.h"
#include <stdlib.h>
#include <string.h>

static task_switcher_t *g_ts = NULL;

int ts_init(void) {
    if (g_ts) return 0;
    g_ts = calloc(1, sizeof(task_switcher_t));
    if (!g_ts) return -1;
    g_ts->visible = 0;
    g_ts->active_idx = -1;
    g_ts->overlay_x = (FB_WIDTH - 800) / 2;
    g_ts->overlay_y = 120;
    g_ts->overlay_w = 800;
    g_ts->overlay_h = 500;
    return 0;
}

void ts_shutdown(void) {
    if (!g_ts) return;
    free(g_ts);
    g_ts = NULL;
}

void ts_track_window(window_t *window) {
    if (!g_ts || !window) return;
    for (int i = 0; i < g_ts->count; i++)
        if (g_ts->windows[i].window == window) return;
    if (g_ts->count >= TS_MAX_WINDOWS) return;
    ts_window_t *w = &g_ts->windows[g_ts->count++];
    w->window = window;
    strncpy(w->title, window->title, sizeof(w->title) - 1);
    w->thumb_w = TS_THUMB_WIDTH;
    w->thumb_h = TS_THUMB_HEIGHT;
}

void ts_untrack_window(window_t *window) {
    if (!g_ts || !window) return;
    for (int i = 0; i < g_ts->count; i++) {
        if (g_ts->windows[i].window == window) {
            memmove(&g_ts->windows[i], &g_ts->windows[i + 1], (g_ts->count - i - 1) * sizeof(ts_window_t));
            g_ts->count--;
            break;
        }
    }
}

void ts_show(void) {
    if (!g_ts || g_ts->count == 0) return;
    g_ts->visible = 1;
    g_ts->active_idx = 0;
    g_ts->windows[0].focused = 1;
    for (int i = 1; i < g_ts->count; i++) g_ts->windows[i].focused = 0;
}

void ts_hide(void) {
    if (!g_ts) return;
    g_ts->visible = 0;
    g_ts->active_idx = -1;
}

void ts_next(void) {
    if (!g_ts || g_ts->count <= 1) return;
    g_ts->windows[g_ts->active_idx].focused = 0;
    g_ts->active_idx = (g_ts->active_idx + 1) % g_ts->count;
    g_ts->windows[g_ts->active_idx].focused = 1;
    g_ts->switching = 1;
    g_ts->switch_timer = 0;
}

void ts_prev(void) {
    if (!g_ts || g_ts->count <= 1) return;
    g_ts->windows[g_ts->active_idx].focused = 0;
    g_ts->active_idx = (g_ts->active_idx - 1 + g_ts->count) % g_ts->count;
    g_ts->windows[g_ts->active_idx].focused = 1;
    g_ts->switching = 1;
    g_ts->switch_timer = 0;
}

int ts_activate(void) {
    if (!g_ts || g_ts->active_idx < 0 || g_ts->active_idx >= g_ts->count) return 0;
    window_t *win = g_ts->windows[g_ts->active_idx].window;
    ts_hide();
    if (win) wm_focus(win);
    return 1;
}

int ts_handle_touch(int x, int y, int action) {
    if (!g_ts || !g_ts->visible) return 0;
    if (action == TOUCH_DOWN) {
        int cols = g_ts->count < 5 ? g_ts->count : (g_ts->count < 9 ? 4 : 5);
        int spacing = 32;
        int tw = TS_THUMB_WIDTH + spacing;
        int start_x = g_ts->overlay_x + (g_ts->overlay_w - cols * tw) / 2 + spacing / 2;
        int ty = g_ts->overlay_y + 60;
        for (int i = 0; i < g_ts->count; i++) {
            int col = i % cols;
            int tx = start_x + col * tw;
            if (x >= tx && x <= tx + TS_THUMB_WIDTH && y >= ty && y <= ty + TS_THUMB_HEIGHT) {
                g_ts->active_idx = i;
                g_ts->windows[i].focused = 1;
                for (int j = 0; j < g_ts->count; j++)
                    if (j != i) g_ts->windows[j].focused = 0;
                return 1;
            }
        }
    }
    return 0;
}

void ts_render(void) {
    if (!g_ts || !g_ts->visible) return;
}

void ts_update(void) {
    if (!g_ts || !g_ts->visible) return;
    if (g_ts->switching) g_ts->switch_timer++;
    if (g_ts->switch_timer > 300) { g_ts->switching = 0; g_ts->switch_timer = 0; }
}

task_switcher_t *ts_get_instance(void) { return g_ts; }

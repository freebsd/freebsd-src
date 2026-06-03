/*
 * Virtual Desktop Manager Implementation
 */

#include "workspace.h"
#include "../ui/framebuffer.h"
#include <stdlib.h>
#include <string.h>

static workspace_mgr_t *g_ws = NULL;

int ws_init(int count) {
    if (g_ws) return 0;
    if (count > WS_MAX_COUNT) count = WS_MAX_COUNT;
    g_ws = calloc(1, sizeof(workspace_mgr_t));
    if (!g_ws) return -1;
    g_ws->count = count;
    g_ws->current_idx = 0;
    g_ws->initialized = 1;
    for (int i = 0; i < count; i++) {
        snprintf(g_ws->workspaces[i].name, WS_NAME_MAX, "Desktop %d", i + 1);
        g_ws->workspaces[i].id = i;
        g_ws->workspaces[i].layout = WS_LAYOUT_TILED;
        g_ws->workspaces[i].window_count = 0;
    }
    return 0;
}

void ws_shutdown(void) {
    if (!g_ws) return;
    free(g_ws);
    g_ws = NULL;
}

int ws_set_current(int idx) {
    if (!g_ws || idx < 0 || idx >= g_ws->count) return -1;
    g_ws->current_idx = idx;
    return 0;
}

int ws_get_current(void) {
    return g_ws ? g_ws->current_idx : -1;
}

workspace_t *ws_get_workspace(int idx) {
    if (!g_ws || idx < 0 || idx >= g_ws->count) return NULL;
    return &g_ws->workspaces[idx];
}

int ws_add_window(window_t *window, int ws_idx) {
    if (!g_ws || !window || ws_idx < 0 || ws_idx >= g_ws->count) return -1;
    workspace_t *ws = &g_ws->workspaces[ws_idx];
    if (ws->window_count >= MAX_WINDOWS) return -1;
    ws->windows[ws->window_count++] = window;
    return 0;
}

int ws_remove_window(window_t *window, int ws_idx) {
    if (!g_ws || !window || ws_idx < 0 || ws_idx >= g_ws->count) return -1;
    workspace_t *ws = &g_ws->workspaces[ws_idx];
    for (int i = 0; i < ws->window_count; i++) {
        if (ws->windows[i] == window) {
            memmove(&ws->windows[i], &ws->windows[i + 1], (ws->window_count - i - 1) * sizeof(window_t *));
            ws->window_count--;
            return 0;
        }
    }
    return -1;
}

int ws_move_window(window_t *window, int from_ws, int to_ws) {
    if (ws_remove_window(window, from_ws) != 0) return -1;
    return ws_add_window(window, to_ws);
}

void ws_switch_next(void) {
    if (!g_ws) return;
    int next = (g_ws->current_idx + 1) % g_ws->count;
    ws_set_current(next);
}

void ws_switch_prev(void) {
    if (!g_ws) return;
    int prev = (g_ws->current_idx - 1 + g_ws->count) % g_ws->count;
    ws_set_current(prev);
}

void ws_render_indicators(layer_t *layer, int screen_w, int screen_h) {
    if (!g_ws || !layer) return;
    int dot_r = 8, spacing = WS_INDICATOR_SPACING;
    int total_w = g_ws->count * (dot_r * 2 + spacing) - spacing;
    int x = (screen_w - total_w) / 2;
    int y = screen_h - 48;
    for (int i = 0; i < g_ws->count; i++) {
        uint32_t c = (i == g_ws->current_idx) ? 0xFF4A90D9 : 0xFF606080;
        for (int dy = -dot_r; dy <= dot_r; dy++)
            for (int dx = -dot_r; dx <= dot_r; dx++)
                if (dx * dx + dy * dy <= dot_r * dot_r)
                    if (x + dx >= 0 && x + dx < screen_w && y + dy >= 0 && y + dy < screen_h)
                        if (layer->surfaces && layer->surface_count > 0 && layer->surfaces[0].pixel_data)
                            layer->surfaces[0].pixel_data[(y + dy) * screen_w + (x + dx)] = c;
        x += dot_r * 2 + spacing;
    }
}

int ws_handle_touch_indicators(int x, int y) {
    if (!g_ws) return 0;
    int dot_r = 8, spacing = WS_INDICATOR_SPACING;
    int total_w = g_ws->count * (dot_r * 2 + spacing) - spacing;
    int base_x = (FB_WIDTH - total_w) / 2;
    int base_y = FB_HEIGHT - 48;
    for (int i = 0; i < g_ws->count; i++) {
        int dx = x - (base_x + i * (dot_r * 2 + spacing));
        int dy = y - base_y;
        if (dx * dx + dy * dy <= (dot_r + 4) * (dot_r + 4)) {
            ws_set_current(i);
            return 1;
        }
    }
    return 0;
}

workspace_mgr_t *ws_get_instance(void) { return g_ws; }

void ws_set_layout(int ws_idx, ws_layout_t layout) {
    if (!g_ws || ws_idx < 0 || ws_idx >= g_ws->count) return;
    g_ws->workspaces[ws_idx].layout = layout;
}

void ws_set_name(int ws_idx, const char *name) {
    if (!g_ws || ws_idx < 0 || ws_idx >= g_ws->count || !name) return;
    strncpy(g_ws->workspaces[ws_idx].name, name, WS_NAME_MAX - 1);
}

/*
 * Overview / App Switcher Implementation
 */

#include "overview.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "icon_theme.h"
#include <stdlib.h>
#include <string.h>

static overview_t *g_ov = NULL;

int overview_init(void) {
    if (g_ov) return 0;
    g_ov = calloc(1, sizeof(overview_t));
    if (!g_ov) return -1;
    g_ov->visible = 0;
    g_ov->active_idx = -1;
    g_ov->search_x = (FB_WIDTH - OVERVIEW_SEARCH_WIDTH) / 2;
    g_ov->search_y = 48;
    g_ov->ws_bar_y = g_ov->search_y + 80;
    g_ov->search_w = OVERVIEW_SEARCH_WIDTH;
    g_ov->search_h = 48;
    g_ov->workspace_count = 1;
    g_ov->active_workspace = 0;
    g_ov->grid_x = 40;
    g_ov->grid_y = g_ov->ws_bar_y + OVERVIEW_WS_INDICATOR_H + 40;
    g_ov->grid_width = FB_WIDTH - 80;
    g_ov->grid_height = FB_HEIGHT - g_ov->grid_y - 40;
    g_ov->cols = 4;
    g_ov->rows = 3;
    return 0;
}

void overview_shutdown(void) {
    if (!g_ov) return;
    free(g_ov);
    g_ov = NULL;
}

void overview_show(void) {
    if (!g_ov) return;
    g_ov->visible = 1;
    overview_refresh_windows();
}

void overview_hide(void) {
    if (!g_ov) return;
    g_ov->visible = 0;
}

void overview_render(layer_t *ui_layer) {
    if (!g_ov || !g_ov->visible || !ui_layer) return;
    if (ui_layer->surface_count == 0) comp_add_surface(ui_layer, 0, 0, FB_WIDTH, FB_HEIGHT);
    uint32_t *pixels = ui_layer->surfaces[0].pixel_data;
    if (pixels) {
        for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) pixels[i] = (0x0A << 24) | (0x0A << 16) | (0x1A << 8) | 0xCC;
        int sx = g_ov->search_x, sy = g_ov->search_y, sw = g_ov->search_w, sh = g_ov->search_h;
        for (int j = sy; j < sy + sh && j < FB_HEIGHT; j++)
            for (int i = sx; i < sx + sw && i < FB_WIDTH; i++)
                if (i >= 0 && j >= 0) pixels[j * FB_WIDTH + i] = 0xFF252540;
        fb_draw_text(pixels, sx + 16, sy + 14, g_ov->search_text[0] ? g_ov->search_text : "Search apps and files...", 0xAAAACCFF, 0xFF252540);
    }
    ui_layer->surfaces[0].dirty = 1;
}

void overview_update(void) {
}

int overview_handle_touch(int x, int y, int action) {
    if (!g_ov || !g_ov->visible) return 0;
    if (action == TOUCH_DOWN) return 1;
    return 0;
}

void overview_filter(const char *text) {
    if (!g_ov || !text) return;
    strncpy(g_ov->search_text, text, sizeof(g_ov->search_text) - 1);
}

void overview_close_window(int idx) {
    if (!g_ov || idx < 0 || idx >= g_ov->window_count) return;
    if (g_ov->windows[idx].window) memset(g_ov->windows[idx].window, 0, sizeof(window_t));
}

void overview_activate_window(int idx) {
    if (!g_ov || idx < 0 || idx >= g_ov->window_count) return;
    g_ov->windows[idx].focused = 1;
    overview_hide();
}

void overview_set_workspace(int idx) {
    if (!g_ov || idx < 0 || idx >= g_ov->workspace_count) return;
    g_ov->active_workspace = idx;
}

void overview_set_workspace_count(int count) {
    if (!g_ov || count < 1 || count > 8) return;
    g_ov->workspace_count = count;
}

void overview_refresh_windows(void) {
    if (!g_ov) return;
    g_ov->window_count = 0;
}

overview_t *overview_get_instance(void) { return g_ov; }

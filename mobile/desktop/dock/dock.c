/*
 * Dock Implementation
 */

#include "dock.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/window_manager/window_mgr.h"
#include "icon_theme.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static dock_t *g_dock = NULL;

void dock_init_colors(dock_t *d) {
    (void)d;
}

int dock_init(void) {
    if (g_dock) return 0;
    g_dock = calloc(1, sizeof(dock_t));
    if (!g_dock) return -1;
    g_dock->position = DOCK_POSITION_BOTTOM;
    g_dock->icon_size = DOCK_DEFAULT_SIZE;
    g_dock->app_count = 0;
    g_dock->visible = 1;
    g_dock->autohide = 0;
    g_dock->drag_source_idx = -1;
    g_dock->drag_target_idx = -1;
    g_dock->dragging = 0;
    g_dock->surface_layer = comp_create_layer(LAYER_OVERLAY);
    dock_update_position();
    return 0;
}

void dock_shutdown(void) {
    if (!g_dock) return;
    for (int i = 0; i < g_dock->app_count; i++) {
        dock_app_t *app = &g_dock->apps[i];
        if (app->icon) icon_unload(app->icon);
    }
    if (g_dock->bg_surface) free(g_dock->bg_surface->pixel_data);
    free(g_dock);
    g_dock = NULL;
}

int dock_add_app(const char *app_id, const char *name, int pinned) {
    if (!g_dock || g_dock->app_count >= DOCK_MAX_APPS) return -1;
    dock_app_t *app = &g_dock->apps[g_dock->app_count];
    memset(app, 0, sizeof(dock_app_t));
    strncpy(app->app_id, app_id, sizeof(app->app_id) - 1);
    strncpy(app->name, name ? name : app_id, sizeof(app->name) - 1);
    app->pinned = pinned;
    app->running = 0;
    app->icon = icon_load(app_id, g_dock->icon_size, ICON_TYPE_APP);
    if (!app->icon) app->icon = icon_theme_get_default(ICON_TYPE_APP, g_dock->icon_size);
    g_dock->app_count++;
    return 0;
}

int dock_remove_app(const char *app_id) {
    if (!g_dock) return -1;
    for (int i = 0; i < g_dock->app_count; i++) {
        if (strcmp(g_dock->apps[i].app_id, app_id) == 0) {
            if (g_dock->apps[i].icon) icon_unload(g_dock->apps[i].icon);
            memmove(&g_dock->apps[i], &g_dock->apps[i + 1], (g_dock->app_count - i - 1) * sizeof(dock_app_t));
            g_dock->app_count--;
            return 0;
        }
    }
    return -1;
}

int dock_set_autohide(int enabled, uint32_t delay, int size) {
    if (!g_dock) return -1;
    g_dock->autohide = enabled;
    if (delay) g_dock->autohide_delay = delay;
    if (size > 0) g_dock->autohide_size = size;
    return 0;
}

int dock_set_position(dock_position_t position) {
    if (!g_dock) return -1;
    g_dock->position = position;
    dock_update_position();
    return 0;
}

int dock_set_size(int px) {
    if (!g_dock) return -1;
    if (px < 48 || px > 96) return -1;
    g_dock->icon_size = px;
    for (int i = 0; i < g_dock->app_count; i++) {
        if (g_dock->apps[i].icon) icon_unload(g_dock->apps[i].icon);
        g_dock->apps[i].icon = icon_load(g_dock->apps[i].app_id, px, ICON_TYPE_APP);
        if (!g_dock->apps[i].icon) g_dock->apps[i].icon = icon_theme_get_default(ICON_TYPE_APP, px);
    }
    dock_update_position();
    return 0;
}

void dock_set_running(const char *app_id, int running) {
    if (!g_dock) return;
    for (int i = 0; i < g_dock->app_count; i++) {
        if (strcmp(g_dock->apps[i].app_id, app_id) == 0) {
            g_dock->apps[i].running = running;
            break;
        }
    }
}

void dock_set_progress(const char *app_id, float progress) {
    if (!g_dock) return;
    for (int i = 0; i < g_dock->app_count; i++) {
        if (strcmp(g_dock->apps[i].app_id, app_id) == 0) {
            g_dock->apps[i].progress = progress;
            g_dock->apps[i].show_progress = 1;
            break;
        }
    }
}

void dock_update_position(void) {
    if (!g_dock) return;
    int screen_w = FB_WIDTH;
    int screen_h = FB_HEIGHT;
    int icon_size = g_dock->icon_size;
    int padding = 8;
    int app_count = g_dock->app_count;
    if (app_count == 0) app_count = 1;
    if (g_dock->position == DOCK_POSITION_BOTTOM) {
        g_dock->x = (screen_w - (app_count * (icon_size + padding * 2))) / 2;
        if (g_dock->x < 16) g_dock->x = 16;
        g_dock->width = app_count * (icon_size + padding * 2);
        g_dock->height = icon_size + padding * 2 + (g_dock->autohide ? g_dock->autohide_size : 16);
        g_dock->y = screen_h - g_dock->height - 16;
    } else if (g_dock->position == DOCK_POSITION_LEFT) {
        g_dock->y = (screen_h - g_dock->width) / 2;
        g_dock->x = 16;
        g_dock->width = icon_size + padding;
        g_dock->height = app_count * (icon_size + padding * 2);
    } else {
        g_dock->y = (screen_h - g_dock->width) / 2;
        g_dock->x = screen_w - icon_size - padding - 16;
        g_dock->width = icon_size + padding;
        g_dock->height = app_count * (icon_size + padding * 2);
    }
}

void dock_render(void) {
    if (!g_dock || !g_dock->visible || !g_dock->surface_layer) return;
    if (!g_dock->bg_surface) {
        g_dock->bg_surface = comp_add_surface(g_dock->surface_layer, g_dock->x, g_dock->y, g_dock->width, g_dock->height);
        if (!g_dock->bg_surface) return;
        int size = g_dock->width * g_dock->height;
        for (int i = 0; i < size; i++) {
            int x = i % g_dock->width, y = i / g_dock->width;
            int a = 160 - (y * 40 / g_dock->height);
            g_dock->bg_surface->pixel_data[i] = (0x0A << 24) | (0x0A << 16) | (0x15 << 8) | a;
        }
    }
    g_dock->bg_surface->dirty = 1;
}

int dock_handle_touch(int x, int y, int action) {
    if (!g_dock || !g_dock->visible) return 0;
    if (action == TOUCH_DOWN) {
        if (x >= g_dock->x && x <= g_dock->x + g_dock->width &&
            y >= g_dock->y && y <= g_dock->y + g_dock->height) {
            g_dock->dragging = 1;
            g_dock->drag_x = x;
            g_dock->drag_y = y;
            return 1;
        }
    }
    if (action == TOUCH_UP && g_dock->dragging) {
        g_dock->dragging = 0;
        g_dock->drag_source_idx = -1;
        return 1;
    }
    return 0;
}

int dock_handle_click(int x, int y) {
    if (!g_dock || !g_dock->visible) return 0;
    int icon_pad = 8;
    int icon_size = g_dock->icon_size;
    for (int i = 0; i < g_dock->app_count; i++) {
        int ax = g_dock->x + i * (icon_size + icon_pad * 2) + icon_pad;
        int ay = g_dock->y + icon_pad;
        if (x >= ax && x <= ax + icon_size && y >= ay && y <= ay + icon_size) {
            return i;
        }
    }
    return -1;
}

int dock_handle_right_click(int x, int y) {
    int idx = dock_handle_click(x, y);
    if (idx >= 0 && idx < g_dock->app_count && g_dock->apps[idx].on_right_click) {
        g_dock->apps[idx].on_right_click(g_dock->apps[idx].app_id, x, y);
        return 1;
    }
    return 0;
}

dock_t *dock_get_instance(void) { return g_dock; }

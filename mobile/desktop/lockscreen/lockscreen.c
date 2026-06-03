/*
 * Lockscreen Implementation
 */

#include "lockscreen.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/input.h"
#include "icon_theme.h"
#include "wallpaper.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static lockscreen_t *g_ls = NULL;

void lockscreen_draw_rounded_bg(uint32_t *pixels, int x, int y, int w, int h, uint32_t color, int radius) {
    if (radius <= 0) {
        for (int j = y; j < y + h && j < FB_HEIGHT; j++)
            for (int i = x; i < x + w && i < FB_WIDTH; i++)
                if (i >= 0 && j >= 0) pixels[j * FB_WIDTH + i] = color;
        return;
    }
    for (int j = y; j < y + h && j < FB_HEIGHT; j++) {
        for (int i = x; i < x + w && i < FB_WIDTH; i++) {
            if (i < 0 || j < 0) continue;
            int cx = (i < x + radius) ? (x + radius) : (i >= x + w - radius ? x + w - radius - 1 : i);
            int cy = (j < y + radius) ? (y + radius) : (j >= y + h - radius ? y + h - radius - 1 : j);
            int dx = i - cx, dy = j - cy;
            if (dx * dx + dy * dy <= radius * radius) pixels[j * FB_WIDTH + i] = color;
        }
    }
}

int lockscreen_init(void) {
    if (g_ls) return 0;
    g_ls = calloc(1, sizeof(lockscreen_t));
    if (!g_ls) return -1;
    g_ls->locked = 1;
    g_ls->visible = 0;
    g_ls->unlock_method = LOCK_METHOD_SWIPE;
    g_ls->flashlight_on = 0;
    g_ls->camera_shortcut = 1;
    g_ls->emergency_mode = 0;
    strcpy(g_ls->emergency_number, "112");
    return 0;
}

void lockscreen_shutdown(void) {
    if (!g_ls) return;
    free(g_ls);
    g_ls = NULL;
}

void lockscreen_show(void) {
    if (!g_ls) return;
    g_ls->visible = 1;
    g_ls->locked = 1;
    g_ls->unlock_progress = 0;
}

void lockscreen_hide(void) {
    if (!g_ls) return;
    g_ls->visible = 0;
    g_ls->locked = 0;
}

void lockscreen_render(layer_t *ui_layer) {
    if (!g_ls || !g_ls->visible || !ui_layer) return;
    if (ui_layer->surface_count == 0) comp_add_surface(ui_layer, 0, 0, FB_WIDTH, FB_HEIGHT);
    if (ui_layer->surfaces[0].pixel_data) {
        for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) ui_layer->surfaces[0].pixel_data[i] = 0x00000000;
    }
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char time_str[32], date_str[64];
    if (tm) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm->tm_hour, tm->tm_min);
        snprintf(date_str, sizeof(date_str), "%a, %b %d", tm->tm_wday ? tm->tm_wday : 0, tm->tm_mon + 1, tm->tm_mday);
    } else {
        strcpy(time_str, "00:00");
        strcpy(date_str, "");
    }
    int cx = FB_WIDTH / 2, cy = FB_HEIGHT / 2 - 40;
    fb_draw_text(ui_layer->surfaces[0].pixel_data, cx - 60, cy, time_str, 0xFFFFFFFF, 0x00000000);
    fb_draw_text(ui_layer->surfaces[0].pixel_data, cx - 40, cy + 60, date_str, 0xAABBCCFF, 0x00000000);
    int hx = FB_WIDTH - 80, hy = FB_HEIGHT - 120;
    icon_handle_t *cam = icon_load("camera", 48, ICON_TYPE_ACTION);
    if (cam) {
        for (int j = 0; j < 48; j++)
            for (int i = 0; i < 48; i++)
                if (cam->pixels[j * 48 + i] != 0 && hx + i < FB_WIDTH && hy + j < FB_HEIGHT)
                    ui_layer->surfaces[0].pixel_data[(hy + j) * FB_WIDTH + (hx + i)] = 0xFFFFFFFF;
    }
    int fx = FB_WIDTH - 160, fy = FB_HEIGHT - 120;
    icon_handle_t *flash = icon_load("flashlight", 48, ICON_TYPE_ACTION);
    if (flash) {
        for (int j = 0; j < 48; j++)
            for (int i = 0; i < 48; i++)
                if (flash->pixels[j * 48 + i] != 0 && fx + i < FB_WIDTH && fy + j < FB_HEIGHT)
                    ui_layer->surfaces[0].pixel_data[(fy + j) * FB_WIDTH + (fx + i)] = g_ls->flashlight_on ? 0xFFFFFF55 : 0xFFFFFFFF;
    }
    ui_layer->surfaces[0].dirty = 1;
}

void lockscreen_update(void) {
    if (!g_ls || !g_ls->visible) return;
}

int lockscreen_handle_touch(int x, int y, int action) {
    if (!g_ls || !g_ls->visible) return 0;
    if (action == TOUCH_DOWN && y >= FB_HEIGHT - 200) {
        if (y >= FB_HEIGHT - 120 && x >= FB_WIDTH - 160 && x < FB_WIDTH - 80) {
            lockscreen_toggle_flashlight();
            return 1;
        }
        if (y >= FB_HEIGHT - 120 && x >= FB_WIDTH - 80) { lockscreen_hide(); return 1; }
    }
    return 0;
}

void lockscreen_set_unlock_method(unlock_method_t method) {
    if (!g_ls) return;
    g_ls->unlock_method = method;
}

void lockscreen_set_camera_shortcut(int enabled) {
    if (!g_ls) return;
    g_ls->camera_shortcut = enabled;
}

void lockscreen_toggle_flashlight(void) {
    if (!g_ls) return;
    g_ls->flashlight_on = !g_ls->flashlight_on;
}

void lockscreen_set_emergency_number(const char *number) {
    if (!g_ls || !number) return;
    strncpy(g_ls->emergency_number, number, sizeof(g_ls->emergency_number) - 1);
}

lockscreen_t *lockscreen_get_instance(void) { return g_ls; }

/*
 * Panel Implementation
 */

#include "panel.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "icon_theme.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static panel_t *g_panel = NULL;

static void panel_draw_rounded_rect(uint32_t *pixels, int x, int y, int w, int h, uint32_t color, int radius) {
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

int panel_init(void) {
    if (g_panel) return 0;
    g_panel = calloc(1, sizeof(panel_t));
    if (!g_panel) return -1;
    g_panel->width = FB_WIDTH;
    g_panel->height = PANEL_HEIGHT;
    g_panel->x = 0;
    g_panel->y = 0;
    g_panel->visible = 1;
    g_panel->glass_effect = 1;
    g_panel->applet_count = 0;
    g_panel->bg_color = 0x0A0A1A;
    g_panel->menu_item_count = 0;
    strcpy(g_panel->active_app, "Home");
    return 0;
}

void panel_shutdown(void) {
    if (!g_panel) return;
    free(g_panel);
    g_panel = NULL;
}

int panel_add_applet(applet_type_t type) {
    if (!g_panel || g_panel->applet_count >= PANEL_MAX_APPLETS) return -1;
    panel_applet_t *applet = &g_panel->applets[g_panel->applet_count];
    memset(applet, 0, sizeof(panel_applet_t));
    applet->type = type;
    applet->active = 1;
    applet->width = 48;
    applet->height = PANEL_HEIGHT;
    g_panel->applet_count++;
    return g_panel->applet_count - 1;
}

void panel_remove_applet(int idx) {
    if (!g_panel || idx < 0 || idx >= g_panel->applet_count) return;
    memmove(&g_panel->applets[idx], &g_panel->applets[idx + 1], (g_panel->applet_count - idx - 1) * sizeof(panel_applet_t));
    g_panel->applet_count--;
}

void panel_render(void) {
    if (!g_panel || !g_panel->visible) return;
    if (!g_panel->bg_surface) {
        g_panel->bg_surface = comp_add_surface(comp_get_layer(LAYER_OVERLAY), 0, 0, FB_WIDTH, PANEL_HEIGHT);
        if (!g_panel->bg_surface) return;
        for (int i = 0; i < FB_WIDTH * PANEL_HEIGHT; i++) {
            g_panel->bg_surface->pixel_data[i] = g_panel->bg_color;
        }
        int cx = g_panel->width / 2;
        panel_draw_rounded_rect(g_panel->bg_surface->pixel_data, 16, 2, g_panel->width - 32, PANEL_HEIGHT - 4, 0x1A1A2E, 12);
    }
    int cx = 0;
    if (g_panel->menu_item_count > 0 && g_panel->menu_x > 0) {
        for (int i = 0; i < g_panel->menu_item_count; i++) {
            fb_draw_text(g_panel->bg_surface->pixel_data, cx + 8, 14, g_panel->active_app, 0xFFFFFFFF, 0x00000000);
            cx += 80;
        }
    }
    for (int i = 0; i < g_panel->applet_count; i++) {
        panel_applet_t *applet = &g_panel->applets[i];
        applet->x = g_panel->width - (g_panel->applet_count - i) * applet->width;
        applet->y = 0;
        if (applet->label[0]) {
            fb_draw_text(g_panel->bg_surface->pixel_data, applet->x + 4, 14, applet->label, 0xFFFFFFFF, 0x00000000);
        }
    }
    if (g_panel->search_focused || g_panel->search_text[0]) {
        int sx = g_panel->width / 2 - 120;
        panel_draw_rounded_rect(g_panel->bg_surface->pixel_data, sx, 6, 240, 36, 0x252540, 14);
        fb_draw_text(g_panel->bg_surface->pixel_data, sx + 12, 20, g_panel->search_text[0] ? g_panel->search_text : "Search...", 0xAABBCCDD, 0x00000000);
    }
    g_panel->bg_surface->dirty = 1;
}

void panel_update(void) {
    if (!g_panel) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm && g_panel->applet_count > 0) {
        panel_applet_t *clk = &g_panel->applets[0];
        if (clk->type == APPLET_CLOCK) {
            snprintf(clk->label, sizeof(clk->label), "%02d:%02d", tm->tm_hour, tm->tm_min);
        }
    }
}

int panel_handle_touch(int x, int y, int action) {
    if (!g_panel || !g_panel->visible) return 0;
    if (action == TOUCH_DOWN && y <= g_panel->height) {
        if (g_panel->search_focused) return 1;
        for (int i = 0; i < g_panel->applet_count; i++) {
            panel_applet_t *a = &g_panel->applets[i];
            if (x >= a->x && x <= a->x + a->width && y >= a->y && y <= a->y + a->height) {
                if (a->on_click) a->on_click();
                return 1;
            }
        }
        return 1;
    }
    return 0;
}

void panel_set_search_text(const char *text) {
    if (!g_panel) return;
    strncpy(g_panel->search_text, text ? text : "", sizeof(g_panel->search_text) - 1);
}

void panel_set_active_app(const char *app_name) {
    if (!g_panel || !app_name) return;
    strncpy(g_panel->active_app, app_name, sizeof(g_panel->active_app) - 1);
}

void panel_add_menu_item(const char *item) {
    if (!g_panel || g_panel->menu_item_count >= PANEL_MAX_MENU_ITEMS) return;
    strncpy(g_panel->menu_items[g_panel->menu_item_count], item, sizeof(g_panel->menu_items[0]) - 1);
    g_panel->menu_item_count++;
}

void panel_set_glass_effect(int enabled) {
    if (!g_panel) return;
    g_panel->glass_effect = enabled;
}

panel_t *panel_get_instance(void) { return g_panel; }

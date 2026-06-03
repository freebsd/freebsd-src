/*
 * System UI Implementation
 * BSD-licensed
 */

#include "systemui.h"
#include <stdlib.h>
#include <string.h>

static systemui_t g_systemui;

int systemui_init(void) {
    memset(&g_systemui, 0, sizeof(g_systemui));
    
    g_systemui.statusbar = widget_statusbar_create(0, 0, FB_WIDTH, STATUSBAR_HEIGHT);
    g_systemui.navbar = widget_navbar_create(0, FB_HEIGHT - NAVBAR_HEIGHT, FB_WIDTH, NAVBAR_HEIGHT);
    
    g_systemui.light_mode = 0;
    g_systemui.visible = 1;
    
    return 0;
}

void systemui_shutdown(void) {
    if (g_systemui.statusbar) {
        free(g_systemui.statusbar);
    }
    if (g_systemui.navbar) {
        free(g_systemui.navbar);
    }
}

void systemui_set_light_mode(int enabled) {
    g_systemui.light_mode = enabled;
    if (g_systemui.statusbar) {
        g_systemui.statusbar->base.bg_color = enabled ? COLOR_LIGHT_BG : COLOR_DARK_BG;
    }
    if (g_systemui.navbar) {
        g_systemui.navbar->base.bg_color = enabled ? COLOR_LIGHT_BG : 0xFF0f3460;
    }
}

void systemui_set_palette(const palette_t *p) {
    if (p) {
        g_systemui.palette = (palette_t *)p;
    }
}

void systemui_update(void) {
}

void systemui_render(void) {
    if (!g_systemui.visible) return;
    
    if (g_systemui.statusbar) {
        widget_statusbar_draw(g_systemui.statusbar);
    }
    
    if (g_systemui.navbar) {
        widget_navbar_draw(g_systemui.navbar);
    }
}

void systemui_set_battery(int level) {
    if (g_systemui.statusbar) {
        widget_statusbar_set_battery(g_systemui.statusbar, level);
    }
}

void systemui_set_time(const char *time) {
    if (g_systemui.statusbar && time) {
        widget_statusbar_set_time(g_systemui.statusbar, time);
    }
}

void systemui_set_wifi(int enabled) {
    if (g_systemui.statusbar) {
        widget_statusbar_set_wifi(g_systemui.statusbar, enabled);
    }
}

void systemui_set_signal(int strength) {
    (void)strength;
}
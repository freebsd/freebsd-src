/*
 * Theme Engine Implementation
 */

#include "theme.h"
#include <stdlib.h>
#include <string.h>

static theme_engine_t *g_theme_engine = NULL;

static void theme_set_defaults(theme_t *t, int dark) {
    if (dark) {
        t->colors[BG_PRIMARY] = (color_t){0x0A, 0x0A, 0x14, 0xFF};
        t->colors[BG_SECONDARY] = (color_t){0x14, 0x14, 0x22, 0xFF};
        t->colors[BG_CARD] = (color_t){0x1E, 0x1E, 0x2E, 0xFF};
        t->colors[BG_OVERLAY] = (color_t){0x28, 0x28, 0x3A, 0xFF};
        t->colors[BG_SURFACE] = (color_t){0x18, 0x18, 0x26, 0xFF};
        t->colors[BG_DOCK] = (color_t){0x1A, 0x1A, 0x2A, 0xFF};
        t->colors[BG_PANEL] = (color_t){0x0D, 0x0D, 0x18, 0xFF};
        t->colors[ACCENT_PRIMARY] = (color_t){0x4A, 0x90, 0xD9, 0xFF};
        t->colors[ACCENT_SECONDARY] = (color_t){0x50, 0xC8, 0x78, 0xFF};
        t->colors[TEXT_PRIMARY] = (color_t){0xFF, 0xFF, 0xFF, 0xFF};
        t->colors[TEXT_SECONDARY] = (color_t){0xAA, 0xAA, 0xCC, 0xFF};
        t->colors[TEXT_TERTIARY] = (color_t){0x66, 0x66, 0x88, 0xFF};
        t->colors[TEXT_ON_ACCENT] = (color_t){0xFF, 0xFF, 0xFF, 0xFF};
        t->colors[BORDER] = (color_t){0x30, 0x30, 0x48, 0xFF};
        t->colors[ERROR] = (color_t){0xFF, 0x6B, 0x6B, 0xFF};
        t->colors[SUCCESS] = (color_t){0x50, 0xC8, 0x78, 0xFF};
        t->colors[WARNING] = (color_t){0xFF, 0xA5, 0x00, 0xFF};
        t->colors[CRITICAL] = (color_t){0xFF, 0x3A, 0x1A, 0xFF};
    } else {
        t->colors[BG_PRIMARY] = (color_t){0xF5, 0xF5, 0xF7, 0xFF};
        t->colors[BG_SECONDARY] = (color_t){0xEB, 0xEB, 0xF0, 0xFF};
        t->colors[BG_CARD] = (color_t){0xFF, 0xFF, 0xFF, 0xFF};
        t->colors[BG_OVERLAY] = (color_t){0xFF, 0xFF, 0xFF, 0xFF};
        t->colors[BG_SURFACE] = (color_t){0xF0, 0xF0, 0xF5, 0xFF};
        t->colors[BG_DOCK] = (color_t){0xF8, 0xF8, 0xFC, 0xFF};
        t->colors[BG_PANEL] = (color_t){0xF0, 0xF0, 0xF5, 0xE0};
        t->colors[ACCENT_PRIMARY] = (color_t){0x3A, 0x70, 0xC0, 0xFF};
        t->colors[ACCENT_SECONDARY] = (color_t){0x30, 0xA0, 0x60, 0xFF};
        t->colors[TEXT_PRIMARY] = (color_t){0x1A, 0x1A, 0x2E, 0xFF};
        t->colors[TEXT_SECONDARY] = (color_t){0x55, 0x55, 0x77, 0xFF};
        t->colors[TEXT_TERTIARY] = (color_t){0x88, 0x88, 0x99, 0xFF};
        t->colors[TEXT_ON_ACCENT] = (color_t){0xFF, 0xFF, 0xFF, 0xFF};
        t->colors[BORDER] = (color_t){0xD0, 0xD0, 0xDD, 0xFF};
        t->colors[ERROR] = (color_t){0xD0, 0x30, 0x30, 0xFF};
        t->colors[SUCCESS] = (color_t){0x30, 0xA0, 0x50, 0xFF};
        t->colors[WARNING] = (color_t){0xE0, 0x80, 0x00, 0xFF};
        t->colors[CRITICAL] = (color_t){0xC0, 0x20, 0x10, 0xFF};
    }
    t->radius_small = (radius_t){4, 4, 4, 4};
    t->radius_medium = (radius_t){8, 8, 8, 8};
    t->radius_large = (radius_t){16, 16, 16, 16};
    t->radius_full = (radius_t){999, 999, 999, 999};
    t->shadow_small = (shadow_t){0, 2, 4, 0, (color_t){0x00, 0x00, 0x00, 0x30}};
    t->shadow_medium = (shadow_t){0, 4, 8, 0, (color_t){0x00, 0x00, 0x00, 0x40}};
    t->shadow_large = (shadow_t){0, 8, 16, 0, (color_t){0x00, 0x00, 0x00, 0x50}};
    t->panel_height = 48;
    t->dock_size = 72;
    t->icon_size = 32;
    t->animation_duration = 300;
    t->bubble_radius = 12;
    strcpy(t->name, dark ? "dark" : "light");
}

int theme_init(const char *theme_name) {
    if (!g_theme_engine) {
        g_theme_engine = calloc(1, sizeof(theme_engine_t));
        if (!g_theme_engine) return -1;
        theme_set_defaults(&g_theme_engine->dark, 1);
        theme_set_defaults(&g_theme_engine->light, 0);
    }
    int dark = (theme_name && strstr(theme_name, "dark"));
    theme_set_defaults(&g_theme_engine->current, dark);
    strncpy(g_theme_engine->current.name, theme_name ? theme_name : "default", THEME_NAME_MAX - 1);
    g_theme_engine->loaded = 1;
    return 0;
}

void theme_shutdown(void) {
    if (!g_theme_engine) return;
    free(g_theme_engine);
    g_theme_engine = NULL;
}

int theme_load(const char *theme_name) {
    return theme_init(theme_name);
}

void theme_apply(theme_t *theme) {
    if (!g_theme_engine || !theme) return;
    memcpy(&g_theme_engine->current, theme, sizeof(theme_t));
}

color_t theme_get_color(theme_role_t role) {
    if (!g_theme_engine) return (color_t){0, 0, 0, 0};
    if (role < 0 || role >= ROLE_COUNT) return (color_t){0, 0, 0, 0};
    return g_theme_engine->current.colors[role];
}

font_t theme_get_font(int font_role) {
    if (!g_theme_engine) return (font_t){"sans", 14, 400, 20};
    return g_theme_engine->current.fonts[font_role];
}

int theme_get_radius(int radius_role, radius_t *out_radius) {
    if (!g_theme_engine || !out_radius) return -1;
    switch (radius_role) {
        case 0: *out_radius = g_theme_engine->current.radius_small; break;
        case 1: *out_radius = g_theme_engine->current.radius_medium; break;
        case 2: *out_radius = g_theme_engine->current.radius_large; break;
        case 3: *out_radius = g_theme_engine->current.radius_full; break;
        default: return -1;
    }
    return 0;
}

shadow_t theme_get_shadow(int shadow_role) {
    shadow_t def = (shadow_t){0, 2, 4, 0, (color_t){0, 0, 0, 0x40}};
    if (!g_theme_engine) return def;
    switch (shadow_role) {
        case 0: return g_theme_engine->current.shadow_small;
        case 1: return g_theme_engine->current.shadow_medium;
        case 2: return g_theme_engine->current.shadow_large;
    }
    return def;
}

theme_t *theme_get_current(void) {
    if (!g_theme_engine) return NULL;
    return &g_theme_engine->current;
}

theme_engine_t *theme_get_engine(void) { return g_theme_engine; }

int theme_parse_stylesheet(const char *path) {
    (void)path;
    return 0;
}

void theme_set_dark_mode(int dark) {
    if (!g_theme_engine) return;
    theme_set_defaults(&g_theme_engine->current, dark);
}

int theme_get_dark_mode(void) {
    if (!g_theme_engine) return 0;
    return g_theme_engine->current.colors[BG_PRIMARY].r < 0x80;
}

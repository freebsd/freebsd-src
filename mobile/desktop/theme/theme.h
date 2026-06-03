/*
 * Theme Engine for uOS(m) Desktop
 * Dark/light themes with CSS-like stylesheet parser
 */

#ifndef _THEME_H_
#define _THEME_H_

#include <stdint.h>
#include "../ui/framebuffer.h"

#define THEME_MAX_COLORS   16
#define THEME_NAME_MAX     64
#define THEME_FILE_MAX     4096

typedef enum {
    BG_PRIMARY,
    BG_SECONDARY,
    BG_CARD,
    BG_OVERLAY,
    BG_SURFACE,
    BG_DOCK,
    BG_PANEL,
    ACCENT_PRIMARY,
    ACCENT_SECONDARY,
    TEXT_PRIMARY,
    TEXT_SECONDARY,
    TEXT_TERTIARY,
    TEXT_ON_ACCENT,
    BORDER,
    ERROR,
    SUCCESS,
    WARNING,
    CRITICAL,
    ROLE_COUNT
} theme_role_t;

typedef struct {
    uint8_t r, g, b, a;
} color_t;

typedef struct {
    char family[64];
    int size;
    int weight;
    int line_height;
} font_t;

typedef struct {
    int top, right, bottom, left;
} radius_t;

typedef struct {
    int offset_x, offset_y;
    int blur_radius;
    int spread;
    color_t color;
} shadow_t;

typedef struct {
    char name[THEME_NAME_MAX];
    color_t colors[ROLE_COUNT];
    font_t fonts[4];
    radius_t radius_small;
    radius_t radius_medium;
    radius_t radius_large;
    radius_t radius_full;
    shadow_t shadow_small;
    shadow_t shadow_medium;
    shadow_t shadow_large;
    int panel_height;
    int dock_size;
    int icon_size;
    int animation_duration;
    int bubble_radius;
} theme_t;

typedef struct {
    theme_t current;
    theme_t dark;
    theme_t light;
    int loaded;
    char theme_path[256];
} theme_engine_t;

int theme_init(const char *theme_name);
void theme_shutdown(void);
int theme_load(const char *theme_name);
void theme_apply(theme_t *theme);
color_t theme_get_color(theme_role_t role);
font_t theme_get_font(int font_role);
int theme_get_radius(int radius_role, radius_t *out_radius);
shadow_t theme_get_shadow(int shadow_role);
theme_t *theme_get_current(void);
theme_engine_t *theme_get_engine(void);
int theme_parse_stylesheet(const char *path);
void theme_set_dark_mode(int dark);
int theme_get_dark_mode(void);

#endif /* _THEME_H_ */

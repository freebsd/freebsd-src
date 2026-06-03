/*
 * Common UI Widgets
 * BSD-licensed
 */

#ifndef _WIDGETS_H_
#define _WIDGETS_H_

#include <stdint.h>
#include "../framebuffer.h"

#define WIDGET_BG_DARK    0xFF1a1a2e
#define WIDGET_BG_CARD    0xFF16213e
#define WIDGET_ACCENT     0xFF0f3460
#define WIDGET_TEXT       0xFFe0e0e0

typedef void (*widget_click_cb_t)(void *user_data);

typedef struct widget {
    int id;
    int x, y;
    int width, height;
    uint32_t bg_color;
    uint32_t fg_color;
    char *text;
    int visible;
    int enabled;
    
    widget_click_cb_t on_click;
    void *user_data;
    
    struct widget *next;
} widget_t;

typedef struct {
    widget_t base;
    int pressed;
} button_t;

typedef struct {
    widget_t base;
} label_t;

typedef struct {
    widget_t base;
    int cursor_pos;
    int text_len;
    char *buffer;
    int max_len;
} textinput_t;

typedef struct {
    widget_t base;
    int scroll_x, scroll_y;
    int content_width, content_height;
    widget_t *children;
} scrollview_t;

typedef struct {
    widget_t base;
    int battery_level;
    int signal_strength;
    int wifi_enabled;
    char time_str[16];
} statusbar_t;

typedef struct {
    widget_t base;
} navbar_t;

/* Button */
button_t *widget_button_create(int x, int y, int width, int height, const char *text);
void widget_button_draw(button_t *btn);
int widget_button_hit_test(button_t *btn, int px, int py);

/* Label */
label_t *widget_label_create(int x, int y, int width, int height, const char *text);
void widget_label_draw(label_t *lbl);
int widget_label_hit_test(label_t *lbl, int px, int py);

/* TextInput */
textinput_t *widget_textinput_create(int x, int y, int width, int height, const char *placeholder);
void widget_textinput_draw(textinput_t *inp);
int widget_textinput_hit_test(textinput_t *inp, int px, int py);

/* ScrollView */
scrollview_t *widget_scrollview_create(int x, int y, int width, int height);
void widget_scrollview_draw(scrollview_t *sv);
int widget_scrollview_hit_test(scrollview_t *sv, int px, int py);

/* StatusBar */
statusbar_t *widget_statusbar_create(int x, int y, int width, int height);
void widget_statusbar_draw(statusbar_t *sb);
void widget_statusbar_set_battery(statusbar_t *sb, int level);
void widget_statusbar_set_time(statusbar_t *sb, const char *time);
void widget_statusbar_set_wifi(statusbar_t *sb, int enabled);

/* NavigationBar */
navbar_t *widget_navbar_create(int x, int y, int width, int height);
void widget_navbar_draw(navbar_t *nb);
int widget_navbar_hit_test(navbar_t *nb, int px, int py);

#endif /* _WIDGETS_H_ */
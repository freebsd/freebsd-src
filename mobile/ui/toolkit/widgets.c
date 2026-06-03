/*
 * Common UI Widgets Implementation
 * BSD-licensed
 */

#include "widgets.h"
#include <stdlib.h>
#include <string.h>

button_t *widget_button_create(int x, int y, int width, int height, const char *text) {
    button_t *btn = calloc(1, sizeof(button_t));
    if (!btn) return NULL;
    btn->base.x = x;
    btn->base.y = y;
    btn->base.width = width;
    btn->base.height = height;
    btn->base.bg_color = WIDGET_BG_CARD;
    btn->base.fg_color = WIDGET_TEXT;
    if (text) {
        btn->base.text = strdup(text);
    }
    btn->base.visible = 1;
    btn->base.enabled = 1;
    btn->pressed = 0;
    return btn;
}

void widget_button_draw(button_t *btn) {
    if (!btn || !btn->base.visible) return;
    
    uint32_t bg = btn->base.bg_color;
    if (btn->pressed) {
        bg = 0xFF0f3460;
    }
    
    fb_fill_rect(btn->base.x, btn->base.y, btn->base.width, btn->base.height, bg);
    fb_draw_rect(btn->base.x, btn->base.y, btn->base.width, btn->base.height, 0xFF3a3a5c);
    
    if (btn->base.text) {
        fb_draw_text(btn->base.x + 10, btn->base.y + btn->base.height / 2 - 5,
                     btn->base.text, btn->base.fg_color, 0);
    }
}

int widget_button_hit_test(button_t *btn, int px, int py) {
    if (!btn) return 0;
    if (px >= btn->base.x && px < btn->base.x + btn->base.width &&
        py >= btn->base.y && py < btn->base.y + btn->base.height) {
        return 1;
    }
    return 0;
}

label_t *widget_label_create(int x, int y, int width, int height, const char *text) {
    label_t *lbl = calloc(1, sizeof(label_t));
    if (!lbl) return NULL;
    lbl->base.x = x;
    lbl->base.y = y;
    lbl->base.width = width;
    lbl->base.height = height;
    lbl->base.bg_color = WIDGET_BG_DARK;
    lbl->base.fg_color = WIDGET_TEXT;
    if (text) {
        lbl->base.text = strdup(text);
    }
    lbl->base.visible = 1;
    return lbl;
}

void widget_label_draw(label_t *lbl) {
    if (!lbl || !lbl->base.visible) return;
    if (lbl->base.text) {
        fb_draw_text(lbl->base.x, lbl->base.y + lbl->base.height / 2 - 5,
                     lbl->base.text, lbl->base.fg_color, 0);
    }
}

int widget_label_hit_test(label_t *lbl, int px, int py) {
    if (!lbl) return 0;
    return px >= lbl->base.x && px < lbl->base.x + lbl->base.width &&
           py >= lbl->base.y && py < lbl->base.y + lbl->base.height;
}

textinput_t *widget_textinput_create(int x, int y, int width, int height, const char *placeholder) {
    textinput_t *inp = calloc(1, sizeof(textinput_t));
    if (!inp) return NULL;
    inp->base.x = x;
    inp->base.y = y;
    inp->base.width = width;
    inp->base.height = height;
    inp->base.bg_color = WIDGET_BG_CARD;
    inp->base.fg_color = WIDGET_TEXT;
    inp->max_len = 256;
    inp->buffer = calloc(1, inp->max_len);
    if (placeholder) {
        inp->base.text = strdup(placeholder);
    }
    return inp;
}

void widget_textinput_draw(textinput_t *inp) {
    if (!inp || !inp->base.visible) return;
    
    fb_fill_rect(inp->base.x, inp->base.y, inp->base.width, inp->base.height, inp->base.bg_color);
    fb_draw_rect(inp->base.x, inp->base.y, inp->base.width, inp->base.height, 0xFF3a3a5c);
    
    if (inp->buffer && inp->text_len > 0) {
        fb_draw_text(inp->base.x + 5, inp->base.y + inp->base.height / 2 - 5,
                     inp->buffer, inp->base.fg_color, 0);
    } else if (inp->base.text) {
        fb_draw_text(inp->base.x + 5, inp->base.y + inp->base.height / 2 - 5,
                     inp->base.text, 0xFF808080, 0);
    }
}

int widget_textinput_hit_test(textinput_t *inp, int px, int py) {
    if (!inp) return 0;
    return px >= inp->base.x && px < inp->base.x + inp->base.width &&
           py >= inp->base.y && py < inp->base.y + inp->base.height;
}

scrollview_t *widget_scrollview_create(int x, int y, int width, int height) {
    scrollview_t *sv = calloc(1, sizeof(scrollview_t));
    if (!sv) return NULL;
    sv->base.x = x;
    sv->base.y = y;
    sv->base.width = width;
    sv->base.height = height;
    sv->base.bg_color = WIDGET_BG_DARK;
    sv->scroll_x = 0;
    sv->scroll_y = 0;
    return sv;
}

void widget_scrollview_draw(scrollview_t *sv) {
    if (!sv || !sv->base.visible) return;
    fb_fill_rect(sv->base.x, sv->base.y, sv->base.width, sv->base.height, sv->base.bg_color);
    fb_draw_rect(sv->base.x, sv->base.y, sv->base.width, sv->base.height, 0xFF3a3a5c);
}

int widget_scrollview_hit_test(scrollview_t *sv, int px, int py) {
    if (!sv) return 0;
    return px >= sv->base.x && px < sv->base.x + sv->base.width &&
           py >= sv->base.y && py < sv->base.y + sv->base.height;
}

statusbar_t *widget_statusbar_create(int x, int y, int width, int height) {
    statusbar_t *sb = calloc(1, sizeof(statusbar_t));
    if (!sb) return NULL;
    sb->base.x = x;
    sb->base.y = y;
    sb->base.width = width;
    sb->base.height = height;
    sb->base.bg_color = 0xFF0f3460;
    sb->battery_level = 100;
    sb->signal_strength = 4;
    sb->wifi_enabled = 1;
    strcpy(sb->time_str, "12:00");
    sb->base.visible = 1;
    return sb;
}

void widget_statusbar_draw(statusbar_t *sb) {
    if (!sb || !sb->base.visible) return;
    
    fb_fill_rect(sb->base.x, sb->base.y, sb->base.width, sb->base.height, sb->base.bg_color);
    
    int bar_x = sb->base.x + sb->base.width - 100;
    fb_draw_text(bar_x, sb->base.y + sb->base.height / 2 - 5, sb->time_str,
               WIDGET_TEXT, 0);
    
    int batt_x = sb->base.x + sb->base.width - 40;
    int batt_level = sb->battery_level;
    int batt_width = (batt_level * 20) / 100;
    fb_fill_rect(batt_x, sb->base.y + 10, 25, 12, 0xFF3a3a5c);
    fb_fill_rect(batt_x + 2, sb->base.y + 12, batt_width, 8, 0xFFe0e0e0);
    
    if (sb->wifi_enabled) {
        fb_draw_text(sb->base.x + 10, sb->base.y + sb->base.height / 2 - 5,
                     "WIFI", WIDGET_TEXT, 0);
    }
}

void widget_statusbar_set_battery(statusbar_t *sb, int level) {
    if (sb) sb->battery_level = level;
}

void widget_statusbar_set_time(statusbar_t *sb, const char *time) {
    if (sb && time) strncpy(sb->time_str, time, sizeof(sb->time_str) - 1);
}

void widget_statusbar_set_wifi(statusbar_t *sb, int enabled) {
    if (sb) sb->wifi_enabled = enabled;
}

navbar_t *widget_navbar_create(int x, int y, int width, int height) {
    navbar_t *nb = calloc(1, sizeof(navbar_t));
    if (!nb) return NULL;
    nb->base.x = x;
    nb->base.y = y;
    nb->base.width = width;
    nb->base.height = height;
    nb->base.bg_color = 0xFF0f3460;
    nb->base.visible = 1;
    return nb;
}

void widget_navbar_draw(navbar_t *nb) {
    if (!nb || !nb->base.visible) return;
    
    fb_fill_rect(nb->base.x, nb->base.y, nb->base.width, nb->base.height, nb->base.bg_color);
    
    int btn_size = 40;
    int spacing = nb->base.width / 4;
    
    fb_fill_circle(nb->base.x + spacing - btn_size/2, nb->base.y + nb->base.height/2,
                 btn_size/2, WIDGET_ACCENT);
    
    fb_fill_circle(nb->base.x + 2*spacing - btn_size/2, nb->base.y + nb->base.height/2,
                 btn_size/2, WIDGET_ACCENT);
    
    fb_fill_circle(nb->base.x + 3*spacing - btn_size/2, nb->base.y + nb->base.height/2,
                 btn_size/2, WIDGET_ACCENT);
}

int widget_navbar_hit_test(navbar_t *nb, int px, int py) {
    if (!nb) return 0;
    return px >= nb->base.x && px < nb->base.x + nb->base.width &&
           py >= nb->base.y && py < nb->base.y + nb->base.height;
}
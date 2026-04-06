/*
 * Mobile UI Widget System
 * uOS(m) - User OS Mobile
 */

#ifndef _UI_WIDGET_H_
#define _UI_WIDGET_H_

#include <stdint.h>
#include "framebuffer.h"

#define MAX_WIDGETS 256

/* Widget types */
typedef enum {
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXTBOX,
    WIDGET_IMAGE,
    WIDGET_CONTAINER
} widget_type_t;

/* Widget states */
typedef enum {
    WIDGET_STATE_NORMAL,
    WIDGET_STATE_HOVER,
    WIDGET_STATE_PRESSED,
    WIDGET_STATE_DISABLED
} widget_state_t;

/* Touch actions */
typedef enum {
    TOUCH_DOWN,
    TOUCH_UP,
    TOUCH_MOVE
} touch_action_t;

/* Widget structure */
typedef struct widget {
    uint32_t id;
    widget_type_t type;
    widget_state_t state;

    int x, y;
    int width, height;

    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t border_color;

    char *text;
    void *image_data;

    /* Callbacks */
    void (*on_click)(struct widget *widget);
    void (*on_touch)(struct widget *widget, int x, int y, touch_action_t action);
    void (*on_draw)(struct widget *widget);

    /* Widget-specific data */
    void *user_data;

    /* Linked list */
    struct widget *next;
} widget_t;

/* Initialize widget system */
int ui_widget_init(void);

/* Create widgets */
widget_t *ui_create_button(int x, int y, int width, int height, const char *text);
widget_t *ui_create_label(int x, int y, int width, int height, const char *text);
widget_t *ui_create_textbox(int x, int y, int width, int height, const char *text);

/* Widget management */
void ui_add_widget(widget_t *widget);
void ui_remove_widget(widget_t *widget);
void ui_destroy_widget(widget_t *widget);

/* Event handling */
void ui_handle_touch(int x, int y, touch_action_t action);

/* Rendering */
void ui_render_all(void);
void ui_render_widget(widget_t *widget);

/* Widget utilities */
void ui_set_widget_position(widget_t *widget, int x, int y);
void ui_set_widget_size(widget_t *widget, int width, int height);
void ui_set_widget_colors(widget_t *widget, uint32_t bg, uint32_t fg, uint32_t border);
void ui_set_widget_text(widget_t *widget, const char *text);

#endif /* _UI_WIDGET_H_ */
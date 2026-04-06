/*
 * Mobile UI Widget Implementation
 * uOS(m) - User OS Mobile
 */

#include "ui_widget.h"
#include "framebuffer.h"
#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"

/* Simple strlen function */
static size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static widget_t *widget_list = NULL;
static uint32_t next_widget_id = 1;

/* Initialize widget system */
int ui_widget_init(void) {
    widget_list = NULL;
    next_widget_id = 1;
    return 0;
}

/* Create a new widget */
static widget_t *create_widget(widget_type_t type, int x, int y, int width, int height) {
    widget_t *widget = (widget_t *)mem_alloc(sizeof(widget_t));
    if (!widget) return NULL;

    memset(widget, 0, sizeof(widget_t));

    widget->id = next_widget_id++;
    widget->type = type;
    widget->state = WIDGET_STATE_NORMAL;
    widget->x = x;
    widget->y = y;
    widget->width = width;
    widget->height = height;
    widget->bg_color = COLOR_GRAY;
    widget->fg_color = COLOR_BLACK;
    widget->border_color = COLOR_BLACK;

    return widget;
}

/* Create button widget */
widget_t *ui_create_button(int x, int y, int width, int height, const char *text) {
    widget_t *button = create_widget(WIDGET_BUTTON, x, y, width, height);
    if (!button) return NULL;

    if (text) {
        size_t len = strlen(text) + 1;
        button->text = (char *)mem_alloc(len);
        if (button->text) {
            memcpy(button->text, text, len);
        }
    }

    button->bg_color = COLOR_BLUE;
    button->fg_color = COLOR_WHITE;

    return button;
}

/* Create label widget */
widget_t *ui_create_label(int x, int y, int width, int height, const char *text) {
    widget_t *label = create_widget(WIDGET_LABEL, x, y, width, height);
    if (!label) return NULL;

    if (text) {
        size_t len = strlen(text) + 1;
        label->text = (char *)mem_alloc(len);
        if (label->text) {
            memcpy(label->text, text, len);
        }
    }

    label->bg_color = COLOR_TRANSPARENT;
    label->fg_color = COLOR_BLACK;

    return label;
}

/* Create textbox widget */
widget_t *ui_create_textbox(int x, int y, int width, int height, const char *text) {
    widget_t *textbox = create_widget(WIDGET_TEXTBOX, x, y, width, height);
    if (!textbox) return NULL;

    if (text) {
        size_t len = strlen(text) + 1;
        textbox->text = (char *)mem_alloc(len);
        if (textbox->text) {
            memcpy(textbox->text, text, len);
        }
    }

    textbox->bg_color = COLOR_WHITE;
    textbox->fg_color = COLOR_BLACK;
    textbox->border_color = COLOR_BLACK;

    return textbox;
}

/* Add widget to list */
void ui_add_widget(widget_t *widget) {
    if (!widget) return;

    widget->next = widget_list;
    widget_list = widget;
}

/* Remove widget from list */
void ui_remove_widget(widget_t *widget) {
    if (!widget) return;

    if (widget_list == widget) {
        widget_list = widget->next;
        return;
    }

    widget_t *current = widget_list;
    while (current && current->next != widget) {
        current = current->next;
    }

    if (current) {
        current->next = widget->next;
    }
}

/* Destroy widget */
void ui_destroy_widget(widget_t *widget) {
    if (!widget) return;

    ui_remove_widget(widget);

    if (widget->text) {
        mem_free(widget->text);
    }

    if (widget->image_data) {
        mem_free(widget->image_data);
    }

    mem_free(widget);
}

/* Handle touch events */
void ui_handle_touch(int x, int y, touch_action_t action) {
    widget_t *widget = widget_list;

    while (widget) {
        if (x >= widget->x && x < widget->x + widget->width &&
            y >= widget->y && y < widget->y + widget->height) {

            // Update widget state
            if (action == TOUCH_DOWN) {
                widget->state = WIDGET_STATE_PRESSED;
            } else if (action == TOUCH_UP) {
                if (widget->state == WIDGET_STATE_PRESSED) {
                    // Click event
                    if (widget->on_click) {
                        widget->on_click(widget);
                    }
                }
                widget->state = WIDGET_STATE_NORMAL;
            }

            // Touch callback
            if (widget->on_touch) {
                widget->on_touch(widget, x, y, action);
            }

            break; // Only handle topmost widget
        }
        widget = widget->next;
    }
}

/* Render all widgets */
void ui_render_all(void) {
    fb_clear(COLOR_WHITE); // Clear screen

    widget_t *widget = widget_list;
    while (widget) {
        ui_render_widget(widget);
        widget = widget->next;
    }

    fb_flush();
}

/* Render a single widget */
void ui_render_widget(widget_t *widget) {
    if (!widget) return;

    uint32_t bg_color = widget->bg_color;
    uint32_t fg_color = widget->fg_color;

    // Adjust colors based on state
    switch (widget->state) {
        case WIDGET_STATE_PRESSED:
            bg_color = (bg_color & 0xFF000000) | ((bg_color & 0x00FFFFFF) >> 1);
            break;
        case WIDGET_STATE_HOVER:
            bg_color = (bg_color & 0xFF000000) | ((bg_color & 0x00FFFFFF) | 0x404040);
            break;
        case WIDGET_STATE_DISABLED:
            bg_color = COLOR_GRAY;
            fg_color = COLOR_BLACK;
            break;
        default:
            break;
    }

    // Draw background
    if (bg_color != COLOR_TRANSPARENT) {
        fb_fill_rect(widget->x, widget->y, widget->width, widget->height, bg_color);
    }

    // Draw border
    if (widget->border_color != COLOR_TRANSPARENT) {
        fb_draw_rect(widget->x, widget->y, widget->width, widget->height, widget->border_color);
    }

    // Draw content based on type
    switch (widget->type) {
        case WIDGET_BUTTON:
        case WIDGET_LABEL:
            if (widget->text) {
                int text_x = widget->x + (widget->width - strlen(widget->text) * 8) / 2;
                int text_y = widget->y + (widget->height - 16) / 2;
                fb_draw_text(text_x, text_y, widget->text, fg_color, COLOR_TRANSPARENT);
            }
            break;

        case WIDGET_TEXTBOX:
            if (widget->text) {
                fb_draw_text(widget->x + 4, widget->y + 4, widget->text, fg_color, COLOR_TRANSPARENT);
            }
            break;

        default:
            break;
    }

    // Custom draw callback
    if (widget->on_draw) {
        widget->on_draw(widget);
    }
}

/* Widget utilities */
void ui_set_widget_position(widget_t *widget, int x, int y) {
    if (widget) {
        widget->x = x;
        widget->y = y;
    }
}

void ui_set_widget_size(widget_t *widget, int width, int height) {
    if (widget) {
        widget->width = width;
        widget->height = height;
    }
}

void ui_set_widget_colors(widget_t *widget, uint32_t bg, uint32_t fg, uint32_t border) {
    if (widget) {
        widget->bg_color = bg;
        widget->fg_color = fg;
        widget->border_color = border;
    }
}

void ui_set_widget_text(widget_t *widget, const char *text) {
    if (!widget) return;

    if (widget->text) {
        mem_free(widget->text);
        widget->text = NULL;
    }

    if (text) {
        size_t len = strlen(text) + 1;
        widget->text = (char *)mem_alloc(len);
        if (widget->text) {
            memcpy(widget->text, text, len);
        }
    }
}
/*
 * Mobile Window Manager Implementation
 * uOS(m) - User OS Mobile
 */

#include "window_manager.h"
#include "framebuffer.h"
#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"

/* Simple strlen function */
static size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static window_t *window_list = NULL;
static window_t *focused_window = NULL;
static uint32_t next_window_id = 1;

/* Initialize window manager */
int wm_init(void) {
    window_list = NULL;
    focused_window = NULL;
    next_window_id = 1;
    return 0;
}

/* Create a new window */
window_t *wm_create_window(const char *title, int x, int y, int width, int height) {
    window_t *window = (window_t *)mem_alloc(sizeof(window_t));
    if (!window) return NULL;

    memset(window, 0, sizeof(window_t));

    window->id = next_window_id++;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->z_order = 0;
    window->bg_color = COLOR_WHITE;
    window->visible = 1;
    window->focused = 0;
    window->minimized = 0;

    if (title) {
        size_t len = strlen(title);
        if (len > sizeof(window->title) - 1) len = sizeof(window->title) - 1;
        memcpy(window->title, title, len);
        window->title[len] = '\0';
    } else {
        memcpy(window->title, "Window", 7);
    }

    // Add to window list (sorted by z-order)
    if (!window_list) {
        window_list = window;
    } else {
        window_t *current = window_list;
        window_t *prev = NULL;

        while (current && current->z_order <= window->z_order) {
            prev = current;
            current = current->next;
        }

        if (prev) {
            window->next = prev->next;
            prev->next = window;
        } else {
            window->next = window_list;
            window_list = window;
        }
    }

    return window;
}

/* Destroy window */
void wm_destroy_window(window_t *window) {
    if (!window) return;

    // Remove from list
    if (window_list == window) {
        window_list = window->next;
    } else {
        window_t *current = window_list;
        while (current && current->next != window) {
            current = current->next;
        }
        if (current) {
            current->next = window->next;
        }
    }

    // Destroy all widgets in window
    widget_t *widget = window->root_widget;
    while (widget) {
        widget_t *next = widget->next;
        ui_destroy_widget(widget);
        widget = next;
    }

    // Update focus
    if (focused_window == window) {
        focused_window = window_list; // Focus next window
        if (focused_window) {
            focused_window->focused = 1;
        }
    }

    mem_free(window);
}

/* Show window */
void wm_show_window(window_t *window) {
    if (window) {
        window->visible = 1;
        window->minimized = 0;
    }
}

/* Hide window */
void wm_hide_window(window_t *window) {
    if (window) {
        window->visible = 0;
    }
}

/* Focus window */
void wm_focus_window(window_t *window) {
    if (!window || window == focused_window) return;

    // Unfocus current window
    if (focused_window) {
        focused_window->focused = 0;
    }

    // Focus new window
    focused_window = window;
    window->focused = 1;

    // Bring to front (highest z-order)
    int max_z = 0;
    window_t *current = window_list;
    while (current) {
        if (current->z_order > max_z) max_z = current->z_order;
        current = current->next;
    }
    window->z_order = max_z + 1;

    // Re-sort window list
    // Remove window from list
    if (window_list == window) {
        window_list = window->next;
    } else {
        current = window_list;
        while (current && current->next != window) {
            current = current->next;
        }
        if (current) {
            current->next = window->next;
        }
    }

    // Insert at front
    window->next = window_list;
    window_list = window;
}

/* Minimize window */
void wm_minimize_window(window_t *window) {
    if (window) {
        window->minimized = 1;
    }
}

/* Move window */
void wm_move_window(window_t *window, int x, int y) {
    if (window) {
        window->x = x;
        window->y = y;
    }
}

/* Resize window */
void wm_resize_window(window_t *window, int width, int height) {
    if (window) {
        window->width = width;
        window->height = height;
    }
}

/* Set window title */
void wm_set_window_title(window_t *window, const char *title) {
    if (window && title) {
        size_t len = strlen(title);
        if (len > sizeof(window->title) - 1) len = sizeof(window->title) - 1;
        memcpy(window->title, title, len);
        window->title[len] = '\0';
    }
}

/* Add widget to window */
void wm_add_widget_to_window(window_t *window, widget_t *widget) {
    if (!window || !widget) return;

    // Adjust widget coordinates relative to window
    widget->x += window->x;
    widget->y += window->y;

    // Add to window's widget list
    widget->next = window->root_widget;
    window->root_widget = widget;
}

/* Remove widget from window */
void wm_remove_widget_from_window(window_t *window, widget_t *widget) {
    if (!window || !widget) return;

    // Adjust widget coordinates back to absolute
    widget->x -= window->x;
    widget->y -= window->y;

    // Remove from window's widget list
    if (window->root_widget == widget) {
        window->root_widget = widget->next;
    } else {
        widget_t *current = window->root_widget;
        while (current && current->next != widget) {
            current = current->next;
        }
        if (current) {
            current->next = widget->next;
        }
    }
}

/* Handle touch events */
void wm_handle_touch(int x, int y, touch_action_t action) {
    window_t *window = wm_get_window_at(x, y);
    if (window) {
        wm_focus_window(window);

        // Adjust coordinates relative to window
        int rel_x = x - window->x;
        int rel_y = y - window->y;

        // Handle window chrome (title bar, etc.)
        if (rel_y < 30) { // Title bar area
            // Handle window controls
            return;
        }

        // Pass event to window's widgets
        ui_handle_touch(rel_x, rel_y, action);
    }
}

/* Render a single window */
static void wm_render_window(window_t *window);

/* Render all windows */
void wm_render_all(void) {
    fb_clear(COLOR_WHITE); // Clear screen

    window_t *window = window_list;
    while (window) {
        if (window->visible && !window->minimized) {
            wm_render_window(window);
        }
        window = window->next;
    }

    fb_flush();
}

/* Render a single window */
static void wm_render_window(window_t *window) {
    // Draw window background
    fb_fill_rect(window->x, window->y, window->width, window->height, window->bg_color);

    // Draw window border
    uint32_t border_color = window->focused ? COLOR_BLUE : COLOR_GRAY;
    fb_draw_rect(window->x, window->y, window->width, window->height, border_color);

    // Draw title bar
    fb_fill_rect(window->x, window->y, window->width, 30, border_color);
    fb_draw_text(window->x + 10, window->y + 8, window->title, COLOR_WHITE, COLOR_TRANSPARENT);

    // Draw close button
    fb_draw_rect(window->x + window->width - 30, window->y + 5, 20, 20, COLOR_RED);
    fb_draw_line(window->x + window->width - 25, window->y + 10,
                 window->x + window->width - 15, window->y + 20, COLOR_WHITE);
    fb_draw_line(window->x + window->width - 15, window->y + 10,
                 window->x + window->width - 25, window->y + 20, COLOR_WHITE);

    // Render window widgets
    widget_t *widget = window->root_widget;
    while (widget) {
        ui_render_widget(widget);
        widget = widget->next;
    }
}

/* Get window at coordinates */
window_t *wm_get_window_at(int x, int y) {
    window_t *window = window_list; // Windows are sorted by z-order (front to back)

    while (window) {
        if (window->visible && !window->minimized &&
            x >= window->x && x < window->x + window->width &&
            y >= window->y && y < window->y + window->height) {
            return window;
        }
        window = window->next;
    }

    return NULL;
}

/* Get focused window */
window_t *wm_get_focused_window(void) {
    return focused_window;
}

/* Get window count */
int wm_get_window_count(void) {
    int count = 0;
    window_t *window = window_list;
    while (window) {
        count++;
        window = window->next;
    }
    return count;
}
/*
 * Mobile UI System Implementation
 * uOS(m) - User OS Mobile
 */

#include "mobile_ui.h"
#include "ui_animation.h"
#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static ui_state_t ui_state = UI_STATE_INIT;
static int screen_brightness = 100;

/* Initialize mobile UI system */
int mobile_ui_init(void) {
    ui_state = UI_STATE_INIT;

    // Initialize subsystems
    if (fb_init() < 0) {
        return -1;
    }

    if (ui_widget_init() < 0) {
        return -1;
    }

    if (wm_init() < 0) {
        return -1;
    }

    if (ui_anim_init() < 0) {
        return -1;
    }

    ui_state = UI_STATE_RUNNING;
    return 0;
}

/* Start UI system */
int mobile_ui_start(void) {
    if (ui_state != UI_STATE_RUNNING) {
        return -1;
    }

    // Create demo UI
    mobile_ui_create_demo();
    mobile_ui_demo_animations();

    return 0;
}

/* Stop UI system */
void mobile_ui_stop(void) {
    ui_state = UI_STATE_SHUTDOWN;
}

/* Get UI system state */
ui_state_t mobile_ui_get_state(void) {
    return ui_state;
}

/* Main UI event loop */
void mobile_ui_event_loop(void) {
    if (ui_state != UI_STATE_RUNNING) {
        return;
    }

    // Update animations
    ui_anim_update();

    // Render all windows
    wm_render_all();
}

/* Handle touch input */
void mobile_ui_handle_touch(int x, int y, touch_action_t action) {
    if (ui_state != UI_STATE_RUNNING) {
        return;
    }

    wm_handle_touch(x, y, action);
}

/* Handle keyboard input */
void mobile_ui_handle_key(int keycode, int pressed) {
    if (ui_state != UI_STATE_RUNNING) {
        return;
    }

    // Handle global keyboard shortcuts
    switch (keycode) {
        case 0x1B: // ESC
            if (pressed) {
                // Show/hide task switcher or main menu
            }
            break;
        case 0x0D: // Enter
            if (pressed) {
                // Activate focused element
            }
            break;
        default:
            // Pass to focused window
            window_t *focused = wm_get_focused_window();
            if (focused) {
                // Handle key in focused window
            }
            break;
    }
}

/* Set screen brightness */
void mobile_ui_set_brightness(int brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    screen_brightness = brightness;

    // In a real implementation, this would control the display hardware
}

/* Get screen brightness */
int mobile_ui_get_brightness(void) {
    return screen_brightness;
}

/* Create demo UI */
void mobile_ui_create_demo(void) {
    // Create main window
    window_t *main_window = wm_create_window("uOS(m) Mobile", 50, 100, 980, 1600);
    if (!main_window) return;

    // Create title label
    widget_t *title_label = ui_create_label(20, 50, 940, 60, "Welcome to uOS(m)");
    ui_set_widget_colors(title_label, COLOR_TRANSPARENT, COLOR_BLACK, COLOR_TRANSPARENT);
    wm_add_widget_to_window(main_window, title_label);

    // Create status label
    widget_t *status_label = ui_create_label(20, 120, 940, 40, "Mobile Operating System Ready");
    ui_set_widget_colors(status_label, COLOR_TRANSPARENT, COLOR_BLUE, COLOR_TRANSPARENT);
    wm_add_widget_to_window(main_window, status_label);

    // Create buttons
    widget_t *apps_button = ui_create_button(20, 200, 200, 80, "Apps");
    wm_add_widget_to_window(main_window, apps_button);

    widget_t *settings_button = ui_create_button(240, 200, 200, 80, "Settings");
    wm_add_widget_to_window(main_window, settings_button);

    widget_t *files_button = ui_create_button(460, 200, 200, 80, "Files");
    wm_add_widget_to_window(main_window, files_button);

    // Create system info display
    widget_t *info_label = ui_create_label(20, 320, 940, 200,
        "System Information:\n"
        "Kernel: uOS(m) v1.0\n"
        "Architecture: RISC-V 64-bit\n"
        "Memory: Protected with PMP\n"
        "Security: ASLR + Stack Canaries\n"
        "UI: Touch-optimized Mobile Interface");
    ui_set_widget_colors(info_label, COLOR_TRANSPARENT, COLOR_BLACK, COLOR_TRANSPARENT);
    wm_add_widget_to_window(main_window, info_label);

    // Create terminal button
    widget_t *terminal_button = ui_create_button(20, 550, 300, 80, "Terminal");
    wm_add_widget_to_window(main_window, terminal_button);

    // Create browser button
    widget_t *browser_button = ui_create_button(340, 550, 300, 80, "Browser");
    wm_add_widget_to_window(main_window, browser_button);

    // Create power button
    widget_t *power_button = ui_create_button(680, 550, 200, 80, "Power");
    ui_set_widget_colors(power_button, COLOR_RED, COLOR_WHITE, COLOR_BLACK);
    wm_add_widget_to_window(main_window, power_button);

    // Focus the main window
    wm_focus_window(main_window);
}

/* Demo animation showcase */
void mobile_ui_demo_animations(void) {
    window_t *main_window = wm_get_focused_window();
    if (!main_window) return;

    // Get some widgets to animate
    widget_t *apps_button = NULL;
    widget_t *settings_button = NULL;
    widget_t *files_button = NULL;

    // Find buttons in the window's widget list
    widget_t *widget = main_window->root_widget;
    while (widget) {
        if (widget->text) {
            if (strcmp(widget->text, "Apps") == 0) {
                apps_button = widget;
            } else if (strcmp(widget->text, "Settings") == 0) {
                settings_button = widget;
            } else if (strcmp(widget->text, "Files") == 0) {
                files_button = widget;
            }
        }
        widget = widget->next;
    }

    if (apps_button) {
        // Bounce in animation
        animation_t *bounce_anim = ui_anim_bounce_in(apps_button, 1000);
        ui_anim_start(bounce_anim);
    }

    if (settings_button) {
        // Spring scale animation (1.2x = 0x1333 in 16.16 fixed-point)
        animation_t *spring_anim = ui_anim_spring_scale(settings_button, 0x13333, 800);
        ui_anim_start(spring_anim);
    }

    if (files_button) {
        // Color transition animation
        animation_t *color_anim = ui_anim_create_color(files_button,
            COLOR_BLUE, COLOR_GREEN, 1500);
        ui_anim_set_easing(color_anim, EASING_EASE_IN_OUT_SINE);
        ui_anim_start(color_anim);
    }
}
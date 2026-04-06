/*
 * Animation System Test
 * uOS(m) - User OS Mobile
 * Demonstrates iOS-style smooth animations
 */

#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"
#include "mobile_ui.h"
#include "ui_animation.h"
#include "ui_widget.h"
#include "window_manager.h"

void test_basic_animations(void) {
    printf("Testing basic animations...\n");

    // Create a test window
    window_t *test_window = wm_create_window("Animation Test", 100, 100, 600, 400);
    if (!test_window) {
        printf("Failed to create test window\n");
        return;
    }

    // Create test widgets
    widget_t *test_button = ui_create_button(50, 50, 150, 60, "Animate Me!");
    wm_add_widget_to_window(test_window, test_button);

    widget_t *test_label = ui_create_label(50, 150, 300, 40, "Testing animations...");
    wm_add_widget_to_window(test_window, test_label);

    // Test position animation
    animation_t *pos_anim = ui_anim_create_position(test_button,
        test_button->x, test_button->y,
        test_button->x + 200, test_button->y, 1000);
    ui_anim_set_easing(pos_anim, EASING_EASE_OUT_QUART);
    ui_anim_start(pos_anim);

    // Test color animation
    animation_t *color_anim = ui_anim_create_color(test_label,
        COLOR_BLACK, COLOR_BLUE, 1500);
    ui_anim_set_easing(color_anim, EASING_EASE_IN_OUT_SINE);
    ui_anim_start(color_anim);

    printf("Basic animations started\n");
}

void test_ios_style_animations(void) {
    printf("Testing iOS-style animations...\n");

    // Create a test window
    window_t *ios_window = wm_create_window("iOS Style", 200, 200, 500, 300);
    if (!ios_window) {
        printf("Failed to create iOS window\n");
        return;
    }

    // Create test widgets
    widget_t *bounce_button = ui_create_button(50, 50, 120, 50, "Bounce");
    wm_add_widget_to_window(ios_window, bounce_button);

    widget_t *spring_button = ui_create_button(200, 50, 120, 50, "Spring");
    wm_add_widget_to_window(ios_window, spring_button);

    widget_t *fade_button = ui_create_button(350, 50, 120, 50, "Fade");
    wm_add_widget_to_window(ios_window, fade_button);

    // Bounce animation
    animation_t *bounce_anim = ui_anim_bounce_in(bounce_button, 800);
    ui_anim_start(bounce_anim);

    // Spring animation
    animation_t *spring_anim = ui_anim_spring_scale(spring_button, 0x14CCCC, 600); // 1.3 in 16.16 fixed point
    ui_anim_start(spring_anim);

    // Fade animation
    animation_t *fade_anim = ui_anim_fade_in(fade_button, 1000);
    ui_anim_start(fade_anim);

    printf("iOS-style animations started\n");
}

void test_animation_groups(void) {
    printf("Testing animation groups...\n");

    // Create a test window
    window_t *group_window = wm_create_window("Group Test", 300, 300, 400, 200);
    if (!group_window) {
        printf("Failed to create group window\n");
        return;
    }

    // Create test widgets
    widget_t *widget1 = ui_create_button(20, 20, 80, 40, "W1");
    widget_t *widget2 = ui_create_button(120, 20, 80, 40, "W2");
    widget_t *widget3 = ui_create_button(220, 20, 80, 40, "W3");

    wm_add_widget_to_window(group_window, widget1);
    wm_add_widget_to_window(group_window, widget2);
    wm_add_widget_to_window(group_window, widget3);

    // Create parallel animations
    animation_t *anim1 = ui_anim_create_position(widget1, 20, 20, 20, 100, 500);
    animation_t *anim2 = ui_anim_create_position(widget2, 120, 20, 120, 100, 500);
    animation_t *anim3 = ui_anim_create_position(widget3, 220, 20, 220, 100, 500);

    animation_t *animations[] = {anim1, anim2, anim3};
    anim_group_t *group = ui_anim_create_group(animations, 3, 1); // Parallel
    ui_anim_start_group(group);

    printf("Animation group started\n");
}

int main(void) {
    printf("Animation System Test Starting...\n");

    // Initialize memory system
    if (mem_init() < 0) {
        printf("Failed to initialize memory system\n");
        return -1;
    }

    // Initialize UI system
    if (mobile_ui_init() < 0) {
        printf("Failed to initialize mobile UI\n");
        return -1;
    }

    // Create demo UI
    mobile_ui_create_demo();

    // Run animation tests
    test_basic_animations();
    test_ios_style_animations();
    test_animation_groups();

    // Run UI event loop for a few iterations to see animations
    printf("Running animation loop...\n");
    for (int i = 0; i < 100; i++) {
        mobile_ui_event_loop();
        // Small delay (in real system, this would be handled by kernel timer)
    }

    printf("Animation tests completed\n");
    return 0;
}
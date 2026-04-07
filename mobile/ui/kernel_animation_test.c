/*
 * Animation Test for uOS(m) Kernel
 * Tests the animation system and outputs results to UART
 */

#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"
#include "mobile_ui.h"
#include "ui_animation.h"
#include "ui_widget.h"
#include "window_manager.h"

// Simple UART output for testing
void uart_putc(char c) {
    volatile char *uart = (volatile char *)0x10000000L;
    while ((*(volatile unsigned char *)(0x10000000L + 0x5) & 0x20) == 0);
    *uart = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

void uart_puthex(uint64_t val) {
    char buf[17];
    buf[16] = 0;
    for (int i = 15; i >= 0; i--) {
        int digit = val & 0xF;
        buf[i] = (digit < 10) ? '0' + digit : 'A' + digit - 10;
        val >>= 4;
    }
    uart_puts("0x");
    uart_puts(buf);
}

void test_animation_system(void) {
    uart_puts("\n=== Testing Animation System ===\n");

    // Initialize memory system
    if (mem_init() < 0) {
        uart_puts("ERROR: Failed to initialize memory system\n");
        return;
    }
    uart_puts("Memory system initialized\n");

    // Initialize UI system
    if (mobile_ui_init() < 0) {
        uart_puts("ERROR: Failed to initialize mobile UI\n");
        return;
    }
    uart_puts("Mobile UI initialized\n");

    // Create a test window
    window_t *test_window = wm_create_window("Animation Test", 100, 100, 600, 400);
    if (!test_window) {
        uart_puts("ERROR: Failed to create test window\n");
        return;
    }
    uart_puts("Test window created\n");

    // Create test widgets
    widget_t *test_button = ui_create_button(50, 50, 150, 60, "Animate!");
    if (!test_button) {
        uart_puts("ERROR: Failed to create test button\n");
        return;
    }
    wm_add_widget_to_window(test_window, test_button);
    uart_puts("Test button created\n");

    // Test position animation
    animation_t *pos_anim = ui_anim_create_position(test_button,
        test_button->x, test_button->y,
        test_button->x + 200, test_button->y, 1000);
    if (!pos_anim) {
        uart_puts("ERROR: Failed to create position animation\n");
        return;
    }
    ui_anim_set_easing(pos_anim, EASING_EASE_OUT_QUART);
    ui_anim_start(pos_anim);
    uart_puts("Position animation started\n");

    // Test color animation
    widget_t *test_label = ui_create_label(50, 150, 300, 40, "Testing...");
    if (test_label) {
        wm_add_widget_to_window(test_window, test_label);
        animation_t *color_anim = ui_anim_create_color(test_label,
            0xFF0000FF, 0xFF00FF00, 1500); // Red to Green
        if (color_anim) {
            ui_anim_set_easing(color_anim, EASING_EASE_IN_OUT_SINE);
            ui_anim_start(color_anim);
            uart_puts("Color animation started\n");
        }
    }

    // Test iOS-style animations
    widget_t *bounce_button = ui_create_button(50, 250, 120, 50, "Bounce");
    if (bounce_button) {
        wm_add_widget_to_window(test_window, bounce_button);
        animation_t *bounce_anim = ui_anim_bounce_in(bounce_button, 800);
        if (bounce_anim) {
            ui_anim_start(bounce_anim);
            uart_puts("Bounce animation started\n");
        }
    }

    widget_t *spring_button = ui_create_button(200, 250, 120, 50, "Spring");
    if (spring_button) {
        wm_add_widget_to_window(test_window, spring_button);
        animation_t *spring_anim = ui_anim_spring_scale(spring_button, 0x13333, 600); // 1.2x
        if (spring_anim) {
            ui_anim_start(spring_anim);
            uart_puts("Spring animation started\n");
        }
    }

    // Run animation loop and monitor progress
    uart_puts("Running animation loop...\n");
    for (int frame = 0; frame < 120; frame++) {  // 2 seconds at ~60fps
        mobile_ui_event_loop();

        // Report progress every 10 frames
        if (frame % 10 == 0) {
            uart_puts("Frame ");
            // Simple decimal output
            if (frame < 10) uart_putc('0' + frame);
            else if (frame < 100) {
                uart_putc('0' + frame / 10);
                uart_putc('0' + frame % 10);
            } else {
                uart_putc('1');
                uart_putc('0' + (frame - 100) / 10);
                uart_putc('0' + (frame - 100) % 10);
            }
            uart_puts(": Animations running\n");
        }

        // Small delay (in real kernel, this would be timer-based)
        for (volatile int i = 0; i < 100000; i++);
    }

    uart_puts("Animation tests completed!\n");
    uart_puts("=== Animation System Test Complete ===\n");
}

int main(void) {
    uart_puts("\nStarting Animation System Test...\n");
    test_animation_system();
    uart_puts("Test finished.\n");
    return 0;
}
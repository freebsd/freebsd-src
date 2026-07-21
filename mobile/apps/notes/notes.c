/*
 * Notes App
 */

#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <stdio.h>

static void draw_notes(void) {
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFFEEEEEE);
    fb_fill_rect(0, 0, FB_WIDTH, 150, 0xFFFFCC00);
    fb_draw_text(50, 60, "My Notes", COLOR_BLACK, COLOR_TRANSPARENT);
    ui_render_all();
}

int main(void) {
    mobile_ui_init();
    wm_create_window("Notes", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    ui_add_widget(ui_create_button(50, 200, FB_WIDTH - 100, 200, "Buy groceries"));
    ui_add_widget(ui_create_button(50, 450, FB_WIDTH - 100, 200, "Call mom"));
    ui_add_widget(ui_create_button(50, 700, FB_WIDTH - 100, 200, "Fix UOS(m) bugs"));
    
    while (1) {
        draw_notes();
        fb_flush();
        mobile_ui_event_loop();
    }
    return 0;
}

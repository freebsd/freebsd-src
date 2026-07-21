/*
 * Calculator App
 */

#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <stdio.h>
#include <string.h>

static void draw_calculator(void) {
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFF222222);
    fb_draw_text(50, 100, "Calculator", COLOR_WHITE, COLOR_TRANSPARENT);
    ui_render_all();
}

int main(void) {
    mobile_ui_init();
    wm_create_window("Calculator", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    /* Simple label for result */
    ui_add_widget(ui_create_button(50, 200, FB_WIDTH - 100, 150, "0"));
    
    /* Basic grid of buttons */
    int i, j;
    char labels[4][4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"C", "0", "=", "+"}
    };
    
    int start_y = 400;
    int btn_size = (FB_WIDTH - 250) / 4;
    if (btn_size < 100) btn_size = 200;
    int padding = 40;
    
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            ui_add_widget(ui_create_button(
                padding + j * (btn_size + padding),
                start_y + i * (btn_size + padding),
                btn_size, btn_size, labels[i][j]
            ));
        }
    }
    
    while (1) {
        draw_calculator();
        fb_flush();
        mobile_ui_event_loop();
    }
    return 0;
}

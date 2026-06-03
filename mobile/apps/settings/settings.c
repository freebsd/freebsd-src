/*
 * Settings App - System Configuration
 * uOS(m) - User OS Mobile
 */

#include "settings.h"
#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <string.h>
#include <stdio.h>

static settings_t g_settings = {0};
static const char *section_names[MAX_SECTIONS] = {
    "Network", "Display", "Sound", "Battery", "Storage", "Security", "About"
};

static void add_setting(const char *name, int has_toggle, int has_slider, 
                      int min_val, int max_val, settings_section_t section) {
    if (g_settings.item_count < MAX_SETTINGS) {
        setting_item_t *item = &g_settings.items[g_settings.item_count];
        strncpy(item->name, name, 31);
        item->has_toggle = has_toggle;
        item->has_slider = has_slider;
        item->slider_value = (min_val + max_val) / 2;
        item->min_value = min_val;
        item->max_value = max_val;
        item->section = section;
        item->enabled = 0;
        item->y_offset = 150 + g_settings.item_count * 80;
        g_settings.item_count++;
    }
}

static void draw_header(void) {
    fb_fill_rect(0, 0, FB_WIDTH, 100, COLOR_GRAY);
    fb_draw_text(40, 40, section_names[g_settings.current_section], 
                 COLOR_WHITE, COLOR_TRANSPARENT);
}

static void draw_setting_item(setting_item_t *item, int idx) {
    int y = item->y_offset - g_settings.scroll_offset;
    if (y < 120 || y > FB_HEIGHT - 50) return;
    
    fb_fill_rect(40, y, FB_WIDTH - 80, 60, 0xFF202020);
    fb_draw_text(60, y + 20, item->name, COLOR_WHITE, COLOR_TRANSPARENT);
    
    if (item->has_toggle) {
        int toggle_x = FB_WIDTH - 120;
        uint32_t color = item->enabled ? 0xFF40FF40 : COLOR_GRAY;
        fb_fill_rect(toggle_x, y + 15, 50, 30, color);
    }
    
    if (item->has_slider) {
        int slider_x = FB_WIDTH - 200;
        fb_fill_rect(slider_x, y + 30, 120, 10, COLOR_GRAY);
        int fill = slider_x + (item->slider_value - item->min_value) * 120 / 
                 (item->max_value - item->min_value);
        fb_fill_rect(slider_x, y + 30, fill - slider_x, 10, COLOR_BLUE);
    }
}

int settings_init(void) {
    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.current_section = 0;
    
    add_setting("WiFi", 1, 0, 0, 0, SECTION_NETWORK);
    add_setting("Bluetooth", 1, 0, 0, 0, SECTION_NETWORK);
    add_setting("Airplane Mode", 1, 0, 0, 0, SECTION_NETWORK);
    
    add_setting("Brightness", 0, 1, 0, 100, SECTION_DISPLAY);
    
    add_setting("Volume", 0, 1, 0, 100, SECTION_SOUND);
    add_setting("Ringtone", 0, 1, 0, 100, SECTION_SOUND);
    
    add_setting("Battery Level", 0, 0, 0, 0, SECTION_BATTERY);
    
    add_setting("Internal Storage", 0, 0, 0, 0, SECTION_STORAGE);
    
    add_setting("Screen Lock", 1, 0, 0, 0, SECTION_SECURITY);
    add_setting("Notifications", 1, 0, 0, 0, SECTION_SECURITY);
    
    add_setting("Version", 0, 0, 0, 0, SECTION_ABOUT);
    
    wm_init();
    ui_widget_init();
    return 0;
}

void settings_deinit(void) {}

void settings_render(void) {
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFF101010);
    draw_header();
    
    int i;
    for (i = 0; i < g_settings.item_count; i++) {
        if (g_settings.items[i].section == g_settings.current_section) {
            draw_setting_item(&g_settings.items[i], i);
        }
    }
}

void settings_handle_touch(int x, int y, int action) {
    if (action == 0) {
        if (y < 100) {
            if (x < 100) settings_prev_section();
            else if (x > FB_WIDTH - 100) settings_next_section();
        }
    } else {
        int i;
        for (i = 0; i < g_settings.item_count; i++) {
            if (g_settings.items[i].section == g_settings.current_section) {
                if (y >= g_settings.items[i].y_offset - g_settings.scroll_offset &&
                    y <= g_settings.items[i].y_offset - g_settings.scroll_offset + 60) {
                    settings_toggle_item(i);
                }
            }
        }
    }
}

void settings_next_section(void) {
    g_settings.current_section = (g_settings.current_section + 1) % MAX_SECTIONS;
}

void settings_prev_section(void) {
    g_settings.current_section = (g_settings.current_section - 1 + MAX_SECTIONS) % MAX_SECTIONS;
}

void settings_toggle_item(int index) {
    if (g_settings.items[index].has_toggle) {
        g_settings.items[index].enabled = !g_settings.items[index].enabled;
    }
}

int main(void) {
    if (settings_init() != 0) return 1;
    
    mobile_ui_init();
    window_t *win = wm_create_window("Settings", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    while (1) {
        settings_render();
        fb_flush();
        mobile_ui_event_loop();
    }
    
    settings_deinit();
    return 0;
}
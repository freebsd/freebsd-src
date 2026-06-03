/*
 * Home Screen App - Mobile OS Launcher
 * uOS(m) - User OS Mobile
 */

#include "home_screen.h"
#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static home_screen_t g_home = {0};
static widget_t *g_clock_label = NULL;
static widget_t *g_app_grid[MAX_APPS];

static void draw_wallpaper(void) {
    framebuffer_t *fb = fb_get();
    int y;
    uint32_t color;
    
    for (y = 0; y < fb->height; y++) {
        int ratio = y * 255 / fb->height;
        color = (0xFF - ratio) << 24 | (ratio / 2) << 16 | (0xFF - ratio) << 8 | 0xFF;
        fb_fill_rect(0, y, fb->width, 1, color);
    }
}

static void draw_page_indicator(void) {
    int dot_x = 500;
    int dot_y = 1800;
    int i;
    uint32_t color;
    
    for (i = 0; i < 5; i++) {
        color = (i == g_home.current_page) ? COLOR_WHITE : COLOR_GRAY;
        fb_fill_circle(dot_x + i * 30, dot_y, 8, color);
    }
}

static void on_app_click(widget_t *widget) {
    int i;
    for (i = 0; i < g_home.app_count; i++) {
        if (g_app_grid[i] == widget) {
            home_screen_open_app(g_home.apps[i].package_id);
            return;
        }
    }
}

static void draw_clock(time_t *now) {
    struct tm *tm_info = localtime(now);
    char time_str[16];
    char date_str[32];
    
    snprintf(time_str, sizeof(time_str), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    snprintf(date_str, sizeof(date_str), "%02d/%02d/%02d", 
             tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_year % 100);
    
    fb_draw_text(50, 80, time_str, COLOR_WHITE, COLOR_TRANSPARENT);
    fb_draw_text(50, 140, date_str, 0xFFCCCCCC, COLOR_TRANSPARENT);
}

static void create_app_grid(void) {
    int cols = 4;
    int rows = 6;
    int icon_size = 120;
    int padding = 80;
    int start_y = 250;
    int i;
    
    for (i = 0; i < g_home.app_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = padding + col * (icon_size + padding);
        int y = start_y + row * (icon_size + padding);
        
        g_home.apps[i].icon_x = x;
        g_home.apps[i].icon_y = y;
        
        g_app_grid[i] = ui_create_button(x, y, icon_size, icon_size, "");
        if (g_app_grid[i]) {
            g_app_grid[i]->on_click = on_app_click;
            ui_add_widget(g_app_grid[i]);
            
            fb_fill_rect(x, y, icon_size, icon_size, 
                        0xFF404040 + (i * 0x10101));
            
            char label[64];
            snprintf(label, sizeof(label), "%s", g_home.apps[i].name);
            int text_x = x + (icon_size - strlen(label) * 8) / 2;
            int text_y = y + icon_size + 20;
            fb_draw_text(text_x, text_y, label, COLOR_WHITE, COLOR_TRANSPARENT);
        }
    }
}

int home_screen_init(void) {
    memset(&g_home, 0, sizeof(g_home));
    g_home.current_page = 0;
    g_home.drawer_open = 0;
    
    home_screen_add_app("Settings", "com.uos.settings", 0);
    home_screen_add_app("Terminal", "com.uos.terminal", 0);
    home_screen_add_app("Browser", "com.uos.browser", 0);
    home_screen_add_app("Contacts", "com.uos.contacts", 0);
    home_screen_add_app("Messages", "com.uos.messages", 0);
    
    wm_init();
    ui_widget_init();
    
    create_app_grid();
    
    return 0;
}

void home_screen_deinit(void) {
    int i;
    for (i = 0; i < g_home.app_count; i++) {
        if (g_app_grid[i]) {
            ui_destroy_widget(g_app_grid[i]);
        }
    }
}

void home_screen_render(void) {
    draw_wallpaper();
    time_t now = time(NULL);
    draw_clock(&now);
    draw_page_indicator();
    ui_render_all();
}

void home_screen_handle_touch(int x, int y, int action) {
    if (action == 0) {
        if (y > 1500) g_home.pull_start_y = y;
    } else if (action == 1) {
        int delta = y - g_home.pull_start_y;
        if (delta > 100) {
            home_screen_open_drawer();
        }
    }
    
    ui_handle_touch(x, y, (touch_action_t)action);
}

void home_screen_open_app(const char *package_id) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "mobile_app_start %s", package_id);
    system(cmd);
}

void home_screen_open_drawer(void) {
    g_home.drawer_open = 1;
    fb_fill_rect(0, 300, FB_WIDTH, 1620, 0xCC000000);
}

void home_screen_close_drawer(void) {
    g_home.drawer_open = 0;
}

void home_screen_add_app(const char *name, const char *package_id, int page) {
    if (g_home.app_count < MAX_APPS) {
        strncpy(g_home.apps[g_home.app_count].name, name, 31);
        strncpy(g_home.apps[g_home.app_count].package_id, package_id, 63);
        g_home.apps[g_home.app_count].page = page;
        g_home.app_count++;
    }
}

int main(void) {
    if (home_screen_init() != 0) {
        return 1;
    }
    
    mobile_ui_init();
    window_t *win = wm_create_window("Home", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    while (1) {
        home_screen_render();
        fb_flush();
        mobile_ui_event_loop();
    }
    
    home_screen_deinit();
    return 0;
}
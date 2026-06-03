/*
 * Browser App - Basic HTML Renderer
 * uOS(m) - User OS Mobile
 */

#include "browser.h"
#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <string.h>
#include <stdio.h>

static browser_t g_browser = {0};

static html_tag_type_t parse_tag(const char *tag) {
    if (strcmp(tag, "h1") == 0) return TAG_H1;
    if (strcmp(tag, "h2") == 0) return TAG_H2;
    if (strcmp(tag, "h3") == 0) return TAG_H3;
    if (strcmp(tag, "p") == 0) return TAG_P;
    if (strcmp(tag, "a") == 0) return TAG_A;
    if (strcmp(tag, "div") == 0) return TAG_DIV;
    if (strcmp(tag, "img") == 0) return TAG_IMG;
    if (strcmp(tag, "br") == 0) return TAG_BR;
    return TAG_UNKNOWN;
}

static void parse_html(const char *html) {
    const char *p = html;
    const char *tag_start, *tag_end, *content_start, *content_end;
    
    while (*p && g_browser.tabs[g_browser.current_tab].element_count < MAX_HTML_TAGS) {
        if (*p == '<') {
            tag_start = p + 1;
            tag_end = strchr(tag_start, '>');
            if (!tag_end) break;
            
            char tag_name[MAX_TAG_NAME] = {0};
            int len = tag_end - tag_start;
            if (len >= MAX_TAG_NAME) len = MAX_TAG_NAME - 1;
            strncpy(tag_name, tag_start, len);
            
            html_element_t *el = &g_browser.tabs[g_browser.current_tab].elements
                               [g_browser.tabs[g_browser.current_tab].element_count];
            el->type = parse_tag(tag_name);
            el->font_size = 16;
            el->color = COLOR_WHITE;
            
            p = tag_end + 1;
            content_start = p;
            content_end = strchr(p, '<');
            if (content_end) {
                len = content_end - content_start;
                if (len >= MAX_CONTENT) len = MAX_CONTENT - 1;
                strncpy(el->content, content_start, len);
                el->content[len] = '\0';
                p = content_end;
            }
            g_browser.tabs[g_browser.current_tab].element_count++;
        } else {
            p++;
        }
    }
}

static void draw_url_bar(void) {
    fb_fill_rect(0, 0, FB_WIDTH, 80, 0xFF303030);
    fb_draw_text(20, 30, g_browser.tabs[g_browser.current_tab].url, 
                 COLOR_WHITE, COLOR_TRANSPARENT);
}

static void draw_toolbar(void) {
    int y = 80;
    fb_fill_rect(0, y, FB_WIDTH, 60, 0xFF202020);
    
    fb_fill_rect(20, y + 15, 40, 30, COLOR_GRAY);
    fb_draw_text(30, y + 20, "<", COLOR_WHITE, COLOR_TRANSPARENT);
    
    fb_fill_rect(70, y + 15, 40, 30, COLOR_GRAY);
    fb_draw_text(80, y + 20, ">", COLOR_WHITE, COLOR_TRANSPARENT);
    
    fb_fill_rect(120, y + 15, 40, 30, COLOR_GRAY);
    fb_draw_text(130, y + 20, "R", COLOR_WHITE, COLOR_TRANSPARENT);
    
    int i;
    for (i = 0; i < MAX_TABS; i++) {
        uint32_t color = (i == g_browser.current_tab) ? COLOR_BLUE : COLOR_GRAY;
        fb_fill_circle(900 + i * 50, y + 30, 15, color);
    }
}

static void render_element(html_element_t *el, int y_offset) {
    int y = el->y - g_browser.scroll_y + 140;
    if (y < 140 || y > FB_HEIGHT - 20) return;
    
    uint32_t color = (el->type == TAG_A) ? COLOR_BLUE : el->color;
    int font_size = el->font_size;
    
    switch (el->type) {
        case TAG_H1:
            fb_draw_text(40, y, el->content, color, COLOR_TRANSPARENT);
            break;
        case TAG_P:
            fb_draw_text(40, y, el->content, color, COLOR_TRANSPARENT);
            break;
        case TAG_IMG:
            fb_fill_rect(40, y, 200, 150, 0xFF505050);
            fb_draw_text(80, y + 70, "Image", COLOR_WHITE, COLOR_TRANSPARENT);
            break;
        default:
            break;
    }
}

int browser_init(void) {
    memset(&g_browser, 0, sizeof(g_browser));
    g_browser.current_tab = 0;
    strncpy(g_browser.tabs[0].url, "https://example.com", MAX_CONTENT - 1);
    browser_load_html("<h1>Welcome to uOS(m) Browser</h1><p>This is a test page.</p>");
    
    wm_init();
    ui_widget_init();
    return 0;
}

void browser_deinit(void) {}

void browser_load_html(const char *html) {
    g_browser.loading = 1;
    parse_html(html);
    g_browser.loading = 0;
}

void browser_navigate(const char *url) {
    strncpy(g_browser.tabs[g_browser.current_tab].url, url, MAX_CONTENT - 1);
    browser_load_html("<h1>Loading...</h1>");
}

void browser_render(void) {
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFF000000);
    draw_url_bar();
    draw_toolbar();
    
    int i;
    for (i = 0; i < g_browser.tabs[g_browser.current_tab].element_count; i++) {
        g_browser.tabs[g_browser.current_tab].elements[i].y = 140 + i * 40;
        render_element(&g_browser.tabs[g_browser.current_tab].elements[i], 0);
    }
}

void browser_handle_touch(int x, int y, int action) {
    if (y < 80) {
        if (x > FB_WIDTH - 100) {
            g_browser.scroll_y += 40;
        }
    } else if (y < 140) {
        if (x < 60) browser_back();
        else if (x < 110) browser_forward();
        else if (x < 160) browser_refresh();
        
        int i;
        for (i = 0; i < MAX_TABS; i++) {
            if (x > 900 + i * 50 - 20 && x < 900 + i * 50 + 20 && y > 95 && y < 115) {
                g_browser.current_tab = i;
                break;
            }
        }
    }
}

void browser_back(void) {}
void browser_forward(void) {}
void browser_refresh(void) {}
void browser_next_tab(void) {
    g_browser.current_tab = (g_browser.current_tab + 1) % MAX_TABS;
}
void browser_prev_tab(void) {
    g_browser.current_tab = (g_browser.current_tab - 1 + MAX_TABS) % MAX_TABS;
}

int main(void) {
    if (browser_init() != 0) return 1;
    
    mobile_ui_init();
    window_t *win = wm_create_window("Browser", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    while (1) {
        browser_render();
        fb_flush();
        mobile_ui_event_loop();
    }
    
    browser_deinit();
    return 0;
}
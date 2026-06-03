/*
 * Browser App - Basic HTML Renderer
 * uOS(m) - User OS Mobile
 */

#ifndef _BROWSER_H_
#define _BROWSER_H_

#include <stdint.h>

#define MAX_TABS 3
#define MAX_HTML_TAGS 128
#define MAX_TAG_NAME 32
#define MAX_CONTENT 4096
#define MAX_URLS 16

typedef enum {
    TAG_H1, TAG_H2, TAG_H3, TAG_P, TAG_A, TAG_DIV, TAG_IMG, TAG_BR, TAG_UNKNOWN
} html_tag_type_t;

typedef struct {
    html_tag_type_t type;
    char content[MAX_CONTENT];
    int x, y;
    int width, height;
    int font_size;
    uint32_t color;
} html_element_t;

typedef struct {
    char url[MAX_CONTENT];
    html_element_t elements[MAX_HTML_TAGS];
    int element_count;
} browser_tab_t;

typedef struct {
    browser_tab_t tabs[MAX_TABS];
    int current_tab;
    int scroll_y;
    int loading;
} browser_t;

int browser_init(void);
void browser_deinit(void);
void browser_render(void);
void browser_handle_touch(int x, int y, int action);
void browser_navigate(const char *url);
void browser_load_html(const char *html);
void browser_back(void);
void browser_forward(void);
void browser_refresh(void);
void browser_next_tab(void);
void browser_prev_tab(void);

#endif /* _BROWSER_H_ */
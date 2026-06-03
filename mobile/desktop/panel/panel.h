/*
 * System Panel for uOS(m) Desktop
 * Top bar with clock, battery, wifi, sound, tray, search, global menu
 * Translucent/blurred glass effect
 */

#ifndef _PANEL_H_
#define _PANEL_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/input.h"
#include "icon_theme.h"

#define PANEL_HEIGHT       48
#define PANEL_MAX_APPLETS   8
#define PANEL_MAX_MENU_ITEMS 12
#define PANEL_ALPHA        180

typedef enum {
    APPLET_CLOCK,
    APPLET_BATTERY,
    APPLET_NETWORK,
    APPLET_SOUND,
    APPLET_TRAY,
    APPLET_POWER,
    APPLET_SEARCH,
    APPLET_MENU
} applet_type_t;

typedef struct {
    applet_type_t type;
    int x, y;
    int width, height;
    int active;
    int hovered;
    char label[64];
    icon_handle_t *icon;
    void (*on_click)(void);
    void (*on_hover)(int entered);
} panel_applet_t;

typedef struct {
    panel_applet_t applets[PANEL_MAX_APPLETS];
    int applet_count;
    int x, y;
    int width, height;
    int visible;
    int search_focused;
    char search_text[128];
    int glass_effect;
    uint32_t bg_color;
    surface_t *bg_surface;
    surface_t *blur_surface;
    int search_x, search_y, search_w, search_h;
    int menu_x, menu_y, menu_w, menu_h;
    char active_app[64];
    char menu_items[PANEL_MAX_MENU_ITEMS][64];
    int menu_item_count;
} panel_t;

int panel_init(void);
void panel_shutdown(void);
int panel_add_applet(applet_type_t type);
void panel_remove_applet(int idx);
void panel_render(void);
void panel_update(void);
int panel_handle_touch(int x, int y, int action);
void panel_set_search_text(const char *text);
void panel_set_active_app(const char *app_name);
void panel_add_menu_item(const char *item);
void panel_set_glass_effect(int enabled);
panel_t *panel_get_instance(void);

#endif /* _PANEL_H_ */

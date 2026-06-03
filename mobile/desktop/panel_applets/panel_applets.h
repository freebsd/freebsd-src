/*
 * Panel Applets for uOS(m) Desktop
 * Clock, battery, network, sound, tray, power applets
 */

#ifndef _PANEL_APPLETS_H_
#define _PANEL_APPLETS_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "icon_theme.h"

#define APPLET_CLOCK_FORMAT      "%H:%M"
#define APPLET_DATE_FORMAT       "%a, %b %d"
#define APPLET_BATTERY_UPDATE_MS 30000
#define APPLET_NETWORK_UPDATE_MS 5000
#define APPLET_VOLUME_UPDATE_MS  1000
#define APPLET_CAL_WIDTH        300
#define APPLET_CAL_HEIGHT       320

typedef enum {
    APPLET_STATE_NORMAL,
    APPLET_STATE_HOVER,
    APPLET_STATE_ACTIVE,
    APPLET_STATE_DISABLED
} applet_state_t;

typedef struct {
    applet_type_t type;
    applet_state_t state;
    int x, y;
    int width, height;
    int enabled;
    icon_handle_t *icon;
    char label[64];
    uint32_t last_update;
    int menu_open;
} clock_applet_t;

typedef struct {
    applet_type_t type;
    applet_state_t state;
    int x, y;
    int width, height;
    int enabled;
    int level;
    int charging;
    icon_handle_t *icon;
    char label[32];
} battery_applet_t;

typedef struct {
    applet_type_t type;
    applet_state_t state;
    int x, y;
    int width, height;
    int enabled;
    int connected;
    int signal_strength;
    char ssid[64];
    icon_handle_t *icon;
} network_applet_t;

typedef struct {
    applet_type_t type;
    applet_state_t state;
    int x, y;
    int width, height;
    int enabled;
    int level;
    int muted;
    icon_handle_t *icon;
    char label[32];
} sound_applet_t;

typedef struct {
    applet_type_t type;
    applet_state_t state;
    int x, y;
    int width, height;
    int enabled;
    icon_handle_t *icons[16];
    int icon_count;
    int tray_count;
} tray_applet_t;

typedef struct {
    applet_type_t type;
    applet_state_t state;
    int x, y;
    int width, height;
    int enabled;
    icon_handle_t *icon;
    int menu_open;
} power_applet_t;

typedef struct {
    clock_applet_t clock;
    battery_applet_t battery;
    network_applet_t network;
    sound_applet_t sound;
    tray_applet_t tray;
    power_applet_t power;
} panel_applets_t;

int applets_init(void);
void applets_shutdown(void);
void applets_render(panel_applets_t *applets, layer_t *layer, int panel_width);
void applets_update(void);
int applets_handle_click(int x, int y);
int applets_handle_hover(int x, int y);
void applets_enable(applet_type_t type, int enabled);
void applets_set_position(applet_type_t type, int x, int y);
panel_applets_t *applets_get_instance(void);
void clock_applet_render(clock_applet_t *applet, layer_t *layer);
void battery_applet_render(battery_applet_t *applet, layer_t *layer);
void network_applet_render(network_applet_t *applet, layer_t *layer);
void sound_applet_render(sound_applet_t *applet, layer_t *layer);
void tray_applet_render(tray_applet_t *applet, layer_t *layer);
void power_applet_render(power_applet_t *applet, layer_t *layer);

#endif /* _PANEL_APPLETS_H_ */

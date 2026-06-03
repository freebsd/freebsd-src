/*
 * Panel Applets Implementation
 */

#include "panel_applets.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "icon_theme.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static panel_applets_t *g_applets = NULL;

void clock_applet_render(clock_applet_t *applet, layer_t *layer) {
    (void)applet; (void)layer;
}

void battery_applet_render(battery_applet_t *applet, layer_t *layer) {
    (void)applet; (void)layer;
}

void network_applet_render(network_applet_t *applet, layer_t *layer) {
    (void)applet; (void)layer;
}

void sound_applet_render(sound_applet_t *applet, layer_t *layer) {
    (void)applet; (void)layer;
}

void tray_applet_render(tray_applet_t *applet, layer_t *layer) {
    (void)applet; (void)layer;
}

void power_applet_render(power_applet_t *applet, layer_t *layer) {
    (void)applet; (void)layer;
}

int applets_init(void) {
    if (g_applets) return 0;
    g_applets = calloc(1, sizeof(panel_applets_t));
    if (!g_applets) return -1;
    return 0;
}

void applets_shutdown(void) {
    if (!g_applets) return;
    free(g_applets);
    g_applets = NULL;
}

void applets_render(panel_applets_t *applets, layer_t *layer, int panel_width) {
    (void)layer; (void)panel_width;
    if (!applets) return;
}

void applets_update(void) {
    if (!g_applets) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm) {
        snprintf(g_applets->clock.label, sizeof(g_applets->clock.label), "%02d:%02d", tm->tm_hour, tm->tm_min);
        g_applets->clock.enabled = 1;
    }
}

int applets_handle_click(int x, int y) { (void)x; (void)y; return 0; }
int applets_handle_hover(int x, int y) { (void)x; (void)y; return 0; }
void applets_enable(applet_type_t type, int enabled) { (void)type; (void)enabled; }
void applets_set_position(applet_type_t type, int x, int y) { (void)type; (void)x; (void)y; }
panel_applets_t *applets_get_instance(void) { return g_applets; }

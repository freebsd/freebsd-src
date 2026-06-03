/*
 * System UI - Status bar and navigation bar
 * BSD-licensed
 */

#ifndef _SYSTEMUI_H_
#define _SYSTEMUI_H_

#include <stdint.h>
#include "../framebuffer.h"
#include "../toolkit/widgets.h"

#define STATUSBAR_HEIGHT 40
#define NAVBAR_HEIGHT   60

#define COLOR_LIGHT_BG    0xFFf0f0f0
#define COLOR_LIGHT_TEXT  0xFF1a1a1a
#define COLOR_DARK_BG     0xFF1a1a2e
#define COLOR_DARK_TEXT   0xFFe0e0e0

typedef struct {
    uint32_t bg_color;
    uint32_t text_color;
    uint32_t accent_color;
} palette_t;

typedef struct {
    palette_t *palette;
    statusbar_t *statusbar;
    navbar_t *navbar;
    int visible;
    int light_mode;
} systemui_t;

int systemui_init(void);
void systemui_shutdown(void);

void systemui_set_light_mode(int enabled);
void systemui_set_palette(const palette_t *p);

void systemui_update(void);
void systemui_render(void);

void systemui_set_battery(int level);
void systemui_set_time(const char *time);
void systemui_set_wifi(int enabled);
void systemui_set_signal(int strength);

#endif /* _SYSTEMUI_H_ */
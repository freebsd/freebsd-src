/*
 * Settings App - System Configuration
 * uOS(m) - User OS Mobile
 */

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>

#define MAX_SETTINGS 64
#define MAX_SECTIONS 7

typedef enum {
    SECTION_NETWORK,
    SECTION_DISPLAY,
    SECTION_SOUND,
    SECTION_BATTERY,
    SECTION_STORAGE,
    SECTION_SECURITY,
    SECTION_ABOUT
} settings_section_t;

typedef struct {
    char name[32];
    int enabled;
    int has_toggle;
    int has_slider;
    int slider_value;
    int min_value;
    int max_value;
    settings_section_t section;
    int y_offset;
} setting_item_t;

typedef struct {
    setting_item_t items[MAX_SETTINGS];
    int item_count;
    settings_section_t current_section;
    int scroll_offset;
} settings_t;

int settings_init(void);
void settings_deinit(void);
void settings_render(void);
void settings_handle_touch(int x, int y, int action);
void settings_next_section(void);
void settings_prev_section(void);
void settings_toggle_item(int index);

#endif /* _SETTINGS_H_ */
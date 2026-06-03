/*
 * Application Dock for uOS(m) Desktop
 * Bottom bar with pinned apps, running indicators, drag reorder
 */

#ifndef _DOCK_H_
#define _DOCK_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/window_manager/window_mgr.h"
#include "icon_theme.h"

#define DOCK_MAX_APPS      64
#define DOCK_MAX_MENU_ITEMS 16
#define DOCK_DEFAULT_SIZE  72

typedef enum {
    DOCK_POSITION_BOTTOM,
    DOCK_POSITION_LEFT,
    DOCK_POSITION_RIGHT
} dock_position_t;

typedef enum {
    DOCK_ANIM_NONE,
    DOCK_ANIM_SLIDE,
    DOCK_ANIM_FADE,
    DOCK_ANIM_SPRING
} dock_anim_t;

typedef struct {
    char app_id[64];
    char name[128];
    icon_handle_t *icon;
    int pinned;
    int running;
    float progress;
    int show_progress;
    uint64_t launch_time;
    void (*on_launch)(const char *app_id);
    void (*on_focus)(const char *app_id);
    void (*on_right_click)(const char *app_id, int x, int y);
} dock_app_t;

typedef struct {
    dock_app_t apps[DOCK_MAX_APPS];
    int app_count;
    int icon_size;
    dock_position_t position;
    int x, y;
    int width, height;
    int autohide;
    int autohide_enabled;
    uint32_t autohide_delay;
    int autohide_size;
    int autohide_visible;
    int autohide_timer_active;
    uint64_t autohide_start;
    dock_anim_t anim_type;
    int visible;
    int focused_app;
    int drag_source_idx;
    int drag_target_idx;
    int dragging;
    int drag_x, drag_y;
    layer_t *surface_layer;
    surface_t *bg_surface;
    surface_t *icon_surfaces[DOCK_MAX_APPS];
} dock_t;

int dock_init(void);
void dock_shutdown(void);
int dock_add_app(const char *app_id, const char *name, int pinned);
int dock_remove_app(const char *app_id);
int dock_set_autohide(int enabled, uint32_t delay, int size);
int dock_set_position(dock_position_t position);
int dock_set_size(int px);
void dock_set_running(const char *app_id, int running);
void dock_set_progress(const char *app_id, float progress);
void dock_render(void);
int dock_handle_touch(int x, int y, int action);
int dock_handle_click(int x, int y);
int dock_handle_right_click(int x, int y);
void dock_update_position(void);
dock_t *dock_get_instance(void);

#endif /* _DOCK_H_ */

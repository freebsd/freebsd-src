/*
 * Desktop Animations for uOS(m)
 * Spring physics, easing, requestAnimationFrame-style scheduling
 */

#ifndef _ANIMATIONS_H_
#define _ANIMATIONS_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/anim_window.h"

#define ANIM_MAX_ACTIVE       64
#define ANIM_DEFAULT_DURATION 300
#define ANIM_SPRING_TENSION    0.3f
#define ANIM_SPRING_FRICTION   0.8f

typedef enum {
    ANIM_WINDOW_OPEN,
    ANIM_WINDOW_CLOSE,
    ANIM_WINDOW_MINIMIZE,
    ANIM_WINDOW_RESTORE,
    ANIM_WORKSPACE_SWITCH,
    ANIM_NOTIFICATION_IN,
    ANIM_NOTIFICATION_OUT,
    ANIM_PANEL_SHOW,
    ANIM_PANEL_HIDE,
    ANIM_DOCK_SHOW,
    ANIM_DOCK_HIDE,
    ANIM_OVERLAY_FADE,
    ANIM_CUSTOM
} desktop_anim_type_t;

typedef struct {
    float position_x, position_y;
    float scale_x, scale_y;
    float opacity;
    float rotation;
} anim_state_t;

typedef struct {
    desktop_anim_type_t type;
    void *target;
    float start_x, start_y;
    float end_x, end_y;
    float start_scale_x, start_scale_y;
    float end_scale_x, end_scale_y;
    float start_opacity, end_opacity;
    uint64_t start_time;
    uint64_t duration;
    int active;
    int complete;
    void (*on_complete)(void *target);
    int easing;
    float spring_tension;
    float spring_friction;
    float spring_velocity_x;
    float spring_velocity_y;
    float spring_velocity_scale;
    anim_state_t current;
    anim_state_t rest_anchor;
} desktop_anim_t;

typedef struct {
    desktop_anim_t animations[ANIM_MAX_ACTIVE];
    int count;
    uint64_t last_frame_time;
    int frame_scheduled;
} anim_engine_t;

int desktop_anim_init(void);
void desktop_anim_shutdown(void);
desktop_anim_t *anim_window_open(int x, int y, int w, int h);
desktop_anim_t *anim_window_close(int x, int y, int w, int h);
desktop_anim_t *anim_window_minimize(int x, int y, int dock_y);
desktop_anim_t *anim_window_restore(int x, int y, int w, int h);
desktop_anim_t *anim_workspace_switch(int from_x, int to_x, int direction);
desktop_anim_t *anim_notification_in(int x, int y, int w, int h);
desktop_anim_t *anim_notification_out(int x, int y, int w, int h);
desktop_anim_t *anim_panel_show(int y);
desktop_anim_t *anim_panel_hide(int y);
void desktop_anim_update(void);
void desktop_anim_render_all(void);
float desktop_anim_ease(float t, int easing_type);
float desktop_anim_spring_step(float current, float target, float velocity, float tension, float friction, float dt);

#endif /* _ANIMATIONS_H_ */

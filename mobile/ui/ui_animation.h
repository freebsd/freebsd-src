/*
 * Animation System for Mobile UI
 * uOS(m) - User OS Mobile
 * iOS-style smooth animations with easing and spring physics
 */

#ifndef _UI_ANIMATION_H_
#define _UI_ANIMATION_H_

#include <stdint.h>
#include "ui_widget.h"
#include "window_manager.h"

#define MAX_ANIMATIONS 128
#define ANIMATION_FPS 60
#define ANIMATION_FRAME_TIME (1000 / ANIMATION_FPS)  // ~16.67ms

/* Animation types */
typedef enum {
    ANIM_TYPE_POSITION,     /* Move widget/window */
    ANIM_TYPE_SIZE,         /* Resize widget/window */
    ANIM_TYPE_OPACITY,      /* Fade in/out */
    ANIM_TYPE_SCALE,        /* Scale transformation */
    ANIM_TYPE_ROTATION,     /* Rotation transformation */
    ANIM_TYPE_COLOR         /* Color transition */
} animation_type_t;

/* Easing functions (iOS-style) */
typedef enum {
    EASING_LINEAR,
    EASING_EASE_IN_QUAD,
    EASING_EASE_OUT_QUAD,
    EASING_EASE_IN_OUT_QUAD,
    EASING_EASE_IN_CUBIC,
    EASING_EASE_OUT_CUBIC,
    EASING_EASE_IN_OUT_CUBIC,
    EASING_EASE_IN_QUART,
    EASING_EASE_OUT_QUART,
    EASING_EASE_IN_OUT_QUART,
    EASING_EASE_IN_SINE,
    EASING_EASE_OUT_SINE,
    EASING_EASE_IN_OUT_SINE,
    EASING_SPRING,          /* iOS spring animation */
    EASING_BOUNCE,
    EASING_ELASTIC
} easing_type_t;

/* Animation state */
typedef enum {
    ANIM_STATE_IDLE,
    ANIM_STATE_RUNNING,
    ANIM_STATE_PAUSED,
    ANIM_STATE_COMPLETED,
    ANIM_STATE_CANCELLED
} animation_state_t;

/* Animation value types */
typedef union {
    int i;              /* Integer values */
    float f;            /* Float values */
    uint32_t color;     /* Color values */
} anim_value_t;

/* Animation target */
typedef struct {
    void *object;           /* Widget or window pointer */
    int is_widget;          /* 1 for widget, 0 for window */
    animation_type_t type;  /* What property to animate */
} anim_target_t;

/* Animation structure */
typedef struct animation {
    uint32_t id;
    anim_target_t target;

    /* Start and end values */
    anim_value_t start_value;
    anim_value_t end_value;

    /* Timing */
    uint64_t start_time;
    uint64_t duration;      /* in milliseconds */
    uint64_t delay;         /* delay before starting */

    /* Animation parameters */
    easing_type_t easing;
    float spring_damping;   /* For spring animations */
    float spring_stiffness; /* For spring animations */

    /* State */
    animation_state_t state;
    float progress;         /* 0.0 to 1.0 */

    /* Callbacks */
    void (*on_complete)(struct animation *anim);
    void (*on_update)(struct animation *anim, float progress);

    /* Internal */
    anim_value_t current_value;
    struct animation *next;
} animation_t;

/* Initialize animation system */
int ui_anim_init(void);

/* Create animations */
animation_t *ui_anim_create_position(widget_t *widget, int start_x, int start_y, int end_x, int end_y, uint64_t duration);
animation_t *ui_anim_create_size(widget_t *widget, int start_w, int start_h, int end_w, int end_h, uint64_t duration);
animation_t *ui_anim_create_opacity(widget_t *widget, float start_opacity, float end_opacity, uint64_t duration);
animation_t *ui_anim_create_scale(widget_t *widget, float start_scale, float end_scale, uint64_t duration);
animation_t *ui_anim_create_color(widget_t *widget, uint32_t start_color, uint32_t end_color, uint64_t duration);

/* Window animations */
animation_t *ui_anim_create_window_position(window_t *window, int start_x, int start_y, int end_x, int end_y, uint64_t duration);
animation_t *ui_anim_create_window_size(window_t *window, int start_w, int start_h, int end_w, int end_h, uint64_t duration);

/* Animation control */
void ui_anim_start(animation_t *anim);
void ui_anim_pause(animation_t *anim);
void ui_anim_resume(animation_t *anim);
void ui_anim_cancel(animation_t *anim);
void ui_anim_set_easing(animation_t *anim, easing_type_t easing);
void ui_anim_set_spring_params(animation_t *anim, float damping, float stiffness);

/* Animation groups and sequences */
typedef struct anim_group {
    animation_t **animations;
    int count;
    int parallel;  /* 1 for parallel, 0 for sequential */
    void (*on_complete)(struct anim_group *group);
} anim_group_t;

anim_group_t *ui_anim_create_group(animation_t **animations, int count, int parallel);
void ui_anim_start_group(anim_group_t *group);

/* Main animation loop (call this regularly) */
void ui_anim_update(void);

/* Utility functions */
float ui_anim_ease(easing_type_t type, float t);
uint32_t ui_anim_lerp_color(uint32_t start, uint32_t end, float t);

/* iOS-style preset animations */
animation_t *ui_anim_fade_in(widget_t *widget, uint64_t duration);
animation_t *ui_anim_fade_out(widget_t *widget, uint64_t duration);
animation_t *ui_anim_slide_in_left(widget_t *widget, uint64_t duration);
animation_t *ui_anim_slide_out_right(widget_t *widget, uint64_t duration);
animation_t *ui_anim_bounce_in(widget_t *widget, uint64_t duration);
animation_t *ui_anim_spring_scale(widget_t *widget, float target_scale, uint64_t duration);

#endif /* _UI_ANIMATION_H_ */
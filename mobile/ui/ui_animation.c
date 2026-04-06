/*
 * Animation System Implementation
 * uOS(m) - User OS Mobile
 * iOS-style smooth animations with physics-based spring animations
 */

#include "ui_animation.h"
#include "ui_widget.h"
#include "window_manager.h"
#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"

static animation_t *anim_list = NULL;
static uint32_t next_anim_id = 1;

/* Get current time in milliseconds (simplified) */
static uint64_t get_current_time(void) {
    static uint64_t time = 0;
    return time++;  // Simplified - in real implementation, use system timer
}

/* Initialize animation system */
int ui_anim_init(void) {
    anim_list = NULL;
    next_anim_id = 1;
    return 0;
}

/* Create animation structure */
static animation_t *create_animation(anim_target_t target, anim_value_t start, anim_value_t end, uint64_t duration) {
    animation_t *anim = (animation_t *)mem_alloc(sizeof(animation_t));
    if (!anim) return NULL;

    memset(anim, 0, sizeof(animation_t));

    anim->id = next_anim_id++;
    anim->target = target;
    anim->start_value = start;
    anim->end_value = end;
    anim->current_value = start;
    anim->duration = duration;
    anim->state = ANIM_STATE_IDLE;
    anim->progress = 0.0f;
    anim->easing = EASING_EASE_OUT_QUART;  // iOS default
    anim->spring_damping = 0.8f;
    anim->spring_stiffness = 0.3f;

    return anim;
}

/* Add animation to list */
static void add_animation(animation_t *anim) {
    anim->next = anim_list;
    anim_list = anim;
}

/* Remove animation from list */
static void remove_animation(animation_t *anim) {
    if (anim_list == anim) {
        anim_list = anim->next;
        return;
    }

    animation_t *current = anim_list;
    while (current && current->next != anim) {
        current = current->next;
    }

    if (current) {
        current->next = anim->next;
    }
}

/* Easing functions - iOS style */
float ui_anim_ease(easing_type_t type, float t) {
    switch (type) {
        case EASING_LINEAR:
            return t;

        case EASING_EASE_IN_QUAD:
            return t * t;

        case EASING_EASE_OUT_QUAD:
            return t * (2 - t);

        case EASING_EASE_IN_OUT_QUAD:
            return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;

        case EASING_EASE_IN_CUBIC:
            return t * t * t;

        case EASING_EASE_OUT_CUBIC:
            return (--t) * t * t + 1;

        case EASING_EASE_IN_OUT_CUBIC:
            return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;

        case EASING_EASE_IN_QUART:
            return t * t * t * t;

        case EASING_EASE_OUT_QUART:
            return 1 - (--t) * t * t * t;

        case EASING_EASE_IN_OUT_QUART:
            return t < 0.5f ? 8 * t * t * t * t : 1 - 8 * (--t) * t * t * t;

        case EASING_EASE_IN_SINE:
            return t * t * (3 - 2 * t);

        case EASING_EASE_OUT_SINE:
            return 1 - (1 - t) * (1 - t) * (1 - t);

        case EASING_EASE_IN_OUT_SINE:
            if (t < 0.5f) {
                return 4 * t * t * (3 - 2 * t);
            } else {
                float u = 1 - t;
                return 1 - u * u * u * (3 - 2 * u);
            }

        case EASING_SPRING: {
            float overshoot = 1.2f;
            return t * t * ((overshoot + 1) * t - overshoot);
        }

        case EASING_BOUNCE: {
            if (t < 1/2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2/2.75f) {
                t -= 1.5f/2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f/2.75f) {
                t -= 2.25f/2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f/2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
        }

        case EASING_ELASTIC: {
            float overshoot = 1.1f;
            return t * t * ((overshoot + 1) * t - overshoot);
        }

        default:
            return t;
    }
}

/* Linear interpolation between two values */
static float lerp(float start, float end, float t) {
    return start + (end - start) * t;
}

/* Color interpolation */
uint32_t ui_anim_lerp_color(uint32_t start, uint32_t end, float t) {
    uint8_t r1 = (start >> 16) & 0xFF;
    uint8_t g1 = (start >> 8) & 0xFF;
    uint8_t b1 = start & 0xFF;
    uint8_t a1 = (start >> 24) & 0xFF;

    uint8_t r2 = (end >> 16) & 0xFF;
    uint8_t g2 = (end >> 8) & 0xFF;
    uint8_t b2 = end & 0xFF;
    uint8_t a2 = (end >> 24) & 0xFF;

    uint8_t r = (uint8_t)lerp(r1, r2, t);
    uint8_t g = (uint8_t)lerp(g1, g2, t);
    uint8_t b = (uint8_t)lerp(b1, b2, t);
    uint8_t a = (uint8_t)lerp(a1, a2, t);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* Apply animation value to target */
static void apply_animation_value(animation_t *anim) {
    if (!anim->target.object) return;

    switch (anim->target.type) {
        case ANIM_TYPE_POSITION: {
            int x = anim->current_value.i;
            int y = (anim->current_value.i >> 16) & 0xFFFF;
            if (anim->target.is_widget) {
                ui_set_widget_position((widget_t *)anim->target.object, x, y);
            } else {
                wm_move_window((window_t *)anim->target.object, x, y);
            }
            break;
        }

        case ANIM_TYPE_SIZE: {
            int w = anim->current_value.i;
            int h = (anim->current_value.i >> 16) & 0xFFFF;
            if (anim->target.is_widget) {
                ui_set_widget_size((widget_t *)anim->target.object, w, h);
            } else {
                wm_resize_window((window_t *)anim->target.object, w, h);
            }
            break;
        }

        case ANIM_TYPE_OPACITY: {
            // For now, we'll use color alpha channel
            if (anim->target.is_widget) {
                widget_t *widget = (widget_t *)anim->target.object;
                uint32_t color = widget->bg_color;
                uint8_t alpha = (uint8_t)(anim->current_value.f * 255);
                widget->bg_color = (color & 0x00FFFFFF) | (alpha << 24);
            }
            break;
        }

        case ANIM_TYPE_SCALE: {
            // Scale affects size
            if (anim->target.is_widget) {
                widget_t *widget = (widget_t *)anim->target.object;
                int base_width = 100;  // Assume base size
                int base_height = 40;
                int new_width = (int)(base_width * anim->current_value.f);
                int new_height = (int)(base_height * anim->current_value.f);
                ui_set_widget_size(widget, new_width, new_height);
            }
            break;
        }

        case ANIM_TYPE_COLOR: {
            if (anim->target.is_widget) {
                widget_t *widget = (widget_t *)anim->target.object;
                widget->bg_color = anim->current_value.color;
            }
            break;
        }

        default:
            break;
    }
}

/* Update animation */
static void update_animation(animation_t *anim) {
    if (anim->state != ANIM_STATE_RUNNING) return;

    uint64_t current_time = get_current_time();
    uint64_t elapsed = current_time - anim->start_time;

    if (elapsed < anim->delay) return;

    elapsed -= anim->delay;
    anim->progress = (float)elapsed / (float)anim->duration;

    if (anim->progress >= 1.0f) {
        anim->progress = 1.0f;
        anim->state = ANIM_STATE_COMPLETED;
    }

    // Apply easing
    float eased_progress = ui_anim_ease(anim->easing, anim->progress);

    // Interpolate values
    switch (anim->target.type) {
        case ANIM_TYPE_POSITION:
        case ANIM_TYPE_SIZE:
            anim->current_value.i = (int)lerp(anim->start_value.i, anim->end_value.i, eased_progress);
            break;

        case ANIM_TYPE_OPACITY:
        case ANIM_TYPE_SCALE:
            anim->current_value.f = lerp(anim->start_value.f, anim->end_value.f, eased_progress);
            break;

        case ANIM_TYPE_COLOR:
            anim->current_value.color = ui_anim_lerp_color(anim->start_value.color, anim->end_value.color, eased_progress);
            break;

        default:
            break;
    }

    // Apply to target
    apply_animation_value(anim);

    // Call update callback
    if (anim->on_update) {
        anim->on_update(anim, anim->progress);
    }

    // Check completion
    if (anim->state == ANIM_STATE_COMPLETED) {
        if (anim->on_complete) {
            anim->on_complete(anim);
        }
        remove_animation(anim);
        mem_free(anim);
    }
}

/* Create position animation */
animation_t *ui_anim_create_position(widget_t *widget, int start_x, int start_y, int end_x, int end_y, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_POSITION};
    anim_value_t start = {.i = start_x | (start_y << 16)};
    anim_value_t end = {.i = end_x | (end_y << 16)};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

/* Create size animation */
animation_t *ui_anim_create_size(widget_t *widget, int start_w, int start_h, int end_w, int end_h, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_SIZE};
    anim_value_t start = {.i = start_w | (start_h << 16)};
    anim_value_t end = {.i = end_w | (end_h << 16)};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

/* Create opacity animation */
animation_t *ui_anim_create_opacity(widget_t *widget, float start_opacity, float end_opacity, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_OPACITY};
    anim_value_t start = {.f = start_opacity};
    anim_value_t end = {.f = end_opacity};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

/* Create scale animation */
animation_t *ui_anim_create_scale(widget_t *widget, float start_scale, float end_scale, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_SCALE};
    anim_value_t start = {.f = start_scale};
    anim_value_t end = {.f = end_scale};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

/* Create color animation */
animation_t *ui_anim_create_color(widget_t *widget, uint32_t start_color, uint32_t end_color, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_COLOR};
    anim_value_t start = {.color = start_color};
    anim_value_t end = {.color = end_color};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

/* Window animations */
animation_t *ui_anim_create_window_position(window_t *window, int start_x, int start_y, int end_x, int end_y, uint64_t duration) {
    anim_target_t target = {window, 0, ANIM_TYPE_POSITION};
    anim_value_t start = {.i = start_x | (start_y << 16)};
    anim_value_t end = {.i = end_x | (end_y << 16)};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_window_size(window_t *window, int start_w, int start_h, int end_w, int end_h, uint64_t duration) {
    anim_target_t target = {window, 0, ANIM_TYPE_SIZE};
    anim_value_t start = {.i = start_w | (start_h << 16)};
    anim_value_t end = {.i = end_w | (end_h << 16)};

    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

/* Animation control */
void ui_anim_start(animation_t *anim) {
    if (anim && anim->state == ANIM_STATE_IDLE) {
        anim->start_time = get_current_time();
        anim->state = ANIM_STATE_RUNNING;
    }
}

void ui_anim_pause(animation_t *anim) {
    if (anim && anim->state == ANIM_STATE_RUNNING) {
        anim->state = ANIM_STATE_PAUSED;
    }
}

void ui_anim_resume(animation_t *anim) {
    if (anim && anim->state == ANIM_STATE_PAUSED) {
        anim->state = ANIM_STATE_RUNNING;
    }
}

void ui_anim_cancel(animation_t *anim) {
    if (anim) {
        anim->state = ANIM_STATE_CANCELLED;
        remove_animation(anim);
        mem_free(anim);
    }
}

void ui_anim_set_easing(animation_t *anim, easing_type_t easing) {
    if (anim) anim->easing = easing;
}

void ui_anim_set_spring_params(animation_t *anim, float damping, float stiffness) {
    if (anim) {
        anim->spring_damping = damping;
        anim->spring_stiffness = stiffness;
    }
}

/* Animation groups */
anim_group_t *ui_anim_create_group(animation_t **animations, int count, int parallel) {
    anim_group_t *group = (anim_group_t *)mem_alloc(sizeof(anim_group_t));
    if (!group) return NULL;

    group->animations = (animation_t **)mem_alloc(sizeof(animation_t *) * count);
    if (!group->animations) {
        mem_free(group);
        return NULL;
    }

    memcpy(group->animations, animations, sizeof(animation_t *) * count);
    group->count = count;
    group->parallel = parallel;

    return group;
}

void ui_anim_start_group(anim_group_t *group) {
    if (!group) return;

    if (group->parallel) {
        // Start all animations at once
        for (int i = 0; i < group->count; i++) {
            ui_anim_start(group->animations[i]);
        }
    } else {
        // Start first animation, others will be chained
        if (group->count > 0) {
            ui_anim_start(group->animations[0]);
        }
    }
}

/* Main animation update loop */
void ui_anim_update(void) {
    animation_t *anim = anim_list;
    while (anim) {
        animation_t *next = anim->next;
        update_animation(anim);
        anim = next;
    }
}

/* iOS-style preset animations */
animation_t *ui_anim_fade_in(widget_t *widget, uint64_t duration) {
    return ui_anim_create_opacity(widget, 0.0f, 1.0f, duration);
}

animation_t *ui_anim_fade_out(widget_t *widget, uint64_t duration) {
    return ui_anim_create_opacity(widget, 1.0f, 0.0f, duration);
}

animation_t *ui_anim_slide_in_left(widget_t *widget, uint64_t duration) {
    int start_x = -widget->width;
    int end_x = widget->x;
    return ui_anim_create_position(widget, start_x, widget->y, end_x, widget->y, duration);
}

animation_t *ui_anim_slide_out_right(widget_t *widget, uint64_t duration) {
    int end_x = 1080 + widget->width;  // Screen width
    return ui_anim_create_position(widget, widget->x, widget->y, end_x, widget->y, duration);
}

animation_t *ui_anim_bounce_in(widget_t *widget, uint64_t duration) {
    animation_t *anim = ui_anim_create_scale(widget, 0.3f, 1.0f, duration);
    ui_anim_set_easing(anim, EASING_BOUNCE);
    return anim;
}

animation_t *ui_anim_spring_scale(widget_t *widget, float target_scale, uint64_t duration) {
    animation_t *anim = ui_anim_create_scale(widget, 1.0f, target_scale, duration);
    ui_anim_set_easing(anim, EASING_SPRING);
    return anim;
}
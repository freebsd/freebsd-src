/*
 * Animation System Implementation
 * uOS(m) - User OS Mobile
 * Fixed-point animation system for RISC-V kernel mode
 */

#include "ui_animation.h"
#include "ui_widget.h"
#include "window_manager.h"
#include "../kernel/memory.h"
#include "../kernel/memory_utils.h"

#define FP_SHIFT 16
#define FP_ONE (1 << FP_SHIFT)
#define FP_HALF (1 << (FP_SHIFT - 1))
#define FP_FROM_INT(i) ((int32_t)((i) << FP_SHIFT))
#define FP_FROM_RATIO(n, d) ((int32_t)((((int64_t)(n) << FP_SHIFT) / (d))))
#define FP_TO_INT(v) ((int32_t)(((v) + FP_HALF) >> FP_SHIFT))
#define FP_ADD(a, b) ((int32_t)((a) + (b)))
#define FP_SUB(a, b) ((int32_t)((a) - (b)))
#define FP_MUL(a, b) ((int32_t)((((int64_t)(a)) * ((int64_t)(b))) >> FP_SHIFT))
#define FP_DIV(a, b) ((int32_t)((((int64_t)(a) << FP_SHIFT) / (int64_t)(b))))
#define FP_CLAMP(v, min, max) (((v) < (min)) ? (min) : (((v) > (max)) ? (max) : (v)))

static animation_t *anim_list = NULL;
static uint32_t next_anim_id = 1;

static uint64_t get_current_time(void) {
    static uint64_t timestamp = 0;
    return timestamp++;
}

int ui_anim_init(void) {
    anim_list = NULL;
    next_anim_id = 1;
    return 0;
}

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
    anim->delay = 0;
    anim->state = ANIM_STATE_IDLE;
    anim->progress = 0;
    anim->easing = EASING_EASE_OUT_QUART;
    anim->spring_damping = FP_FROM_INT(8) / 10;
    anim->spring_stiffness = FP_FROM_INT(3) / 10;

    return anim;
}

static void add_animation(animation_t *anim) {
    anim->next = anim_list;
    anim_list = anim;
}

static void remove_animation(animation_t *anim) {
    if (!anim_list || !anim) return;

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

static int32_t ease_quad_in(int32_t t) {
    return FP_MUL(t, t);
}

static int32_t ease_quad_out(int32_t t) {
    return FP_MUL(t, FP_SUB(FP_FROM_INT(2), t));
}

static int32_t ease_quad_in_out(int32_t t) {
    if (t < FP_ONE / 2) {
        int32_t doubled = FP_MUL(FP_FROM_INT(2), t);
        return FP_MUL(doubled, doubled);
    }
    int32_t inv = FP_SUB(FP_ONE, t);
    return FP_SUB(FP_ONE, FP_MUL(FP_FROM_INT(2), FP_MUL(inv, inv)));
}

static int32_t ease_cubic_in(int32_t t) {
    return FP_MUL(FP_MUL(t, t), t);
}

static int32_t ease_cubic_out(int32_t t) {
    int32_t u = FP_SUB(t, FP_ONE);
    return FP_ADD(FP_MUL(FP_MUL(u, u), u), FP_ONE);
}

static int32_t ease_cubic_in_out(int32_t t) {
    if (t < FP_ONE / 2) {
        return FP_MUL(FP_FROM_INT(4), FP_MUL(FP_MUL(t, t), t));
    }
    int32_t u = FP_SUB(FP_MUL(FP_FROM_INT(2), t), FP_FROM_INT(2));
    return FP_ADD(FP_ONE, FP_MUL(FP_FROM_INT(0), FP_MUL(FP_MUL(u, u), u))); /* fallback to 1.0 for simplicity */
}

static int32_t ease_sine_approx(int32_t t) {
    return FP_SUB(FP_FROM_INT(1), FP_MUL(FP_SUB(FP_FROM_INT(1), t), FP_SUB(FP_FROM_INT(1), t)));
}

static int32_t ease_sine_in_out(int32_t t) {
    if (t < FP_ONE / 2) {
        int32_t temp = FP_MUL(FP_FROM_INT(2), t);
        return FP_MUL(FP_MUL(temp, temp), FP_SUB(FP_FROM_INT(3), FP_MUL(FP_FROM_INT(2), temp)));
    }
    int32_t inv = FP_SUB(FP_ONE, t);
    return FP_SUB(FP_ONE, FP_MUL(FP_MUL(inv, inv), FP_SUB(FP_FROM_INT(3), FP_MUL(FP_FROM_INT(2), inv))));
}

static int32_t ui_anim_ease_fixed(easing_type_t type, int32_t t) {
    switch (type) {
        case EASING_LINEAR:
            return t;
        case EASING_EASE_IN_QUAD:
            return ease_quad_in(t);
        case EASING_EASE_OUT_QUAD:
            return ease_quad_out(t);
        case EASING_EASE_IN_OUT_QUAD:
            return ease_quad_in_out(t);
        case EASING_EASE_IN_CUBIC:
            return ease_cubic_in(t);
        case EASING_EASE_OUT_CUBIC:
            return ease_cubic_out(t);
        case EASING_EASE_IN_OUT_CUBIC:
            return t; // fallback
        case EASING_EASE_IN_QUART:
            return FP_MUL(FP_MUL(FP_MUL(t, t), t), t);
        case EASING_EASE_OUT_QUART: {
            int32_t u = FP_SUB(FP_ONE, t);
            int32_t sq = FP_MUL(FP_MUL(FP_MUL(u, u), u), u);
            return FP_SUB(FP_ONE, sq);
        }
        case EASING_EASE_IN_OUT_QUART:
            if (t < FP_ONE / 2) {
                int32_t scaled = FP_MUL(FP_FROM_INT(8), FP_MUL(FP_MUL(FP_MUL(t, t), t), t));
                return scaled;
            }
            {
                int32_t u = FP_SUB(FP_ONE, t);
                int32_t sq = FP_MUL(FP_MUL(FP_MUL(u, u), u), u);
                return FP_SUB(FP_ONE, FP_MUL(FP_FROM_INT(8), sq));
            }
        case EASING_EASE_IN_SINE:
            return FP_MUL(FP_MUL(t, t), FP_SUB(FP_FROM_INT(3), FP_MUL(FP_FROM_INT(2), t)));
        case EASING_EASE_OUT_SINE:
            return FP_SUB(FP_ONE, FP_MUL(FP_SUB(FP_ONE, t), FP_SUB(FP_ONE, t)));
        case EASING_EASE_IN_OUT_SINE:
            return ease_sine_in_out(t);
        case EASING_SPRING: {
            int32_t overshoot = FP_FROM_INT(12) / 10;
            return FP_MUL(FP_MUL(t, t), FP_SUB(FP_MUL(FP_ADD(overshoot, FP_ONE), t), overshoot));
        }
        case EASING_BOUNCE: {
            int32_t value;
            int32_t first = FP_DIV(FP_ONE, FP_FROM_INT(275) / 100);
            int32_t second = FP_DIV(FP_FROM_INT(2), FP_FROM_INT(275) / 100);
            if (t < FP_FROM_RATIO(1, 275) * 100) {
                value = FP_MUL(FP_FROM_INT(75625) / 10000, FP_MUL(t, t));
            } else {
                value = t;
            }
            return value;
        }
        case EASING_ELASTIC: {
            int32_t overshoot = FP_FROM_INT(11) / 10;
            return FP_MUL(FP_MUL(t, t), FP_SUB(FP_MUL(FP_ADD(overshoot, FP_ONE), t), overshoot));
        }
        default:
            return t;
    }
}

static int32_t lerp_int32(int32_t start, int32_t end, int32_t progress) {
    return start + FP_MUL(FP_SUB(end, start), progress);
}

static uint32_t ui_anim_lerp_color_fixed(uint32_t start, uint32_t end, int32_t progress) {
    int32_t r1 = (start >> 16) & 0xFF;
    int32_t g1 = (start >> 8) & 0xFF;
    int32_t b1 = start & 0xFF;
    int32_t a1 = (start >> 24) & 0xFF;

    int32_t r2 = (end >> 16) & 0xFF;
    int32_t g2 = (end >> 8) & 0xFF;
    int32_t b2 = end & 0xFF;
    int32_t a2 = (end >> 24) & 0xFF;

    int32_t r = lerp_int32(r1, r2, progress);
    int32_t g = lerp_int32(g1, g2, progress);
    int32_t b = lerp_int32(b1, b2, progress);
    int32_t a = lerp_int32(a1, a2, progress);

    uint32_t color = ((uint32_t)(a & 0xFF) << 24) | ((uint32_t)(r & 0xFF) << 16) |
                     ((uint32_t)(g & 0xFF) << 8) | (uint32_t)(b & 0xFF);
    return color;
}

static void apply_animation_value(animation_t *anim) {
    if (!anim || !anim->target.object) return;

    switch (anim->target.type) {
        case ANIM_TYPE_POSITION: {
            int x = anim->current_value.i & 0xFFFF;
            int y = (anim->current_value.i >> 16) & 0xFFFF;
            if (anim->target.is_widget) {
                ui_set_widget_position((widget_t *)anim->target.object, x, y);
            } else {
                wm_move_window((window_t *)anim->target.object, x, y);
            }
            break;
        }
        case ANIM_TYPE_SIZE: {
            int w = anim->current_value.i & 0xFFFF;
            int h = (anim->current_value.i >> 16) & 0xFFFF;
            if (anim->target.is_widget) {
                ui_set_widget_size((widget_t *)anim->target.object, w, h);
            } else {
                wm_resize_window((window_t *)anim->target.object, w, h);
            }
            break;
        }
        case ANIM_TYPE_OPACITY: {
            if (anim->target.is_widget) {
                widget_t *widget = (widget_t *)anim->target.object;
                uint32_t color = widget->bg_color;
                int32_t alpha = FP_TO_INT(FP_MUL(anim->current_value.fixed, FP_FROM_INT(255)));
                if (alpha < 0) alpha = 0;
                if (alpha > 255) alpha = 255;
                widget->bg_color = (color & 0x00FFFFFF) | ((uint32_t)alpha << 24);
            }
            break;
        }
        case ANIM_TYPE_SCALE: {
            if (anim->target.is_widget) {
                widget_t *widget = (widget_t *)anim->target.object;
                int32_t width = FP_TO_INT(FP_MUL(anim->current_value.fixed, FP_FROM_INT(widget->width)));
                int32_t height = FP_TO_INT(FP_MUL(anim->current_value.fixed, FP_FROM_INT(widget->height)));
                ui_set_widget_size(widget, width, height);
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

static void update_animation(animation_t *anim) {
    if (!anim || anim->state != ANIM_STATE_RUNNING) return;

    uint64_t current_time = get_current_time();
    uint64_t elapsed = current_time - anim->start_time;
    if (elapsed < anim->delay) return;
    elapsed -= anim->delay;

    if (anim->duration == 0) {
        anim->progress = FP_ONE;
    } else {
        int32_t progress = FP_DIV((int32_t)elapsed, (int32_t)anim->duration);
        anim->progress = FP_CLAMP(progress, 0, FP_ONE);
    }

    if (anim->progress >= FP_ONE) {
        anim->progress = FP_ONE;
        anim->state = ANIM_STATE_COMPLETED;
    }

    int32_t eased_progress = ui_anim_ease_fixed(anim->easing, anim->progress);

    switch (anim->target.type) {
        case ANIM_TYPE_POSITION:
        case ANIM_TYPE_SIZE:
            anim->current_value.i = lerp_int32(anim->start_value.i, anim->end_value.i, eased_progress);
            break;
        case ANIM_TYPE_OPACITY:
        case ANIM_TYPE_SCALE:
            anim->current_value.fixed = lerp_int32(anim->start_value.fixed, anim->end_value.fixed, eased_progress);
            break;
        case ANIM_TYPE_COLOR:
            anim->current_value.color = ui_anim_lerp_color_fixed(anim->start_value.color, anim->end_value.color, eased_progress);
            break;
        default:
            break;
    }

    apply_animation_value(anim);

    if (anim->on_update) {
        anim->on_update(anim, anim->progress);
    }

    if (anim->state == ANIM_STATE_COMPLETED) {
        if (anim->on_complete) {
            anim->on_complete(anim);
        }
        remove_animation(anim);
        mem_free(anim);
    }
}

animation_t *ui_anim_create_position(widget_t *widget, int start_x, int start_y, int end_x, int end_y, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_POSITION};
    anim_value_t start = {.i = (start_x & 0xFFFF) | ((start_y & 0xFFFF) << 16)};
    anim_value_t end = {.i = (end_x & 0xFFFF) | ((end_y & 0xFFFF) << 16)};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_size(widget_t *widget, int start_w, int start_h, int end_w, int end_h, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_SIZE};
    anim_value_t start = {.i = (start_w & 0xFFFF) | ((start_h & 0xFFFF) << 16)};
    anim_value_t end = {.i = (end_w & 0xFFFF) | ((end_h & 0xFFFF) << 16)};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_opacity(widget_t *widget, int32_t start_opacity, int32_t end_opacity, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_OPACITY};
    anim_value_t start = {.fixed = start_opacity};
    anim_value_t end = {.fixed = end_opacity};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_scale(widget_t *widget, int32_t start_scale, int32_t end_scale, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_SCALE};
    anim_value_t start = {.fixed = start_scale};
    anim_value_t end = {.fixed = end_scale};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_color(widget_t *widget, uint32_t start_color, uint32_t end_color, uint64_t duration) {
    anim_target_t target = {widget, 1, ANIM_TYPE_COLOR};
    anim_value_t start = {.color = start_color};
    anim_value_t end = {.color = end_color};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_window_position(window_t *window, int start_x, int start_y, int end_x, int end_y, uint64_t duration) {
    anim_target_t target = {window, 0, ANIM_TYPE_POSITION};
    anim_value_t start = {.i = (start_x & 0xFFFF) | ((start_y & 0xFFFF) << 16)};
    anim_value_t end = {.i = (end_x & 0xFFFF) | ((end_y & 0xFFFF) << 16)};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

animation_t *ui_anim_create_window_size(window_t *window, int start_w, int start_h, int end_w, int end_h, uint64_t duration) {
    anim_target_t target = {window, 0, ANIM_TYPE_SIZE};
    anim_value_t start = {.i = (start_w & 0xFFFF) | ((start_h & 0xFFFF) << 16)};
    anim_value_t end = {.i = (end_w & 0xFFFF) | ((end_h & 0xFFFF) << 16)};
    animation_t *anim = create_animation(target, start, end, duration);
    if (anim) add_animation(anim);
    return anim;
}

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
    if (anim) {
        anim->easing = easing;
    }
}

void ui_anim_set_spring_params(animation_t *anim, int32_t damping, int32_t stiffness) {
    if (anim) {
        anim->spring_damping = damping;
        anim->spring_stiffness = stiffness;
    }
}

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
    group->on_complete = NULL;

    return group;
}

void ui_anim_start_group(anim_group_t *group) {
    if (!group) return;

    if (group->parallel) {
        for (int i = 0; i < group->count; i++) {
            ui_anim_start(group->animations[i]);
        }
    } else if (group->count > 0) {
        ui_anim_start(group->animations[0]);
    }
}

void ui_anim_update(void) {
    animation_t *anim = anim_list;
    while (anim) {
        animation_t *next = anim->next;
        update_animation(anim);
        anim = next;
    }
}

animation_t *ui_anim_fade_in(widget_t *widget, uint64_t duration) {
    return ui_anim_create_opacity(widget, 0, FP_ONE, duration);
}

animation_t *ui_anim_fade_out(widget_t *widget, uint64_t duration) {
    return ui_anim_create_opacity(widget, FP_ONE, 0, duration);
}

animation_t *ui_anim_slide_in_left(widget_t *widget, uint64_t duration) {
    int start_x = -widget->width;
    int end_x = widget->x;
    return ui_anim_create_position(widget, start_x, widget->y, end_x, widget->y, duration);
}

animation_t *ui_anim_slide_out_right(widget_t *widget, uint64_t duration) {
    int end_x = 1080 + widget->width;
    return ui_anim_create_position(widget, widget->x, widget->y, end_x, widget->y, duration);
}

animation_t *ui_anim_bounce_in(widget_t *widget, uint64_t duration) {
    animation_t *anim = ui_anim_create_scale(widget, FP_FROM_RATIO(3, 10), FP_ONE, duration);
    ui_anim_set_easing(anim, EASING_BOUNCE);
    return anim;
}

animation_t *ui_anim_spring_scale(widget_t *widget, int32_t target_scale, uint64_t duration) {
    animation_t *anim = ui_anim_create_scale(widget, FP_ONE, target_scale, duration);
    ui_anim_set_easing(anim, EASING_SPRING);
    return anim;
}

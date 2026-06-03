/*
 * Desktop Animations Implementation - Spring Physics
 */

#include "animations.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static anim_engine_t *g_anim = NULL;

static float cubic_bezier(float x1, float y1, float x2, float y2, float t) {
    float u = 1 - t;
    return 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t;
}

float desktop_anim_ease(float t, int easing_type) {
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    switch (easing_type) {
        case 0: return t;
        case 1: return t * t;
        case 2: return t * (2 - t);
        case 3: return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
        case 4: return t * t * t;
        case 5: return 1 - powf(1 - t, 3);
        case 6: return t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
        case 7: return t * t * t * t;
        case 8: return 1 - powf(1 - t, 4);
        case 12: return cubic_bezier(0.68, -0.55, 0.265, 1.55, t);
        default: return t;
    }
}

float desktop_anim_spring_step(float current, float target, float velocity, float tension, float friction, float dt) {
    float displacement = target - current;
    float spring_force = displacement * tension;
    float damping_force = -velocity * friction;
    float total_force = spring_force + damping_force;
    float new_velocity = velocity + total_force * dt;
    float new_position = current + new_velocity * dt;
    return new_position;
}

int desktop_anim_init(void) {
    if (g_anim) return 0;
    g_anim = calloc(1, sizeof(anim_engine_t));
    if (!g_anim) return -1;
    g_anim->last_frame_time = 0;
    g_anim->frame_scheduled = 0;
    return 0;
}

void desktop_anim_shutdown(void) {
    if (!g_anim) return;
    free(g_anim);
    g_anim = NULL;
}

desktop_anim_t *anim_window_open(int x, int y, int w, int h) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_WINDOW_OPEN;
    a->start_x = x + w / 2;
    a->start_y = y + h + 150;
    a->end_x = x;
    a->end_y = y;
    a->start_scale_x = 0.3f;
    a->start_scale_y = 0.3f;
    a->end_scale_x = 1.0f;
    a->end_scale_y = 1.0f;
    a->start_opacity = 0.0f;
    a->end_opacity = 1.0f;
    a->start_time = 0;
    a->duration = 350;
    a->active = 1;
    a->spring_tension = ANIM_SPRING_TENSION;
    a->spring_friction = ANIM_SPRING_FRICTION;
    a->rest_anchor.position_x = x;
    a->rest_anchor.position_y = y;
    a->rest_anchor.scale_x = 1.0f;
    a->rest_anchor.scale_y = 1.0f;
    a->rest_anchor.opacity = 1.0f;
    return a;
}

desktop_anim_t *anim_window_close(int x, int y, int w, int h) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_WINDOW_CLOSE;
    a->start_x = x;
    a->start_y = y;
    a->end_x = x + w / 2;
    a->end_y = y + h + 150;
    a->start_scale_x = 1.0f;
    a->start_scale_y = 1.0f;
    a->end_scale_x = 0.3f;
    a->end_scale_y = 0.3f;
    a->start_opacity = 1.0f;
    a->end_opacity = 0.0f;
    a->duration = 250;
    a->active = 1;
    a->rest_anchor.position_x = x;
    a->rest_anchor.position_y = y;
    return a;
}

desktop_anim_t *anim_window_minimize(int x, int y, int dock_y) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_WINDOW_CLOSE;
    a->start_x = x;
    a->start_y = y;
    a->end_x = FB_WIDTH / 2 - 50;
    a->end_y = dock_y;
    a->start_scale_x = 1.0f;
    a->start_scale_y = 1.0f;
    a->end_scale_x = 0.2f;
    a->end_scale_y = 0.2f;
    a->start_opacity = 1.0f;
    a->end_opacity = 0.0f;
    a->duration = 300;
    a->active = 1;
    return a;
}

desktop_anim_t *anim_window_restore(int x, int y, int w, int h) {
    return anim_window_open(x, y, w, h);
}

desktop_anim_t *anim_workspace_switch(int from_x, int to_x, int direction) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_WORKSPACE_SWITCH;
    a->start_x = from_x;
    a->end_x = to_x;
    a->duration = 300;
    a->active = 1;
    a->spring_tension = 0.2f;
    a->spring_friction = 0.75f;
    return a;
}

desktop_anim_t *anim_notification_in(int x, int y, int w, int h) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_NOTIFICATION_IN;
    a->start_x = FB_WIDTH;
    a->end_x = x;
    a->start_y = y;
    a->end_y = y;
    a->start_opacity = 0.0f;
    a->end_opacity = 1.0f;
    a->duration = 300;
    a->active = 1;
    a->easing = 2;
    return a;
}

desktop_anim_t *anim_notification_out(int x, int y, int w, int h) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_NOTIFICATION_OUT;
    a->start_x = x;
    a->end_x = FB_WIDTH;
    a->start_y = y;
    a->end_y = y;
    a->start_opacity = 1.0f;
    a->end_opacity = 0.0f;
    a->duration = 250;
    a->active = 1;
    a->easing = 1;
    return a;
}

desktop_anim_t *anim_panel_show(int y) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_PANEL_SHOW;
    a->start_y = -PANEL_HEIGHT;
    a->end_y = y;
    a->duration = 250;
    a->active = 1;
    a->easing = 3;
    return a;
}

desktop_anim_t *anim_panel_hide(int y) {
    if (!g_anim || g_anim->count >= ANIM_MAX_ACTIVE) return NULL;
    desktop_anim_t *a = &g_anim->animations[g_anim->count++];
    memset(a, 0, sizeof(*a));
    a->type = ANIM_PANEL_HIDE;
    a->start_y = y;
    a->end_y = -PANEL_HEIGHT;
    a->duration = 200;
    a->active = 1;
    a->easing = 1;
    return a;
}

void desktop_anim_update(void) {
    if (!g_anim) return;
    for (int i = 0; i < g_anim->count; i++) {
        desktop_anim_t *a = &g_anim->animations[i];
        if (!a->active) continue;
        float dt = 0.016f;
        float progress = a->duration > 0 ? (float)(a->start_time + a->duration > 0 ? 0.5f : 0.0f) : 1.0f;
        if (a->start_time == 0) a->start_time = 0;
        if (a->duration > 0) {
            float elapsed = (float)(a->start_time > 0 ? 16 : 0);
            progress = elapsed < (float)a->duration ? elapsed / (float)a->duration : 1.0f;
        }
        progress = progress < 0 ? 0 : (progress > 1 ? 1 : progress);
        float eased = desktop_anim_ease(progress, a->easing);
        a->current.position_x = a->start_x + (a->end_x - a->start_x) * eased;
        a->current.position_y = a->start_y + (a->end_y - a->start_y) * eased;
        a->current.scale_x = a->start_scale_x + (a->end_scale_x - a->start_scale_x) * eased;
        a->current.scale_y = a->start_scale_y + (a->end_scale_y - a->start_scale_y) * eased;
        a->current.opacity = a->start_opacity + (a->end_opacity - a->start_opacity) * eased;
        if (a->current.position_y != a->current.position_y) a->current.position_y = a->end_y;
        if (progress >= 1.0f || (!a->duration)) {
            a->current = a->rest_anchor;
            a->active = 0;
            a->complete = 1;
            if (a->on_complete) a->on_complete(a->target);
        }
    }
}

void desktop_anim_render_all(void) {
}

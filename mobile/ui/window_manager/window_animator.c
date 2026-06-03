/*
 * Window Animator Implementation
 * BSD-licensed
 */

#include "window_animator.h"
#include <stdlib.h>
#include <math.h>

static int g_anim_frame = 0;

void anim_init(void) {
    g_anim_frame = 0;
}

float anim_ease_in_out_cubic(float t) {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    }
    float f = (t - 1.0f);
    return 1.0f - f * f * f * f * f;
}

float anim_bezier_cubic(float x, float y, float t) {
    float u = 1.0f - t;
    return u * u * u * x + 3 * u * u * t * 0.25f + 3 * u * t * t * 0.75f + t * t * t * y;
}

window_animation_t *anim_create(int frames, anim_done_callback_t done, void *user_data) {
    window_animation_t *anim = calloc(1, sizeof(window_animation_t));
    if (!anim) return NULL;
    anim->total_frames = frames;
    anim->on_done = done;
    anim->user_data = user_data;
    anim->progress = 0;
    anim->active = 1;
    return anim;
}

window_animation_t *anim_open(void *window) {
    (void)window;
    return anim_create(ANIM_DURATION_OPEN, NULL, NULL);
}

window_animation_t *anim_close(void *window) {
    (void)window;
    return anim_create(ANIM_DURATION_CLOSE, NULL, NULL);
}

window_animation_t *anim_minimize(void *window) {
    (void)window;
    return anim_create(ANIM_DURATION_MINIMIZE, NULL, NULL);
}

window_animation_t *anim_restore(void *window) {
    (void)window;
    return anim_create(ANIM_DURATION_RESTORE, NULL, NULL);
}

void anim_update(window_animation_t *anim) {
    if (!anim || !anim->active) return;
    
    anim->current_frame++;
    float t = (float)anim->current_frame / (float)anim->total_frames;
    anim->progress = (int)(anim_ease_in_out_cubic(t) * 100);
    
    if (anim->current_frame >= anim->total_frames) {
        anim->active = 0;
        if (anim->on_done) anim->on_done(anim->user_data);
    }
}

int anim_is_complete(window_animation_t *anim) {
    return anim && !anim->active;
}

void anim_cancel(window_animation_t *anim) {
    if (anim) anim->active = 0;
}
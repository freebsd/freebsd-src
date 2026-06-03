/*
 * Window Animator - Window animations and transitions
 * BSD-licensed
 */

#ifndef _WINDOW_ANIMATOR_H_
#define _WINDOW_ANIMATOR_H_

#include <stdint.h>

#define ANIM_DURATION_OPEN      300
#define ANIM_DURATION_CLOSE     250
#define ANIM_DURATION_MINIMIZE 200
#define ANIM_DURATION_RESTORE   200
#define ANIM_FPS                60

typedef void (*anim_done_callback_t)(void *user_data);

typedef struct {
    int target_x, target_y;
    int target_width, target_height;
    int start_x, start_y;
    int start_width, start_height;
    int progress;
    int frame_count;
    int total_frames;
    int current_frame;
    int active;
    anim_done_callback_t on_done;
    void *user_data;
} window_animation_t;

void anim_init(void);

window_animation_t *anim_open(void *window, int x, int y, int width, int height);
window_animation_t *anim_close(void *window);
window_animation_t *anim_minimize(void *window);
window_animation_t *anim_restore(void *window, int x, int y, int width, int height);

void anim_update(window_animation_t *anim);
int anim_is_complete(window_animation_t *anim);
void anim_cancel(window_animation_t *anim);

float anim_ease_in_out_cubic(float t);
float anim_bezier_cubic(float x, float y, float t);

#endif /* _WINDOW_ANIMATOR_H_ */
/*
 * Touch Gesture Recognizer
 * BSD-licensed
 */

#ifndef _TOUCH_GST_H_
#define _TOUCH_GST_H_

#include <stdint.h>

#define GESTURE_TAP       1
#define GESTURE_DOUBLETAP 2
#define GESTURE_LONGPRESS 3
#define GESTURE_SWIPE_UP  4
#define GESTURE_SWIPE_DOWN 5
#define GESTURE_SWIPE_LEFT 6
#define GESTURE_SWIPE_RIGHT 7
#define GESTURE_PINCH     8

#define TOUCH_STATE_IDLE       0
#define TOUCH_STATE_DOWN       1
#define TOUCH_STATE_MOVED      2
#define TOUCH_STATE_RELEASED   3

#define DOUBLE_TAP_WINDOW_MS 300
#define LONG_PRESS_WINDOW_MS 500
#define SWIPE_THRESHOLD_DP  50

typedef void (*gesture_callback_t)(int gesture_type, int x, int y, void *user_data);

typedef struct {
    int x, y;
    int pressure;
    uint64_t start_time;
    uint64_t last_time;
    int state;
} touch_state_t;

typedef struct {
    touch_state_t points[10];
    int point_count;
    
    uint64_t last_tap_time;
    int tap_x, tap_y;
    
    gesture_callback_t on_tap;
    gesture_callback_t on_doubletap;
    gesture_callback_t on_longpress;
    gesture_callback_t on_swipe;
    gesture_callback_t on_pinch;
    void *user_data;
} gesture_recognizer_t;

int touch_gst_init(void);
void touch_gst_shutdown(void);

void touch_gst_handle_event(int type, int x, int y, int pressure);
void touch_gst_update(void);

void touch_gst_set_on_tap(gesture_callback_t cb);
void touch_gst_set_on_doubletap(gesture_callback_t cb);
void touch_gst_set_on_longpress(gesture_callback_t cb);
void touch_gst_set_on_swipe(gesture_callback_t cb);
void touch_gst_set_on_pinch(gesture_callback_t cb);

#endif /* _TOUCH_GST_H_ */
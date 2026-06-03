/*
 * Touch Gesture Recognizer Implementation
 * BSD-licensed
 */

#include "touch_gst.h"
#include <stdlib.h>
#include <string.h>

static gesture_recognizer_t g_gesture;

int touch_gst_init(void) {
    memset(&g_gesture, 0, sizeof(g_gesture));
    g_gesture.point_count = 0;
    return 0;
}

void touch_gst_shutdown(void) {
    memset(&g_gesture, 0, sizeof(g_gesture));
}

void touch_gst_handle_event(int type, int x, int y, int pressure) {
    if (g_gesture.point_count >= 10) return;
    
    touch_state_t *pt = &g_gesture.points[g_gesture.point_count];
    
    if (type == 1) {
        pt->x = x;
        pt->y = y;
        pt->pressure = pressure;
        pt->start_time = 0;
        pt->last_time = 0;
        pt->state = TOUCH_STATE_DOWN;
        g_gesture.point_count++;
    } else if (type == 3 && g_gesture.point_count > 0) {
        pt = &g_gesture.points[g_gesture.point_count - 1];
        pt->state = TOUCH_STATE_RELEASED;
    } else if (type == 2 && g_gesture.point_count > 0) {
        pt = &g_gesture.points[g_gesture.point_count - 1];
        pt->x = x;
        pt->y = y;
        pt->state = TOUCH_STATE_MOVED;
    }
}

void touch_gst_update(void) {
    for (int i = 0; i < g_gesture.point_count; i++) {
        touch_state_t *pt = &g_gesture.points[i];
        if (pt->state == TOUCH_STATE_DOWN) {
        } else if (pt->state == TOUCH_STATE_RELEASED) {
            uint64_t duration = 0;
            
            if (g_gesture.last_tap_time > 0 && 
                (pt->start_time - g_gesture.last_tap_time) < DOUBLE_TAP_WINDOW_MS &&
                (pt->x - g_gesture.tap_x) * (pt->x - g_gesture.tap_x) + 
                (pt->y - g_gesture.tap_y) * (pt->y - g_gesture.tap_y) < 100) {
                if (g_gesture.on_doubletap) {
                    g_gesture.on_doubletap(GESTURE_DOUBLETAP, pt->x, pt->y, g_gesture.user_data);
                }
            } else {
                if (duration < LONG_PRESS_WINDOW_MS && g_gesture.on_tap) {
                    g_gesture.on_tap(GESTURE_TAP, pt->x, pt->y, g_gesture.user_data);
                }
                
                g_gesture.last_tap_time = pt->start_time;
                g_gesture.tap_x = pt->x;
                g_gesture.tap_y = pt->y;
            }
            
            g_gesture.point_count--;
        }
    }
}

void touch_gst_set_on_tap(gesture_callback_t cb) {
    g_gesture.on_tap = cb;
}

void touch_gst_set_on_doubletap(gesture_callback_t cb) {
    g_gesture.on_doubletap = cb;
}

void touch_gst_set_on_longpress(gesture_callback_t cb) {
    g_gesture.on_longpress = cb;
}

void touch_gst_set_on_swipe(gesture_callback_t cb) {
    g_gesture.on_swipe = cb;
}

void touch_gst_set_on_pinch(gesture_callback_t cb) {
    g_gesture.on_pinch = cb;
}
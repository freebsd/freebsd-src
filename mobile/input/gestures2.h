#ifndef GESTURES2_H
#define GESTURES2_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GESTURE_NONE,
    GESTURE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_LONG_PRESS,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_PINCH_IN,
    GESTURE_PINCH_OUT,
    GESTURE_ROTATE_LEFT,
    GESTURE_ROTATE_RIGHT,
    GESTURE_EDGE_SWIPE_LEFT,
    GESTURE_EDGE_SWIPE_RIGHT,
    GESTURE_EDGE_SWIPE_UP,
    GESTURE_EDGE_SWIPE_DOWN,
    GESTURE_PULL_DOWN
} gesture_type_t;

typedef struct {
    gesture_type_t type;
    int x, y; /* position */
    int dx, dy; /* for swipe */
    float scale; /* for pinch */
    float angle; /* for rotate */
    int finger_count;
    timestamp_t timestamp; /* we'll use struct timespec */
} gesture_event_t;

typedef struct {
    /* state for each finger */
    int active_fingers;
    struct timespec down_time[5]; /* max 5 fingers */
    int down_x[5], down_y[5];
    int last_x[5], last_y[5];
    int edge_threshold; /* pixels from edge to consider edge swipe */
    int tap_max_move; /* max movement for tap */
    int tap_max_time; /* max time for tap in ms */
    int double_tap_max_time; /* max time between taps for double tap */
    int long_press_time; /* time for long press in ms */
    int swipe_min_distance; /* min distance for swipe */
    int swipe_max_time; /* max time for swipe in ms */
    float pinch_min_scale; /* min scale change for pinch */
    float rotate_min_angle; /* min angle change for rotate in degrees */
} g2_context_t;

int g2_init(g2_context_t *ctx, int width, int height);
void g2_free(g2_context_t *ctx);
gesture_event_t g2_process(g2_context_t *ctx, const touch_point_t *points, int point_count, struct timespec now);
const char *g2_gesture_name(gesture_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* GESTURES2_H */
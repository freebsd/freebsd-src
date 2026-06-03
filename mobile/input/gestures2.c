#include "gestures2.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef TIMESPEC_TO_TIMEVAL
#define TIMESPEC_TO_TIMEVAL(tv, ts) {                           \
    (tv)->tv_sec  = (ts)->tv_sec;                             \
    (tv)->tv_usec = (ts)->tv_nsec / 1000;                     \
}
#endif

#define MAX_FINGERS 5

int g2_init(g2_context_t *ctx, int width, int height)
{
    if (!ctx)
        return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->edge_threshold = width > height ? height / 10 : width / 10; /* 10% of smaller dimension */
    ctx->tap_max_move = 10; /* pixels */
    ctx->tap_max_time = 300; /* ms */
    ctx->double_tap_max_time = 300; /* ms */
    ctx->long_press_time = 500; /* ms */
    ctx->swipe_min_distance = 30; /* pixels */
    ctx->swipe_max_time = 500; /* ms */
    ctx->pinch_min_scale = 0.1; /* 10% change */
    ctx->rotate_min_angle = 10.0; /* degrees */
    return 0;
}

void g2_free(g2_context_t *ctx)
{
    if (!ctx)
        return;
    memset(ctx, 0, sizeof(*ctx));
}

static int is_edge(int x, int y, int width, int height, int edge_threshold)
{
    return (x < edge_threshold) || (x >= width - edge_threshold) ||
           (y < edge_threshold) || (y >= height - edge_threshold);
}

static int edge_direction(int x, int y, int width, int height, int edge_threshold)
{
    if (x < edge_threshold) return GESTURE_EDGE_SWIPE_LEFT;
    if (x >= width - edge_threshold) return GESTURE_EDGE_SWIPE_RIGHT;
    if (y < edge_threshold) return GESTURE_EDGE_SWIPE_UP;
    if (y >= height - edge_threshold) return GESTURE_EDGE_SWIPE_DOWN;
    return GESTURE_NONE;
}

static long long timespec_diff_ms(const struct timespec *a, const struct timespec *b)
{
    return (a->tv_sec - b->tv_sec) * 1000LL + (a->tv_nsec - b->tv_nsec) / 1000000LL;
}

gesture_event_t g2_process(g2_context_t *ctx, const touch_point_t *points, int point_count, struct timespec now)
{
    gesture_event_t gesture = { .type = GESTURE_NONE };
    if (!ctx || !points)
        return gesture;

    /* Update active fingers */
    for (int i = 0; i < point_count && i < MAX_FINGERS; i++) {
        const touch_point_t *tp = &points[i];
        int slot = tp->tracking_id; /* assuming tracking_id is the finger index */
        if (slot < 0 || slot >= MAX_FINGERS)
            continue;

        if (tp->state == 1) { /* down */
            ctx->down_time[slot] = now;
            ctx->down_x[slot] = tp->x;
            ctx->down_y[slot] = tp->y;
            ctx->last_x[slot] = tp->x;
            ctx->last_y[slot] = tp->y;
            ctx->active_fingers |= (1 << slot);
        } else if (tp->state == 0) { /* up */
            struct timespec down = ctx->down_time[slot];
            long long press_time = timespec_diff_ms(&now, &down);
            int dx = tp->x - ctx->down_x[slot];
            int dy = tp->y - ctx->down_y[slot];
            int distance = abs(dx) + abs(dy); /* Manhattan distance */

            /* Check for tap */
            if (press_time <= ctx->tap_max_time && distance <= ctx->tap_max_move) {
                /* Check for double tap */
                static struct timespec last_tap_time = {0};
                static int last_tap_x = 0, last_tap_y = 0;
                long long dt = timespec_diff_ms(&now, &last_tap_time);
                if (dt <= ctx->double_tap_max_time &&
                    abs(tp->x - last_tap_x) <= ctx->tap_max_move &&
                    abs(tp->y - last_tap_y) <= ctx->tap_max_move) {
                    gesture.type = GESTURE_DOUBLE_TAP;
                    gesture.x = tp->x;
                    gesture.y = tp->y;
                    gesture.finger_count = 1;
                    last_tap_time = now;
                    last_tap_x = tp->x;
                    last_tap_y = tp->y;
                    ctx->active_fingers &= ~(1 << slot);
                    return gesture;
                }
                last_tap_time = now;
                last_tap_x = tp->x;
                last_tap_y = tp->y;
                gesture.type = GESTURE_TAP;
                gesture.x = tp->x;
                gesture.y = tp->y;
                gesture.finger_count = 1;
            } else if (press_time >= ctx->long_press_time) {
                gesture.type = GESTURE_LONG_PRESS;
                gesture.x = tp->x;
                gesture.y = tp->y;
                gesture.finger_count = 1;
            } else {
                /* Swipe */
                if (press_time <= ctx->swipe_max_time) {
                    if (abs(dx) > ctx->swipe_min_distance && abs(dx) > abs(dy)) {
                        if (dx > 0) {
                            gesture.type = GESTURE_SWIPE_RIGHT;
                        } else {
                            gesture.type = GESTURE_SWIPE_LEFT;
                        }
                        gesture.dx = dx;
                        gesture.dy = dy;
                    } else if (abs(dy) > ctx->swipe_min_distance && abs(dy) > abs(dx)) {
                        if (dy > 0) {
                            gesture.type = GESTURE_SWIPE_DOWN;
                        } else {
                            gesture.type = GESTURE_SWIPE_UP;
                        }
                        gesture.dx = dx;
                        gesture.dy = dy;
                    }
                }
                if (gesture.type != GESTURE_NONE) {
                    gesture.x = tp->x;
                    gesture.y = tp->y;
                    gesture.finger_count = 1;
                }
            }
            ctx->active_fingers &= ~(1 << slot);
        } else if (tp->state == 2) { /* moving */
            ctx->last_x[slot] = tp->x;
            ctx->last_y[slot] = tp->y;
        }
    }

    /* Multi-finger gestures */
    int finger_count = 0;
    int slots[MAX_FINGERS];
    for (int i = 0; i < MAX_FINGERS; i++) {
        if (ctx->active_fingers & (1 << i)) {
            slots[finger_count++] = i;
        }
    }

    if (finger_count == 2) {
        /* Two-finger gestures: pinch and rotate */
        /* We need previous positions to compute delta, so we store them in last_x/y */
        int x0 = ctx->last_x[slots[0]];
        int y0 = ctx->last_y[slots[0]];
        int x1 = ctx->last_x[slots[1]];
        int y1 = ctx->last_y[slots[1]];
        int dx0 = x0 - ctx->down_x[slots[0]];
        int dy0 = y0 - ctx->down_y[slots[0]];
        int dx1 = x1 - ctx->down_x[slots[1]];
        int dy1 = y1 - ctx->down_y[slots[1]];

        /* Distance between fingers */
        int dist_down = sqrt((ctx->down_x[slots[0]] - ctx->down_x[slots[1]]) * (ctx->down_x[slots[0]] - ctx->down_x[slots[1]]) +
                             (ctx->down_y[slots[0]] - ctx->down_y[slots[1]]) * (ctx->down_y[slots[0]] - ctx->down_y[slots[1]]));
        int dist_now = sqrt((x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1));
        float scale = (float)dist_now / (float)dist_down;

        /* Angle */
        float angle_down = atan2(ctx->down_y[slots[0]] - ctx->down_y[slots[1]], ctx->down_x[slots[0]] - ctx->down_x[slots[1]]) * 180.0 / M_PI;
        float angle_now = atan2(y0 - y1, x0 - x1) * 180.0 / M_PI;
        float angle_diff = angle_now - angle_down;
        while (angle_diff < -180) angle_diff += 360;
        while (angle_diff > 180) angle_diff -= 360;

        if (fabs(scale - 1.0) > ctx->pinch_min_scale) {
            if (scale < 1.0) {
                gesture.type = GESTURE_PINCH_IN;
            } else {
                gesture.type = GESTURE_PINCH_OUT;
            }
            gesture.scale = scale;
            gesture.finger_count = 2;
        } else if (fabs(angle_diff) > ctx->rotate_min_angle) {
            if (angle_diff > 0) {
                gesture.type = GESTURE_ROTATE_RIGHT;
            } else {
                gesture.type = GESTURE_ROTATE_LEFT;
            }
            gesture.angle = angle_diff;
            gesture.finger_count = 2;
        }
    } else if (finger_count >= 3) {
        /* Three-finger gesture: we'll just note it */
        gesture.type = GESTURE_NONE; /* placeholder */
        gesture.finger_count = finger_count;
    }

    /* Edge swipe detection */
    if (finger_count == 1) {
        int slot = slots[0];
        int x = ctx->last_x[slot];
        int y = ctx->last_y[slot];
        int edge_dir = edge_direction(x, y, ctx->width, ctx->height, ctx->edge_threshold);
        if (edge_dir != GESTURE_NONE) {
            /* Check if started from edge and moved away */
            int start_x = ctx->down_x[slot];
            int start_y = ctx->down_y[slot];
            if (is_edge(start_x, start_y, ctx->width, ctx->height, ctx->edge_threshold)) {
                gesture.type = edge_dir;
                gesture.finger_count = 1;
            }
        }
    }

    /* Pull-down: start from top edge and move down */
    if (finger_count == 1) {
        int slot = slots[0];
        int start_y = ctx->down_y[slot];
        int now_y = ctx->last_y[slot];
        if (start_y < ctx->edge_threshold && /* started near top */
            (now_y - start_y) > ctx->swipe_min_distance) {
            gesture.type = GESTURE_PULL_DOWN;
            gesture.finger_count = 1;
            gesture.dy = now_y - start_y;
        }
    }

    return gesture;
}

const char *g2_gesture_name(gesture_type_t type)
{
    switch (type) {
        case GESTURE_NONE: return "NONE";
        case GESTURE_TAP: return "TAP";
        case GESTURE_DOUBLE_TAP: return "DOUBLE_TAP";
        case GESTURE_LONG_PRESS: return "LONG_PRESS";
        case GESTURE_SWIPE_LEFT: return "SWIPE_LEFT";
        case GESTURE_SWIPE_RIGHT: return "SWIPE_RIGHT";
        case GESTURE_SWIPE_UP: return "SWIPE_UP";
        case GESTURE_SWIPE_DOWN: return "SWIPE_DOWN";
        case GESTURE_PINCH_IN: return "PINCH_IN";
        case GESTURE_PINCH_OUT: return "PINCH_OUT";
        case GESTURE_ROTATE_LEFT: return "ROTATE_LEFT";
        case GESTURE_ROTATE_RIGHT: return "ROTATE_RIGHT";
        case GESTURE_EDGE_SWIPE_LEFT: return "EDGE_SWIPE_LEFT";
        case GESTURE_EDGE_SWIPE_RIGHT: return "EDGE_SWIPE_RIGHT";
        case GESTURE_EDGE_SWIPE_UP: return "EDGE_SWIPE_UP";
        case GESTURE_EDGE_SWIPE_DOWN: return "EDGE_SWIPE_DOWN";
        case GESTURE_PULL_DOWN: return "PULL_DOWN";
        default: return "UNKNOWN";
    }
}
#ifndef MULTITOUCH_H
#define MULTITOUCH_H

#include <stdint.h>
#include <linux/input.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int pressure;
    int tracking_id;
    int state; /* 0 = up, 1 = down, 2 = moving */
} touch_point_t;

typedef struct {
    touch_point_t *slots;
    int max_slots;
    int *slot_used; /* 1 if slot is in use */
    int *age; /* for tracking */
} mt_context_t;

int mt_init(mt_context_t *ctx, int max_slots);
void mt_free(mt_context_t *ctx);
int mt_handle_evdev(mt_context_t *ctx, const struct input_event *ev);
const touch_point_t *mt_get_slot(const mt_context_t *ctx, int slot);
int mt_get_active_touch_count(const mt_context_t *ctx);
void mt_get_two_finger_gesture(const mt_context_t *ctx, int *dx, int *dy, int *ddistance, int *dangle);
void mt_get_three_finger_centroid(const mt_context_t *ctx, int *x, int *y);
int mt_is_palm(const mt_context_t *ctx, int slot, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* MULTITOUCH_H */
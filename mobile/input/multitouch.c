#include "multitouch.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MT_SLOT 0x2f /* ABS_MT_SLOT */
#define MT_TRACKING_ID 0x39 /* ABS_MT_TRACKING_ID */
#define MT_POSITION_X 0x35 /* ABS_MT_POSITION_X */
#define MT_POSITION_Y 0x36 /* ABS_MT_POSITION_Y */
#define MT_PRESSURE 0x3a /* ABS_MT_PRESSURE */

int mt_init(mt_context_t *ctx, int max_slots)
{
    if (!ctx || max_slots <= 0)
        return -1;

    ctx->max_slots = max_slots;
    ctx->slots = calloc(max_slots, sizeof(touch_point_t));
    ctx->slot_used = calloc(max_slots, sizeof(int));
    ctx->age = calloc(max_slots, sizeof(int));
    if (!ctx->slots || !ctx->slot_used || !ctx->age) {
        mt_free(ctx);
        return -1;
    }
    return 0;
}

void mt_free(mt_context_t *ctx)
{
    if (!ctx)
        return;
    free(ctx->slots);
    free(ctx->slot_used);
    free(ctx->age);
    memset(ctx, 0, sizeof(*ctx));
}

int mt_handle_evdev(mt_context_t *ctx, const struct input_event *ev)
{
    if (!ctx || !ev)
        return -1;

    switch (ev->type) {
        case EV_ABS:
            switch (ev->code) {
                case MT_SLOT:
                    if (ev->value >= 0 && ev->value < ctx->max_slots) {
                        ctx->current_slot = ev->value;
                    }
                    break;
                case MT_TRACKING_ID:
                    if (ctx->current_slot >= 0 && ctx->current_slot < ctx->max_slots) {
                        if (ev->value == -1) {
                            ctx->slot_used[ctx->current_slot] = 0;
                        } else {
                            ctx->slot_used[ctx->current_slot] = 1;
                            ctx->slots[ctx->current_slot].tracking_id = ev->value;
                            ctx->slots[ctx->current_slot].state = 1; /* down */
                        }
                    }
                    break;
                case MT_POSITION_X:
                    if (ctx->current_slot >= 0 && ctx->current_slot < ctx->max_slots) {
                        ctx->slots[ctx->current_slot].x = ev->value;
                        ctx->slots[ctx->current_slot].state = 2; /* moving */
                    }
                    break;
                case MT_POSITION_Y:
                    if (ctx->current_slot >= 0 && ctx->current_slot < ctx->max_slots) {
                        ctx->slots[ctx->current_slot].y = ev->value;
                        ctx->slots[ctx->current_slot].state = 2; /* moving */
                    }
                    break;
                case MT_PRESSURE:
                    if (ctx->current_slot >= 0 && ctx->current_slot < ctx->max_slots) {
                        ctx->slots[ctx->current_slot].pressure = ev->value;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return 0;
}

const touch_point_t *mt_get_slot(const mt_context_t *ctx, int slot)
{
    if (!ctx || slot < 0 || slot >= ctx->max_slots)
        return NULL;
    if (!ctx->slot_used[slot])
        return NULL;
    return &ctx->slots[slot];
}

int mt_get_active_touch_count(const mt_context_t *ctx)
{
    if (!ctx)
        return 0;
    int count = 0;
    for (int i = 0; i < ctx->max_slots; i++) {
        if (ctx->slot_used[i])
            count++;
    }
    return count;
}

void mt_get_two_finger_gesture(const mt_context_t *ctx, int *dx, int *dy, int *ddistance, int *dangle)
{
    /* We need previous positions to compute deltas, so we store them */
    /* For simplicity, we'll compute current distance and angle and let caller compute delta */
    /* This function is a placeholder */
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    if (ddistance) *ddistance = 0;
    if (dangle) *dangle = 0;
}

void mt_get_three_finger_centroid(const mt_context_t *ctx, int *x, int *y)
{
    if (!ctx || !x || !y)
        return;
    int sum_x = 0, sum_y = 0, count = 0;
    for (int i = 0; i < ctx->max_slots; i++) {
        if (ctx->slot_used[i]) {
            sum_x += ctx->slots[i].x;
            sum_y += ctx->slots[i].y;
            count++;
        }
    }
    if (count > 0) {
        *x = sum_x / count;
        *y = sum_y / count;
    } else {
        *x = 0;
        *y = 0;
    }
}

int mt_is_palm(const mt_context_t *ctx, int slot, int width, int height)
{
    /* Simple heuristic: if touch is near edge and pressure is high, it's palm */
    if (!ctx || slot < 0 || slot >= ctx->max_slots || !ctx->slot_used[slot])
        return 0;
    const touch_point_t *tp = &ctx->slots[slot];
    /* Edge threshold: 10% of width/height */
    int edge_x = width / 10;
    int edge_y = height / 10;
    if (tp->x < edge_x || tp->x > width - edge_x ||
        tp->y < edge_y || tp->y > height - edge_y) {
        /* High pressure threshold */
        if (tp->pressure > 50) { /* arbitrary */
            return 1;
        }
    }
    return 0;
}
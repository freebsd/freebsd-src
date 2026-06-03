/*
 * Input Event System for uOS(m)
 * BSD-licensed touch/mouse/keyboard input handling
 */

#ifndef _INPUT_H_
#define _INPUT_H_

#include <stdint.h>
#include <stdatomic.h>

#define INPUT_RING_BUFFER_SIZE 128

#define EVT_TOUCH_DOWN    1
#define EVT_TOUCH_UP      2
#define EVT_TOUCH_MOVE    3
#define EVT_KEY_DOWN      4
#define EVT_KEY_UP        5

typedef struct {
    int32_t x;
    int32_t y;
    int32_t pressure;
    int32_t id;
} touch_point_t;

typedef struct {
    uint8_t type;
    uint64_t timestamp;
    union {
        touch_point_t touch;
        struct {
            int32_t keycode;
            int32_t modifiers;
        } key;
    } data;
} input_event_t;

typedef struct {
    input_event_t buffer[INPUT_RING_BUFFER_SIZE];
    atomic_uint head;
    atomic_uint tail;
    atomic_int active;
} input_ring_buffer_t;

int input_init(void);
void input_shutdown(void);
void input_poll(void);
int input_get_event(input_event_t *event);
void input_push_event(const input_event_t *event);

void input_translate_mouse_to_touch(int mouse_x, int mouse_y, int button_state);

#endif /* _INPUT_H_ */
/*
 * Input Event System Implementation
 * BSD-licensed
 */

#include "input.h"
#include <string.h>

static input_ring_buffer_t g_input_events;

int input_init(void) {
    atomic_init(&g_input_events.head, 0);
    atomic_init(&g_input_events.tail, 0);
    atomic_init(&g_input_events.active, 1);
    memset(g_input_events.buffer, 0, sizeof(g_input_events.buffer));
    return 0;
}

void input_shutdown(void) {
    atomic_store(&g_input_events.active, 0);
}

void input_poll(void) {
}

int input_get_event(input_event_t *event) {
    if (!event) return -1;
    
    unsigned int head = atomic_load(&g_input_events.head);
    unsigned int tail = atomic_load(&g_input_events.tail);
    
    if (head == tail) {
        return 0;
    }
    
    *event = g_input_events.buffer[tail];
    atomic_store(&g_input_events.tail, (tail + 1) % INPUT_RING_BUFFER_SIZE);
    return 1;
}

void input_push_event(const input_event_t *event) {
    if (!event || !atomic_load(&g_input_events.active)) return;
    
    unsigned int head = atomic_load(&g_input_events.head);
    unsigned int next_head = (head + 1) % INPUT_RING_BUFFER_SIZE;
    unsigned int tail = atomic_load(&g_input_events.tail);
    
    if (next_head == tail) {
        return;
    }
    
    g_input_events.buffer[head] = *event;
    atomic_store(&g_input_events.head, next_head);
}

void input_translate_mouse_to_touch(int mouse_x, int mouse_y, int button_state) {
    input_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.timestamp = 0;
    
    evt.data.touch.x = mouse_x;
    evt.data.touch.y = mouse_y;
    evt.data.touch.pressure = button_state ? 1024 : 0;
    evt.data.touch.id = 0;
    
    if (button_state) {
        evt.type = EVT_TOUCH_DOWN;
    } else {
        evt.type = EVT_TOUCH_UP;
    }
    
    input_push_event(&evt);
}
/*
 * UOS Compositor - Input Seat Management
 * BSD-licensed
 */

#ifndef _UOS_SEAT_H_
#define _UOS_SEAT_H_

#include <stdint.h>
#include <stdbool.h>
#include "protocols.h"

/* Seat capabilities */
#define WL_SEAT_POINTER    0x01
#define WL_SEAT_KEYBOARD   0x02
#define WL_SEAT_TOUCH      0x04

/* Pointer button state */
#define WL_POINTER_BUTTON_STATE_RELEASED 0
#define WL_POINTER_BUTTON_STATE_PRESSED   1

/* Key state */
#define WL_KEYBOARD_KEY_STATE_RELEASED 0
#define WL_KEYBOARD_KEY_STATE_PRESSED  1

/* Forward declaration */
struct uos_output;

/* Seat */
typedef struct uos_seat {
    struct wl_seat wl_seat;
    struct wl_list link;
    uint32_t capabilities;
    int32_t pointer_x;
    int32_t pointer_y;
    uint32_t pointer_enter_serial;
    struct wl_surface *pointer_focus;
    int32_t keyboard_enter_serial;
    struct wl_surface *keyboard_focus;
    int32_t touch_id;
    bool repeat_enabled;
    int32_t repeat_delay;
    int32_t repeat_rate;
    uint32_t repeat_key;
    int32_t repeat_timer_fd;
} uos_seat_t;

/* Seat public API */
int seat_init(uos_seat_t *seat);
void seat_deinit(uos_seat_t *seat);
void seat_set_capabilities(uos_seat_t *seat, uint32_t caps);
void seat_send_pointer_motion(uos_seat_t *seat, int32_t x, int32_t y);
void seat_send_pointer_enter(uos_seat_t *seat, struct wl_surface *surface, int32_t x, int32_t y);
void seat_send_pointer_leave(uos_seat_t *seat, struct wl_surface *surface);
void seat_send_button(uos_seat_t *seat, int32_t button, int32_t state);
void seat_send_axis(uos_seat_t *seat, int32_t axis, int32_t value);
void seat_send_key(uos_seat_t *seat, uint32_t key, uint32_t state);
void seat_handle_key_event(uos_seat_t *seat, uint32_t keycode, uint32_t state);
void seat_send_touch_down(uos_seat_t *seat, int32_t id, int32_t x, int32_t y);
void seat_send_touch_up(uos_seat_t *seat, int32_t id);
void seat_send_touch_motion(uos_seat_t *seat, int32_t id, int32_t x, int32_t y);
void seat_send_touch_frame(uos_seat_t *seat);
void seat_handle_event(int fd);

/* Repeat handling */
void seat_start_repeat(uos_seat_t *seat, uint32_t key);
void seat_stop_repeat(uos_seat_t *seat);
void seat_handle_repeat_timeout(uos_seat_t *seat);

#endif /* _UOS_SEAT_H_ */
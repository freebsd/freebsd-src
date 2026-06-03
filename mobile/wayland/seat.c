/*
 * UOS Compositor - Input Seat Implementation
 * BSD-licensed
 */

#include "seat.h"
#include "compositor.h"
#include "../input/evdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/timerfd.h>

static uos_seat_t *g_seat = NULL;

int seat_init(uos_seat_t *seat)
{
    memset(seat, 0, sizeof(*seat));
    wl_list_init(&seat->link);
    seat->capabilities = WL_SEAT_POINTER | WL_SEAT_KEYBOARD | WL_SEAT_TOUCH;
    g_seat = seat;
    return 0;
}

void seat_deinit(uos_seat_t *seat)
{
    g_seat = NULL;
}

void seat_set_capabilities(uos_seat_t *seat, uint32_t caps)
{
    seat->capabilities = caps;
}

void seat_send_pointer_motion(uos_seat_t *seat, int32_t x, int32_t y)
{
    seat->pointer_x = x;
    seat->pointer_y = y;
    /* Send wl_pointer.motion event to focused surface if any */
}

void seat_send_pointer_enter(uos_seat_t *seat, struct wl_surface *surface, int32_t x, int32_t y)
{
    seat->pointer_focus = surface;
    seat->pointer_x = x;
    seat->pointer_y = y;
}

void seat_send_pointer_leave(uos_seat_t *seat, struct wl_surface *surface)
{
    seat->pointer_focus = NULL;
}

void seat_send_button(uos_seat_t *seat, int32_t button, int32_t state)
{
    /* Send wl_pointer.button event */
    (void)button;
    (void)state;
}

void seat_send_axis(uos_seat_t *seat, int32_t axis, int32_t value)
{
    /* Send wl_pointer.axis event */
    (void)axis;
    (void)value;
}

void seat_send_key(uos_seat_t *seat, uint32_t key, uint32_t state)
{
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        seat_start_repeat(seat, key);
    } else {
        seat_stop_repeat(seat);
    }
    /* Send wl_keyboard.key event */
    (void)key;
}

void seat_handle_key_event(uos_seat_t *seat, uint32_t keycode, uint32_t state)
{
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        seat_start_repeat(seat, keycode);
    } else {
        seat_stop_repeat(seat);
    }
}

void seat_start_repeat(uos_seat_t *seat, uint32_t key)
{
    struct itimerspec timer;

    if (seat->repeat_enabled) {
        return;
    }

    seat->repeat_key = key;
    seat->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

    timer.it_value.tv_sec = seat->repeat_delay / 1000;
    timer.it_value.tv_nsec = (seat->repeat_delay % 1000) * 1000000;
    timer.it_interval.tv_sec = 1000 / seat->repeat_rate;
    timer.it_interval.tv_nsec = (1000 % seat->repeat_rate) * 1000000;

    timerfd_settime(seat->repeat_timer_fd, 0, &timer, NULL);
    seat->repeat_enabled = true;
}

void seat_stop_repeat(uos_seat_t *seat)
{
    if (seat->repeat_timer_fd >= 0) {
        close(seat->repeat_timer_fd);
        seat->repeat_timer_fd = -1;
    }
    seat->repeat_enabled = false;
}

void seat_handle_repeat_timeout(uos_seat_t *seat)
{
    /* Send repeated key press */
    seat_send_key(seat, seat->repeat_key, WL_KEYBOARD_KEY_STATE_PRESSED);
}

void seat_send_touch_down(uos_seat_t *seat, int32_t id, int32_t x, int32_t y)
{
    seat->touch_id = id;
    /* Send wl_touch.down event */
}

void seat_send_touch_up(uos_seat_t *seat, int32_t id)
{
    (void)id;
    seat->touch_id = -1;
    /* Send wl_touch.up event */
}

void seat_send_touch_motion(uos_seat_t *seat, int32_t id, int32_t x, int32_t y)
{
    (void)seat;
    (void)id;
    (void)x;
    (void)y;
}

void seat_send_touch_frame(uos_seat_t *seat)
{
    (void)seat;
    /* Send wl_touch.frame event */
}

void seat_handle_event(int fd)
{
    struct input_event ev;
    int ret;

    while ((ret = read(fd, &ev, sizeof(ev)) > 0)) {
        if (g_seat) {
            switch (ev.type) {
            case EV_REL:
                if (ev.code == REL_X || ev.code == REL_Y) {
                    seat_send_pointer_motion(g_seat, g_seat->pointer_x + ev.value, g_seat->pointer_y);
                }
                break;
            case EV_KEY:
                if (ev.code >= BTN_LEFT && ev.code <= BTN_TASK) {
                    seat_send_button(g_seat, ev.code, ev.value);
                } else {
                    seat_handle_key_event(g_seat, ev.code, ev.value);
                }
                break;
            case EV_ABS:
                if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_MT_POSITION_Y) {
                    seat_send_touch_motion(g_seat, g_seat->touch_id,
                                          ev.code == ABS_MT_POSITION_X ? ev.value : 0,
                                          ev.code == ABS_MT_POSITION_Y ? ev.value : 0);
                }
                break;
            }
        }
    }
}
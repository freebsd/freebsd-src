/*
 * Wayland Protocol Stubs - Minimal protocol definitions
 * BSD-licensed
 */

#ifndef _WAYLAND_PROTOCOLS_H_
#define _WAYLAND_PROTOCOLS_H_

#include <stdint.h>
#include <stdlib.h>

/* Forward declarations */
struct wl_resource;
struct wl_client;
struct wl_array;
struct wl_list;
struct wl_signal;

/* wl_resource - represents a protocol object */
struct wl_resource {
    uint32_t id;
    struct wl_client *client;
    const void *implementation;
    void *data;
    struct wl_list link;
    struct wl_signal destroy_signal;
};

/* wl_client - connected client */
struct wl_client {
    int fd;
    struct wl_list resource_list;
    struct wl_signal destroy_signal;
};

/* wl_array - dynamic array */
struct wl_array {
    size_t alloc;
    size_t size;
    uint8_t *data;
};

/* wl_list - linked list */
struct wl_list {
    struct wl_list *prev;
    struct wl_list *next;
};

/* wl_list helpers */
#define wl_list_init(list) ((list)->next = (list), (list)->prev = (list))
#define wl_list_insert(list, item) \
    do { \
        (item)->next = (list); \
        (item)->prev = (list)->prev; \
        (list)->prev->next = (item); \
        (list)->prev = (item); \
    } while (0)
#define wl_list_remove(item) \
    do { \
        (item)->next->prev = (item)->prev; \
        (item)->prev->next = (item)->next; \
    } while (0)
#define wl_list_for_each(pos, head, member) \
    for (pos = wl_container_of((head)->next, pos, member); \
         &pos->member != (head); \
         pos = wl_container_of(pos->member.next, pos, member))
#define wl_container_of(ptr, type, member) ({ \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); })

/* wl_signal - signal for event callbacks */
typedef void (*wl_notify_func_t)(struct wl_resource *resource, void *data);

struct wl_signal {
    struct wl_list listener_list;
};

/* wl_listener - callback listener */
typedef void (*wl_listener_func_t)(void *data);

struct wl_listener {
    struct wl_list link;
    wl_listener_func_t notify;
};

/* Event dispatch macros */
#define WL_EVENT_READ       0x01
#define WL_EVENT_WRITE      0x02
#define WL_EVENT_ERROR      0x04

/* wl_event_loop */
struct wl_event_loop {
    int fd;
};

/* wl_event_source */
typedef int (*wl_event_source_func_t)(int fd, uint32_t mask, void *data);

struct wl_event_source {
    struct wl_event_loop *loop;
    wl_event_source_func_t func;
    void *data;
};

/* wl_display - Wayland display server */
struct wl_display {
    struct wl_event_loop *event_loop;
    struct wl_list client_list;
    struct wl_list protocol_log;
    struct wl_array extra_mem;
};

/* wl_surface - basic surface */
struct wl_surface {
    struct wl_resource resource;
    struct wl_list frame_callback_list;
    int32_t width;
    int32_t height;
    int32_t sx;
    int32_t sy;
};

/* wl_region - input/output region */
struct wl_region {
    struct wl_resource resource;
    int32_t x, y;
    int32_t w, h;
};

/* wl_output - display output */
struct wl_output {
    struct wl_resource resource;
    int32_t x, y;
    int32_t width, height;
    int32_t refresh;
    int32_t transform;
    int32_t scale;
};

/* wl_seat - input seat */
struct wl_seat {
    struct wl_resource resource;
    uint32_t capabilities;
};

/* wl_pointer - mouse pointer */
struct wl_pointer {
    struct wl_resource resource;
    uint32_t enter_serial;
    struct wl_surface *focus_surface;
};

/* wl_keyboard - keyboard */
struct wl_keyboard {
    struct wl_resource resource;
};

/* wl_touch - touch input */
struct wl_touch {
    struct wl_resource resource;
};

/* wl_display API */
struct wl_display *wl_display_create(void);
void wl_display_destroy(struct wl_display *display);
struct wl_event_loop *wl_display_get_event_loop(struct wl_display *display);
int wl_display_add_socket_auto(struct wl_display *display);
struct wl_client *wl_client_create(struct wl_display *display, int fd);
void wl_client_destroy(struct wl_client *client);

/* wl_resource API */
struct wl_resource *wl_resource_create(struct wl_client *client,
                                    const void *interface,
                                    int version,
                                    uint32_t id);
void wl_resource_destroy(struct wl_resource *resource);
void wl_resource_set_user_data(struct wl_resource *resource, void *data);
void *wl_resource_get_user_data(struct wl_resource *resource);
uint32_t wl_resource_get_id(struct wl_resource *resource);

/* wl_event_loop API */
struct wl_event_loop *wl_event_loop_create(void);
void wl_event_loop_destroy(struct wl_event_loop *loop);
int wl_event_loop_add_fd(struct wl_event_loop *loop,
                       int fd,
                       uint32_t mask,
                       wl_event_source_func_t func,
                       void *data);
int wl_event_loop_add_timer(struct wl_event_loop *loop,
                          wl_event_source_func_t func,
                          void *data);

#endif /* _WAYLAND_PROTOCOLS_H_ */
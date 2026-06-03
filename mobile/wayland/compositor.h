/*
 * UOS Compositor - Core Wayland Compositor
 * BSD-licensed
 */

#ifndef _UOS_COMPOSITOR_H_
#define _UOS_COMPOSITOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "protocols.h"
#include "../gfx/drm.h"
#include "../gfx/gbm.h"
#include "../gfx/egl.h"
#include "../gfx/gl.h"

/* Backend types */
#define BACKEND_DRM  1

/* Window states */
#define WINDOW_STATE_NORMAL      0
#define WINDOW_STATE_MAXIMIZED   1
#define WINDOW_STATE_MINIMIZED   2
#define WINDOW_STATE_FULLSCREEN  4
#define WINDOW_STATE_RESIZING    8

/* Output transform */
#define OUTPUT_TRANSFORM_NORMAL        0
#define OUTPUT_TRANSFORM_90            1
#define OUTPUT_TRANSFORM_180           2
#define OUTPUT_TRANSFORM_270           3
#define OUTPUT_TRANSFORM_FLIPPED         4
#define OUTPUT_TRANSFORM_FLIPPED_90      5
#define OUTPUT_TRANSFORM_FLIPPED_180     6
#define OUTPUT_TRANSFORM_FLIPPED_270     7

/* Output */
typedef struct uos_output {
    struct wl_output wl_output;
    int32_t x, y;
    int32_t width, height;
    int32_t refresh;
    int32_t scale;
    int32_t transform;
    uint32_t crtc_id;
    uint32_t connector_id;
    struct wl_list link;
} uos_output_t;

/* Window */
typedef struct uos_window {
    struct wl_resource *xdg_toplevel;
    struct wl_resource *xdg_surface;
    struct wl_resource *surface;
    uos_output_t *output;
    int32_t x, y;
    int32_t width, height;
    int32_t border_width;
    uint32_t state;
    bool decorated;
    bool visible;
    int32_t z_order;
    char title[256];
    char app_id[64];
    struct wl_list link;
} uos_window_t;

/* Surface */
typedef struct uos_surface {
    struct wl_surface wl_surface;
    uint32_t texture;
    struct wl_resource *buffer;
    int32_t width, height;
    int32_t sx, sy;
    int32_t dirty;
    uos_window_t *window;
    struct wl_list frame_callback_list;
    struct wl_list link;
} uos_surface_t;

/* Damage ring - tracks damaged regions */
typedef struct damage_damage {
    int32_t x, y, w, h;
    struct wl_list link;
} damage_damage_t;

typedef struct comp_damage_ring {
    damage_damage_t *damages;
    int32_t count;
    int32_t capacity;
} comp_damage_ring_t;

/* Backend interface */
typedef struct uos_backend uos_backend_t;

typedef int (*backend_init_func_t)(uos_backend_t *backend, const char *card);
typedef void (*backend_deinit_func_t)(uos_backend_t *backend);
typedef struct uos_renderer *(*backend_get_renderer_func_t)(uos_backend_t *backend);
typedef struct uos_gbm_allocator *(*backend_get_allocator_func_t)(uos_backend_t *backend);
typedef int (*backend_get_output_func_t)(uos_backend_t *backend, uos_output_t *output);

struct uos_backend {
    int type;
    union {
        struct {
            struct drm_device *drm;
            struct gbm_device *gbm;
            struct drm_connector *connector;
            struct drm_crtc *crtc;
            struct drm_mode *mode;
        } drm;
    } u;
    struct uos_renderer *(*get_renderer)(uos_backend_t *backend);
    struct uos_gbm_allocator *(*get_allocator)(uos_backend_t *backend);
    int (*get_output)(uos_backend_t *backend, uos_output_t *output);
};

/* GBM allocator */
typedef struct uos_gbm_allocator {
    struct gbm_device *device;
    struct wl_list buffer_list;
} uos_gbm_allocator_t;

/* Renderer */
typedef struct uos_renderer {
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    gl_t gl;
    GLuint shader_program;
    GLuint vertex_shader;
    GLuint fragment_shader;
} uos_renderer_t;

/* Seat */
typedef struct uos_seat {
    struct wl_seat wl_seat;
    uos_output_t *output;
    int32_t pointer_x, pointer_y;
    uint32_t capabilities;
    int32_t repeat_timer_fd;
    uint32_t repeat_key;
    int32_t repeat_delay;
    int32_t repeat_rate;
    uint32_t keycode_to_repeat;
    struct wl_list link;
} uos_seat_t;

/* Compositor */
typedef struct uos_compositor {
    struct wl_display *display;
    struct wl_event_loop *event_loop;
    uos_backend_t backend;
    uos_renderer_t renderer;
    uos_gbm_allocator_t allocator;
    uos_output_t output;
    uos_seat_t seat;
    comp_damage_ring_t damage_ring;
    struct wl_list surface_list;
    struct wl_list window_list;
    int32_t running;
    int32_t frame_callback_count;
} uos_compositor_t;

/* Global compositor instance */
extern uos_compositor_t *g_compositor;

/* Core compositor API */
int comp_init(uos_backend_t *backend);
void comp_run(void);
void comp_shutdown(void);

uos_surface_t *comp_create_surface(struct wl_client *client, uint32_t id);
void comp_destroy_surface(uos_surface_t *surface);
uos_window_t *comp_create_toplevel(struct wl_client *client, uint32_t id, const char *title, int32_t w, int32_t h, const char *app_id);
void comp_destroy_toplevel(uos_window_t *window);

/* Damage ring API */
void comp_damage_ring_init(comp_damage_ring_t *ring);
void comp_damage_ring_add(comp_damage_ring_t *ring, int32_t x, int32_t y, int32_t w, int32_t h);
void comp_damage_ring_clear(comp_damage_ring_t *ring);

/* Frame scheduling */
void comp_schedule_frame(uos_surface_t *surface);
void comp_schedule_frame_callback(uos_surface_t *surface, uint32_t callback_id);

#endif /* _UOS_COMPOSITOR_H_ */
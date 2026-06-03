/*
 * UOS Compositor - Core Wayland Compositor Implementation
 * BSD-licensed
 */

#include "compositor.h"
#include "backend.h"
#include "renderer.h"
#include "xdg-shell.h"
#include "seat.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

uos_compositor_t *g_compositor = NULL;

static void compositor_add_toplevel(uos_window_t *window)
{
    uos_window_t *w;

    wl_list_for_each(w, &g_compositor->window_list, link) {
        if (w->z_order >= window->z_order) {
            window->z_order = w->z_order + 1;
        }
    }
    wl_list_insert(&g_compositor->window_list, &window->link);
}

static void compositor_destroy_surface(uos_surface_t *surface)
{
    if (surface->texture) {
        g_compositor->renderer.gl.glDeleteTextures(1, &surface->texture);
    }
    wl_resource_destroy(surface->wl_surface.resource);
}

uos_surface_t *comp_create_surface(struct wl_client *client, uint32_t id)
{
    uos_surface_t *surface;
    surface = calloc(1, sizeof(*surface));
    if (!surface) return NULL;

    wl_list_init(&surface->frame_callback_list);
    surface->wl_surface.resource = wl_resource_create(client, NULL, 1, id);
    surface->dirty = 1;

    wl_list_insert(&g_compositor->surface_list, &surface->link);
    return surface;
}

void comp_destroy_surface(uos_surface_t *surface)
{
    wl_list_remove(&surface->link);
    free(surface);
}

uos_window_t *comp_create_toplevel(struct wl_client *client, uint32_t id,
                                  const char *title, int32_t w, int32_t h,
                                  const char *app_id)
{
    uos_window_t *window;
    window = calloc(1, sizeof(*window));
    if (!window) return NULL;

    if (title) strncpy(window->title, title, sizeof(window->title) - 1);
    if (app_id) strncpy(window->app_id, app_id, sizeof(window->app_id) - 1);

    window->width = w;
    window->height = h;
    window->decorated = true;
    window->visible = true;
    window->z_order = g_compositor->window_list.next ?
                      ((uos_window_t *)g_compositor->window_list.prev)->z_order + 1 : 0;

    wl_list_insert(&g_compositor->window_list, &window->link);
    compositor_add_toplevel(window);

    return window;
}

void comp_damage_ring_init(comp_damage_ring_t *ring)
{
    ring->damages = NULL;
    ring->count = 0;
    ring->capacity = 0;
}

void comp_damage_ring_add(comp_damage_ring_t *ring, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (ring->count >= ring->capacity) {
        int new_cap = ring->capacity ? ring->capacity * 2 : 16;
        damage_damage_t *new_damages = realloc(ring->damages, new_cap * sizeof(*new_damages));
        if (!new_damages) return;
        ring->damages = new_damages;
        ring->capacity = new_cap;
    }
    ring->damages[ring->count].x = x;
    ring->damages[ring->count].y = y;
    ring->damages[ring->count].w = w;
    ring->damages[ring->count].h = h;
    ring->count++;
}

void comp_damage_ring_clear(comp_damage_ring_t *ring)
{
    ring->count = 0;
}

void comp_schedule_frame(uos_surface_t *surface)
{
    surface->dirty = 1;
    g_compositor->frame_callback_count++;
}

void comp_schedule_frame_callback(uos_surface_t *surface, uint32_t callback_id)
{
    /* Store callback for later delivery */
    (void)surface;
    (void)callback_id;
}

int comp_init(uos_backend_t *backend)
{
    g_compositor = calloc(1, sizeof(*g_compositor));
    if (!g_compositor) return -1;

    g_compositor->display = wl_display_create();
    if (!g_compositor->display) {
        free(g_compositor);
        return -1;
    }

    g_compositor->event_loop = wl_display_get_event_loop(g_compositor->display);

    /* Initialize backend */
    if (backend_drm_init(backend, "/dev/dri/card0") < 0) {
        wl_display_destroy(g_compositor->display);
        free(g_compositor);
        return -1;
    }

    /* Initialize renderer */
    if (renderer_init(&g_compositor->renderer, backend) < 0) {
        backend_deinit(backend);
        wl_display_destroy(g_compositor->display);
        free(g_compositor);
        return -1;
    }

    /* Initialize seat */
    if (seat_init(&g_compositor->seat) < 0) {
        renderer_deinit(&g_compositor->renderer);
        backend_deinit(backend);
        wl_display_destroy(g_compositor->display);
        free(g_compositor);
        return -1;
    }

    comp_damage_ring_init(&g_compositor->damage_ring);
    wl_list_init(&g_compositor->surface_list);
    wl_list_init(&g_compositor->window_list);

    return 0;
}

void comp_run(void)
{
    struct epoll_event events[16];
    int epfd, nfds;

    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        return;
    }

    g_compositor->running = 1;

    while (g_compositor->running) {
        nfds = epoll_wait(epfd, events, 16, 16);
        if (nfds < 0) continue;

        for (int i = 0; i < nfds; i++) {
            /* Handle Wayland events */
            if (events[i].data.fd == 0) {
                /* Process pending events */
            }
            /* Handle DRM page flip */
            else if (events[i].data.fd == g_compositor->backend.u.drm.drm->fd) {
                /* Handle DRM events */
            }
            /* Handle input events */
            else {
                seat_handle_event(events[i].data.fd);
            }
        }

        /* Repaint if needed */
        if (g_compositor->damage_ring.count > 0 || g_compositor->frame_callback_count > 0) {
            renderer_begin(&g_compositor->renderer,
                         g_compositor->output.width,
                         g_compositor->output.height);
            renderer_clear(&g_compositor->renderer, 0.0f, 0.0f, 0.0f, 1.0f);

            /* Render surfaces in z-order */
            uos_window_t *window;
            wl_list_for_each(window, &g_compositor->window_list, link) {
                if (window->visible && window->xdg_toplevel) {
                    uos_surface_t *surface = (uos_surface_t *)window->xdg_toplevel;
                    if (surface && surface->texture) {
                        renderer_blit_surface(&g_compositor->renderer, surface,
                                            window->x, window->y);
                    }
                }
            }

            renderer_end(&g_compositor->renderer);
            backend_commit(&g_compositor->backend);

            comp_damage_ring_clear(&g_compositor->damage_ring);
            g_compositor->frame_callback_count = 0;
        }
    }

    close(epfd);
}

void comp_shutdown(void)
{
    if (!g_compositor) return;

    seat_deinit(&g_compositor->seat);
    renderer_deinit(&g_compositor->renderer);
    backend_deinit(&g_compositor->backend);

    free(g_compositor->damage_ring.damages);
    wl_display_destroy(g_compositor->display);
    free(g_compositor);
    g_compositor = NULL;
}
/*
 * UOS Compositor - Rendering Backend Implementation
 * BSD-licensed
 */

#include "backend.h"
#include "renderer.h"
#include "../gfx/drm.h"
#include "../gfx/gbm.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

int backend_init(uos_backend_t *backend, int type, const char *card)
{
    backend->type = type;

    switch (type) {
    case BACKEND_DRM:
        return backend_drm_init(backend, card);
    default:
        return -1;
    }
}

void backend_deinit(uos_backend_t *backend)
{
    if (!backend) return;

    switch (backend->type) {
    case BACKEND_DRM:
        backend_drm_deinit(backend);
        break;
    }
}

void *backend_get_renderer(uos_backend_t *backend)
{
    if (!backend || !backend->ops.get_renderer) return NULL;
    return backend->ops.get_renderer(backend);
}

void *backend_get_allocator(uos_backend_t *backend)
{
    if (!backend || !backend->ops.get_allocator) return NULL;
    return backend->ops.get_allocator(backend);
}

int backend_get_output(uos_backend_t *backend, void *output)
{
    if (!backend || !backend->ops.get_output) return -1;
    return backend->ops.get_output(backend, output);
}

int backend_commit(uos_backend_t *backend)
{
    backend_drm_t *drm;
    int ret;

    if (!backend) return -1;

    if (backend->type != BACKEND_DRM) return -1;

    drm = (backend_drm_t *)backend->private_data;
    if (!drm || !drm->drm_dev) return -1;

    /* Page flip to display the rendered buffer */
    ret = drm_page_flip(drm->drm_dev, drm->crtc->crtc_id, drm->fb_id,
                       DRM_MODE_PAGE_FLIP_EVENT, backend);
    return ret;
}

/* DRM Backend Implementation */
int backend_drm_init(uos_backend_t *backend, const char *card)
{
    backend_drm_t *drm;
    int ret;

    drm = calloc(1, sizeof(*drm));
    if (!drm) return -1;

    ret = drm_open(card, &drm->drm_dev);
    if (ret < 0) {
        free(drm);
        return -1;
    }

    ret = drm_get_resources(drm->drm_dev);
    if (ret < 0) {
        drm_close(drm->drm_dev);
        free(drm);
        return -1;
    }

    drm->connector = drm_find_connector(drm->drm_dev, DRM_MODE_CONNECTOR_EDP);
    if (!drm->connector) {
        drm->connector = drm_find_connector(drm->drm_dev, DRM_MODE_CONNECTOR_HDMI);
    }
    if (!drm->connector) {
        drm_close(drm->drm_dev);
        free(drm);
        return -1;
    }

    drm->crtc = drm_find_crtc(drm->drm_dev, drm->connector);
    if (!drm->crtc) {
        drm_close(drm->drm_dev);
        free(drm);
        return -1;
    }

    /* Use first preferred mode */
    if (drm->connector->count_modes > 0) {
        drm->mode = *drm->connector->modes;
    }

    /* Create GBM device */
    drm->gbm_dev = gbm_device_create(drm->drm_dev->fd);
    if (!drm->gbm_dev) {
        drm_close(drm->drm_dev);
        free(drm);
        return -1;
    }

    backend->private_data = drm;
    backend->ops.get_renderer = backend_drm_get_renderer;
    backend->ops.get_allocator = backend_drm_get_allocator;
    backend->ops.get_output = backend_drm_get_output;

    return 0;
}

void backend_drm_deinit(uos_backend_t *backend)
{
    backend_drm_t *drm = (backend_drm_t *)backend->private_data;

    if (!drm) return;

    if (drm->gbm_dev) {
        gbm_device_destroy(drm->gbm_dev);
    }

    if (drm->drm_dev) {
        drm_close(drm->drm_dev);
    }

    free(drm);
    backend->private_data = NULL;
}

void *backend_drm_get_renderer(uos_backend_t *backend)
{
    static uos_renderer_t renderer;
    backend_drm_t *drm = (backend_drm_t *)backend->private_data;

    if (!drm) return NULL;
    memset(&renderer, 0, sizeof(renderer));
    /* Renderer will be initialized separately via renderer_init() */
    return &renderer;
}

void *backend_drm_get_allocator(uos_backend_t *backend)
{
    static uos_gbm_allocator_t allocator;
    backend_drm_t *drm = (backend_drm_t *)backend->private_data;

    if (!drm) return NULL;
    allocator.device = drm->gbm_dev;
    return &allocator;
}

int backend_drm_get_output(uos_backend_t *backend, void *output)
{
    backend_drm_t *drm = (backend_drm_t *)backend->private_data;
    uos_output_t *out = (uos_output_t *)output;

    if (!drm || !out) return -1;

    out->x = 0;
    out->y = 0;
    out->width = drm->mode.hdisplay;
    out->height = drm->mode.vdisplay;
    out->refresh = drm->mode.vrefresh;
    out->scale = 1;
    out->transform = OUTPUT_TRANSFORM_NORMAL;
    out->crtc_id = drm->crtc->crtc_id;
    out->connector_id = drm->connector->connector_id;

    return 0;
}

void backend_drm_page_flip_handler(uos_backend_t *backend, void *user_data)
{
    (void)backend;
    (void)user_data;
    /* Called on vsync - schedule next frame if needed */
}
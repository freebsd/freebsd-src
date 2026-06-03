/*
 * UOS Compositor - Rendering Backend
 * BSD-licensed
 */

#ifndef _UOS_BACKEND_H_
#define _UOS_BACKEND_H_

#include <stdint.h>
#include <stdbool.h>
#include "protocols.h"

/* Backend types */
#define BACKEND_DRM  1

/* Backend interface - function pointers for each backend type */
typedef struct uos_backend_ops {
    int (*init)(void *backend, const char *card);
    void (*deinit)(void *backend);
    void *(*get_renderer)(void *backend);
    void *(*get_allocator)(void *backend);
    int (*get_output)(void *backend, void *output);
} uos_backend_ops_t;

/* Backend structure */
typedef struct uos_backend {
    int type;
    uos_backend_ops_t ops;
    void *private_data;
} uos_backend_t;

/* DRM backend specific data */
typedef struct backend_drm {
    struct drm_device *drm_dev;
    struct gbm_device *gbm_dev;
    struct drm_connector *connector;
    struct drm_crtc *crtc;
    struct drm_mode mode;
    uint32_t fb_id;
    uint32_t bo_handle;
    void *fb_map;
} backend_drm_t;

/* Backend public API */
int backend_init(uos_backend_t *backend, int type, const char *card);
void backend_deinit(uos_backend_t *backend);
void *backend_get_renderer(uos_backend_t *backend);
void *backend_get_allocator(uos_backend_t *backend);
int backend_get_output(uos_backend_t *backend, void *output);
int backend_commit(uos_backend_t *backend);

/* DRM backend API */
int backend_drm_init(uos_backend_t *backend, const char *card);
void backend_drm_deinit(uos_backend_t *backend);
void *backend_drm_get_renderer(uos_backend_t *backend);
void *backend_drm_get_allocator(uos_backend_t *backend);
int backend_drm_get_output(uos_backend_t *backend, void *output);
void backend_drm_page_flip_handler(uos_backend_t *backend, void *user_data);

#endif /* _UOS_BACKEND_H_ */
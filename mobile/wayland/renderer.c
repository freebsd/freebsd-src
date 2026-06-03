/*
 * UOS Compositor - GL Renderer Implementation
 * BSD-licensed
 */

#include "renderer.h"
#include "backend.h"
#include "../gfx/egl.h"
#include "../gfx/gbm.h"
#include "compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *vertex_shader_src =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "uniform mat4 u_mvp;\n"
    "void main() {\n"
    "    gl_Position = u_mvp * vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texcoord);\n"
    "}\n";

int renderer_init(uos_renderer_t *renderer, struct uos_backend *backend)
{
    backend_drm_t *drm;
    EGLConfig *configs;
    EGLint num_configs;
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    drm = (backend_drm_t *)backend->private_data;
    if (!drm) return -1;

    memset(renderer, 0, sizeof(*renderer));

    renderer->egl_display = egl_get_display((NativeDisplayType)drm->drm_dev->fd);
    if (renderer->egl_display == EGL_NO_DISPLAY) return -1;

    egl_initialize(renderer->egl_display, NULL, NULL);

    configs = egl_choose_config(renderer->egl_display, attribs, &num_configs);
    if (num_configs < 1) return -1;

    /* Create GBM surface for EGL */
    drm->gbm_surface = gbm_surface_create(drm->gbm_dev,
                                         1920, 1080,
                                         DRM_FORMAT_XRGB8888,
                                         GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
    if (!drm->gbm_surface) return -1;

    renderer->egl_surface = egl_create_window_surface(renderer->egl_display,
                                                     configs[0],
                                                     (NativeWindowType)drm->gbm_surface,
                                                     NULL);
    if (renderer->egl_surface == EGL_NO_SURFACE) return -1;

    EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    renderer->egl_context = egl_create_context(renderer->egl_display, configs[0],
                                              EGL_NO_CONTEXT, ctx_attribs);
    if (renderer->egl_context == EGL_NO_CONTEXT) return -1;

    egl_make_current(renderer->egl_display, renderer->egl_surface,
                    renderer->egl_surface, renderer->egl_context);

    /* Initialize gl wrapper */
    gl_init(&renderer->gl, renderer->egl_display);

    renderer->shader_program = gl_create_program(&renderer->gl, vertex_shader_src, fragment_shader_src);

    return 0;
}

void renderer_deinit(uos_renderer_t *renderer)
{
    if (!renderer) return;

    if (renderer->egl_context) {
        egl_make_current(renderer->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (renderer->egl_surface) {
        /* egl_destroy_surface not in provided header */
    }
    if (renderer->egl_display) {
        egl_terminate(renderer->egl_display);
    }
}

void renderer_begin(uos_renderer_t *renderer, int32_t width, int32_t height)
{
    renderer->width = width;
    renderer->height = height;
    renderer->gl.glViewport(0, 0, width, height);
    egl_make_current(renderer->egl_display, renderer->egl_surface,
                    renderer->egl_surface, renderer->egl_context);
}

void renderer_end(uos_renderer_t *renderer)
{
    egl_swap_buffers(renderer->egl_display, renderer->egl_surface);
}

void renderer_clear(uos_renderer_t *renderer, float r, float g, float b, float a)
{
    renderer->gl.glClearColor(r, g, b, a);
    renderer->gl.glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_blit_surface(uos_renderer_t *renderer, struct uos_surface *surface, int32_t x, int32_t y)
{
    if (!surface || !surface->texture) return;

    gl_draw_quad(&renderer->gl, surface->texture,
                 (float)x, (float)y,
                 (float)surface->width, (float)surface->height);
}

void renderer_blit_cursor(uos_renderer_t *renderer, void *cursor, int32_t x, int32_t y)
{
    (void)renderer;
    (void)cursor;
    (void)x;
    (void)y;
    /* Cursor rendering - implemented when cursor surface support is added */
}
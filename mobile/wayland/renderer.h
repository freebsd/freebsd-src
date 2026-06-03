/*
 * UOS Compositor - GL Renderer
 * BSD-licensed
 */

#ifndef _UOS_RENDERER_H_
#define _UOS_RENDERER_H_

#include <stdint.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "../gfx/gl.h"

/* Forward declaration */
struct uos_backend;
struct uos_surface;

/* Renderer */
typedef struct uos_renderer {
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    gl_t gl;
    GLuint shader_program;
    GLuint vertex_shader;
    GLuint fragment_shader;
    int32_t width;
    int32_t height;
} uos_renderer_t;

/* Renderer public API */
int renderer_init(uos_renderer_t *renderer, struct uos_backend *backend);
void renderer_deinit(uos_renderer_t *renderer);
void renderer_begin(uos_renderer_t *renderer, int32_t width, int32_t height);
void renderer_end(uos_renderer_t *renderer);
void renderer_clear(uos_renderer_t *renderer, float r, float g, float b, float a);
void renderer_blit_surface(uos_renderer_t *renderer, struct uos_surface *surface, int32_t x, int32_t y);
void renderer_blit_cursor(uos_renderer_t *renderer, void *cursor, int32_t x, int32_t y);

#endif /* _UOS_RENDERER_H_ */
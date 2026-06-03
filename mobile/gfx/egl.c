/*
 * Copyright (c) 2026 The FreeBSD Mobile Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "egl.h"
#include <stdlib.h>

/* EGL display from native display (DRM fd) */
egl_display_t
egl_get_display(int native_display)
{
    return eglGetDisplay((EGLNativeDisplayType)(intptr_t)native_display);
}

bool
egl_initialize(egl_display_t display, egl_int_t *major, egl_int_t *minor)
{
    return eglInitialize(display, major, minor) == EGL_TRUE;
}

bool
egl_terminate(egl_display_t display)
{
    return eglTerminate(display) == EGL_TRUE;
}

const char *
egl_query_string(egl_display_t display, egl_int_t name)
{
    return eglQueryString(display, name);
}

egl_config_t *
egl_choose_config(egl_display_t display, const egl_int_t *attribs, egl_int_t *num_config)
{
    EGLint count;
    EGLConfig *configs = eglChooseConfig(display, attribs, NULL, 0, &count);
    if (!configs || count == 0)
        return NULL;

    configs = malloc(count * sizeof(EGLConfig));
    if (!configs)
        return NULL;

    if (eglChooseConfig(display, attribs, configs, count, &count) != EGL_TRUE) {
        free(configs);
        return NULL;
    }

    *num_config = count;
    return configs;
}

egl_context_t
egl_create_context(egl_display_t display, egl_config_t config, egl_context_t share_context, const egl_int_t *attribs)
{
    return eglCreateContext(display, config, share_context, attribs);
}

egl_surface_t
egl_create_window_surface(egl_display_t display, egl_config_t config, NativeWindowType window, const egl_int_t *attribs)
{
    return eglCreateWindowSurface(display, config, window, attribs);
}

egl_surface_t
egl_create_pbuffer_surface(egl_display_t display, egl_config_t config, const egl_int_t *attribs)
{
    return eglCreatePbufferSurface(display, config, attribs);
}

egl_surface_t
egl_create_pbuffer_from_client_buffer(egl_display_t display, egl_int_t buftype, void *buffer, egl_config_t config, const egl_int_t *attribs)
{
    return eglCreatePbufferFromClientBuffer(display, buftype, buffer, config, attribs);
}

bool
egl_make_current(egl_display_t display, egl_surface_t draw, egl_surface_t read, egl_context_t ctx)
{
    return eglMakeCurrent(display, draw, read, ctx) == EGL_TRUE;
}

bool
egl_swap_buffers(egl_display_t display, egl_surface_t surface)
{
    return eglSwapBuffers(display, surface) == EGL_TRUE;
}

bool
egl_swap_interval(egl_display_t display, egl_int_t interval)
{
    return eglSwapInterval(display, interval) == EGL_TRUE;
}

bool
egl_wait_client(egl_display_t display)
{
    return eglWaitClient() == EGL_TRUE;
}

bool
egl_wait_native(egl_int_t engine)
{
    return eglWaitNative(engine) == EGL_TRUE;
}

bool
egl_bind_api(egl_int_t api)
{
    return eglBindAPI(api) == EGL_TRUE;
}

egl_int_t
egl_query_api(void)
{
    return eglQueryAPI();
}

bool
egl_wait_sync(egl_display_t display, egl_sync_t sync, egl_int_t flags)
{
    return eglWaitSync(display, sync, flags) == EGL_TRUE;
}

egl_image_t
egl_create_image(egl_display_t display, egl_context_t ctx, egl_int_t target, egl_client_buffer buffer, const egl_int_t *attribs)
{
    return eglCreateImage(display, ctx, target, buffer, attribs);
}

bool
egl_destroy_image(egl_display_t display, egl_image_t image)
{
    return eglDestroyImage(display, image) == EGL_TRUE;
}

bool
egl_create_sync(egl_display_t display, egl_int_t type, const egl_int_t *attribs, egl_sync_t *sync)
{
    return eglCreateSync(display, type, attribs, sync) == EGL_TRUE;
}

bool
egl_destroy_sync(egl_display_t display, egl_sync_t sync)
{
    return eglDestroySync(display, sync) == EGL_TRUE;
}

egl_int_t
egl_client_wait_sync(egl_display_t display, egl_sync_t sync, egl_int_t flags, egl_time_t timeout)
{
    return eglClientWaitSync(display, sync, flags, timeout);
}

egl_int_t
egl_get_sync_attr(egl_display_t display, egl_sync_t sync, egl_int_t attribute, egl_int_t *value)
{
    return eglGetSyncAttrib(display, sync, attribute, value);
}

NativeDisplayType
egl_get_native_display(egl_display_t display)
{
    return eglGetNativeDisplay(display);
}

egl_int_t
egl_get_error(void)
{
    return eglGetError();
}
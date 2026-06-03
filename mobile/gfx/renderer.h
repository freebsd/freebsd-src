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

#ifndef _MOBILE_GFX_RENDERER_H_
#define _MOBILE_GFX_RENDERER_H_

#include <stdint.h>
#include <stdbool.h>

/* Pixel format */
typedef uint32_t pixel_t;

/* Renderer context */
typedef struct {
    int width;
    int height;
    int pitch;
    pixel_t *buffer;
} renderer_t;

/* Color values (0-255) */
typedef struct {
    uint8_t r, g, b, a;
} color_t;

/* Public API */
renderer_t *renderer_create(int width, int height);
void renderer_destroy(renderer_t *rend);
void renderer_clear(renderer_t *rend, color_t color);
void renderer_fill_rect(renderer_t *rend, int x, int y, int w, int h, color_t color);
void renderer_draw_rect(renderer_t *rend, int x, int y, int w, int h, color_t color, int thickness);
void renderer_blit(renderer_t *rend, renderer_t *src,
                   int sx, int sy, int sw, int sh,
                   int dx, int dy, int dw, int dh);
void renderer_set_clip(renderer_t *rend, int x, int y, int w, int h);
void renderer_draw_circle(renderer_t *rend, int x, int y, int radius, color_t color);
void renderer_fill_circle(renderer_t *rend, int x, int y, int radius, color_t color);
void renderer_draw_line(renderer_t *rend, int x0, int y0, int x1, int y1, color_t color);
void renderer_gradient_fill_rect(renderer_t *rend, int x, int y, int w, int h,
                                 color_t color1, color_t color2, bool vertical);

/* Helper to create color */
static inline color_t make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    color_t c = { r, g, b, a };
    return c;
}

#endif /* _MOBILE_GFX_RENDERER_H_ */
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

#include "renderer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Helper to clip a value */
static int clip(int val, int min, int max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

/* Helper to get pixel value */
static inline pixel_t get_pixel(renderer_t *rend, int x, int y)
{
    if (x < 0 || x >= rend->width || y < 0 || y >= rend->height)
        return 0;
    return rend->buffer[y * rend->pitch + x];
}

/* Helper to set pixel value */
static inline void set_pixel(renderer_t *rend, int x, int y, pixel_t pixel)
{
    if (x < 0 || x >= rend->width || y < 0 || y >= rend->height)
        return;
    rend->buffer[y * rend->pitch + x] = pixel;
}

/* Helper to convert color_t to pixel_t (assuming ARGB8888) */
static inline pixel_t color_to_pixel(color_t c)
{
    return ((pixel_t)c.a << 24) | ((pixel_t)c.r << 16) | ((pixel_t)c.g << 8) | (pixel_t)c.b;
}

/* Create renderer */
renderer_t *
renderer_create(int width, int height)
{
    renderer_t *rend = calloc(1, sizeof(renderer_t));
    if (!rend)
        return NULL;

    rend->width = width;
    rend->height = height;
    rend->pitch = width; /* Assuming 1 pixel per byte? Actually we use 32-bit */
    rend->pitch = width * sizeof(pixel_t);
    rend->buffer = calloc(height, rend->pitch);
    if (!rend->buffer) {
        free(rend);
        return NULL;
    }

    return rend;
}

/* Destroy renderer */
void
renderer_destroy(renderer_t *rend)
{
    if (!rend)
        return;
    free(rend->buffer);
    free(rend);
}

/* Clear entire buffer */
void
renderer_clear(renderer_t *rend, color_t color)
{
    if (!rend)
        return;
    pixel_t pixel = color_to_pixel(color);
    memset(rend->buffer, pixel, rend->height * rend->pitch);
}

/* Fill rectangle */
void
renderer_fill_rect(renderer_t *rend, int x, int y, int w, int h, color_t color)
{
    if (!rend || w <= 0 || h <= 0)
        return;

    int x0 = clip(x, 0, rend->width - 1);
    int y0 = clip(y, 0, rend->height - 1);
    int x1 = clip(x + w - 1, 0, rend->width - 1);
    int y1 = clip(y + h - 1, 0, rend->height - 1);

    if (x0 > x1 || y0 > y1)
        return;

    pixel_t pixel = color_to_pixel(color);
    int yy;
    for (yy = y0; yy <= y1; yy++) {
        pixel_t *row = &rend->buffer[yy * rend->pitch];
        int xx;
        for (xx = x0; xx <= x1; xx++) {
            row[xx] = pixel;
        }
    }
}

/* Draw rectangle outline */
void
renderer_draw_rect(renderer_t *rend, int x, int y, int w, int h, color_t color, int thickness)
{
    if (!rend || w <= 0 || h <= 0 || thickness <= 0)
        return;

    /* Top edge */
    renderer_fill_rect(rend, x, y, w, thickness, color);
    /* Bottom edge */
    renderer_fill_rect(rend, x, y + h - thickness, w, thickness, color);
    /* Left edge */
    renderer_fill_rect(rend, x, y + thickness, thickness, h - 2 * thickness, color);
    /* Right edge */
    renderer_fill_rect(rend, x + w - thickness, y + thickness, thickness, h - 2 * thickness, color);
}

/* Blit (copy) from source renderer to destination */
void
renderer_blit(renderer_t *rend, renderer_t *src,
              int sx, int sy, int sw, int sh,
              int dx, int dy, int dw, int dh)
{
    if (!rend || !src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;

    /* Source clip */
    int sx0 = clip(sx, 0, src->width - 1);
    int sy0 = clip(sy, 0, src->height - 1);
    int sx1 = clip(sx + sw - 1, 0, src->width - 1);
    int sy1 = clip(sy + sh - 1, 0, src->height - 1);
    if (sx0 > sx1 || sy0 > sy1)
        return;

    /* Destination clip */
    int dx0 = clip(dx, 0, rend->width - 1);
    int dy0 = clip(dy, 0, rend->height - 1);
    int dx1 = clip(dx + dw - 1, 0, rend->width - 1);
    int dy1 = clip(dy + dh - 1, 0, rend->height - 1);
    if (dx0 > dx1 || dy0 > dy1)
        return;

    /* Compute scaling factors */
    float src_x_step = (float)(sx1 - sx0 + 1) / dw;
    float src_y_step = (float)(sy1 - sy0 + 1) / dh;

    int dy, sy_acc = sy0 * 65536;
    for (dy = dy0; dy <= dy1; dy++, sy_acc += (int)(src_y_step * 65536)) {
        int sy = sy_acc >> 16;
        if (sy < sy0 || sy > sy1)
            continue;
        pixel_t *src_row = &src->buffer[sy * src->pitch];
        pixel_t *dst_row = &rend->buffer[dy * rend->pitch];
        int dx, sx_acc = sx0 * 65536;
        for (dx = dx0; dx <= dx1; dx++, sx_acc += (int)(src_x_step * 65536)) {
            int sx = sx_acc >> 16;
            if (sx < sx0 || sx > sx1)
                continue;
            dst_row[dx] = src_row[sx];
        }
    }
}

/* Set clipping rectangle */
void
renderer_set_clip(renderer_t *rend, int x, int y, int w, int h)
{
    /* In a simple implementation, we ignore clipping for now */
    /* A full implementation would store clip rect and check in drawing functions */
    (void)rend; (void)x; (void)y; (void)w; (void)h;
}

/* Draw circle outline using midpoint algorithm */
void
renderer_draw_circle(renderer_t *rend, int x, int y, int radius, color_t color)
{
    if (!rend || radius <= 0)
        return;

    pixel_t pixel = color_to_pixel(color);
    int xx = 0, yy = radius;
    int dx = 1, dy = -2 * radius;
    int err = 0;

    while (xx <= yy) {
        set_pixel(rend, x + xx, y + yy, pixel);
        set_pixel(rend, x - xx, y + yy, pixel);
        set_pixel(rend, x + xx, y - yy, pixel);
        set_pixel(rend, x - xx, y - yy, pixel);
        set_pixel(rend, x + yy, y + xx, pixel);
        set_pixel(rend, x - yy, y + xx, pixel);
        set_pixel(rend, x + yy, y - xx, pixel);
        set_pixel(rend, x - yy, y - xx, pixel);

        err += dx;
        dx += 2;
        if (2 * err + dy > 0) {
            yy--;
            dy += 2;
            err += dy;
        }
        xx++;
    }
}

/* Fill circle */
void
renderer_fill_circle(renderer_t *rend, int x, int y, int radius, color_t color)
{
    if (!rend || radius <= 0)
        return;

    pixel_t pixel = color_to_pixel(color);
    int xx = 0, yy = radius;
    int dx = 1, dy = -2 * radius;
    int err = 0;

    while (xx <= yy) {
        int yy_start = y - yy;
        int yy_end = y + yy;
        int xx_start = x - xx;
        int xx_end = x + xx;
        renderer_fill_rect(rend, xx_start, yy_start, xx_end - xx_start + 1, 1, color);
        renderer_fill_rect(rend, x - yy, yy_start, 2 * yy + 1, 1, color);
        yy_start = y - xx;
        yy_end = y + xx;
        renderer_fill_rect(rend, xx_start, yy_start, xx_end - xx_start + 1, 1, color);
        renderer_fill_rect(rend, x - xx, yy_start, 2 * xx + 1, 1, color);

        err += dx;
        dx += 2;
        if (2 * err + dy > 0) {
            yy--;
            dy += 2;
            err += dy;
        }
        xx++;
    }
}

/* Draw line using Bresenham's algorithm */
void
renderer_draw_line(renderer_t *rend, int x0, int y0, int x1, int y1, color_t color)
{
    if (!rend)
        return;

    pixel_t pixel = color_to_pixel(color);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    while (1) {
        set_pixel(rend, x0, y0, pixel);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

/* Gradient fill rectangle */
void
renderer_gradient_fill_rect(renderer_t *rend, int x, int y, int w, int h,
                            color_t color1, color_t color2, bool vertical)
{
    if (!rend || w <= 0 || h <= 0)
        return;

    int x0 = clip(x, 0, rend->width - 1);
    int y0 = clip(y, 0, rend->height - 1);
    int x1 = clip(x + w - 1, 0, rend->width - 1);
    int y1 = clip(y + h - 1, 0, rend->height - 1);
    if (x0 > x1 || y0 > y1)
        return;

    int xx, yy;
    if (vertical) {
        for (yy = y0; yy <= y1; yy++) {
            float ratio = (float)(yy - y0) / (y1 - y0);
            uint8_t r = color1.r + (color2.r - color1.r) * ratio;
            uint8_t g = color1.g + (color2.g - color1.g) * ratio;
            uint8_t b = color1.b + (color2.b - color1.b) * ratio;
            uint8_t a = color1.a + (color2.a - color1.a) * ratio;
            color_t color = { r, g, b, a };
            pixel_t pixel = color_to_pixel(color);
            pixel_t *row = &rend->buffer[yy * rend->pitch];
            for (xx = x0; xx <= x1; xx++) {
                row[xx] = pixel;
            }
        }
    } else {
        for (xx = x0; xx <= x1; xx++) {
            float ratio = (float)(xx - x0) / (x1 - x0);
            uint8_t r = color1.r + (color2.r - color1.r) * ratio;
            uint8_t g = color1.g + (color2.g - color1.g) * ratio;
            uint8_t b = color1.b + (color2.b - color1.b) * ratio;
            uint8_t a = color1.a + (color2.a - color1.a) * ratio;
            color_t color = { r, g, b, a };
            pixel_t pixel = color_to_pixel(color);
            int yy;
            for (yy = y0; yy <= y1; yy++) {
                rend->buffer[yy * rend->pitch + xx] = pixel;
            }
        }
    }
}
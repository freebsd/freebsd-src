/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdlib.h>

#ifndef _WIN32
#   if defined(__FreeBSD__)
#      include <sys/endian.h>
#      define	__BYTE_ORDER	_BYTE_ORDER
#      define	__BIG_ENDIAN	_BIG_ENDIAN
#   else
#      include <endian.h>
#   endif /* ! __FreeBSD__ */
#else
#define __BYTE_ORDER __BYTE_ORDER__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#endif

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"
#include "plot.h"

static inline uint8_t *
get_xy_loc(nsfb_t *nsfb, int x, int y)
{
        return (uint8_t *)(nsfb->ptr + (y * nsfb->linelen) + (x * 3));
}

#if __BYTE_ORDER == __BIG_ENDIAN
static inline nsfb_colour_t pixel_to_colour(uint8_t pixel)
{
        return (pixel >> 8) & ~0xFF000000U;
}

/* convert a colour value to a 32bpp pixel value ready for screen output */
static inline uint32_t colour_to_pixel(nsfb_colour_t c)
{
        return (c << 8);
}
#else /* __BYTE_ORDER == __BIG_ENDIAN */
static inline nsfb_colour_t pixel_to_colour(uint32_t pixel)
{
        return ((pixel & 0xFF) << 16) |
                ((pixel & 0xFF00)) |
                ((pixel & 0xFF0000) >> 16);
}

/* convert a colour value to a 32bpp pixel value ready for screen output */
static inline uint32_t colour_to_pixel(nsfb_colour_t c)
{
        return ((c & 0xff0000) >> 16) | (c & 0xff00) | ((c & 0xff) << 16);
}
#endif

#define SIGN(x)  ((x<0) ?  -1  :  ((x>0) ? 1 : 0))

static bool
line(nsfb_t *nsfb, int linec, nsfb_bbox_t *line, nsfb_plot_pen_t *pen)
{
        int w;
        uint32_t ent;
        uint32_t *pvideo;
        int x, y, i;
        int dx, dy, sdy;
        int dxabs, dyabs;

        ent = colour_to_pixel(pen->stroke_colour);

        for (;linec > 0; linec--) {

                if (line->y0 == line->y1) {
                        /* horizontal line special cased */

                        if (!nsfb_plot_clip_ctx(nsfb, line)) {
                                /* line outside clipping */
                                line++;
                                continue;
                        }

                        pvideo = get_xy_loc(nsfb, line->x0, line->y0);

                        w = line->x1 - line->x0;
                        while (w-- > 0)
                                *(pvideo + w) = ent;

                } else {
                        /* standard bresenham line */

                        if (!nsfb_plot_clip_line_ctx(nsfb, line)) {
                                /* line outside clipping */
                                line++;
                                continue;
                        }

                        /* the horizontal distance of the line */
                        dx = line->x1 - line->x0;
                        dxabs = abs (dx);

                        /* the vertical distance of the line */
                        dy = line->y1 - line->y0;
                        dyabs = abs (dy);

                        sdy = dx ? SIGN(dy) * SIGN(dx) : SIGN(dy);

                        if (dx >= 0)
                                pvideo = get_xy_loc(nsfb, line->x0, line->y0);
                        else
                                pvideo = get_xy_loc(nsfb, line->x1, line->y1);

                        x = dyabs >> 1;
                        y = dxabs >> 1;

                        if (dxabs >= dyabs) {
                                /* the line is more horizontal than vertical */
                                for (i = 0; i < dxabs; i++) {
                                        *pvideo = ent;

                                        pvideo++;
                                        y += dyabs;
                                        if (y >= dxabs) {
                                                y -= dxabs;
                                                pvideo += sdy * (nsfb->linelen>>2);
                                        }
                                }
                        } else {
                                /* the line is more vertical than horizontal */
                                for (i = 0; i < dyabs; i++) {
                                        *pvideo = ent;
                                        pvideo += sdy * (nsfb->linelen >> 2);

                                        x += dxabs;
                                        if (x >= dyabs) {
                                                x -= dyabs;
                                                pvideo++;
                                        }
                                }
                        }

                }
                line++;
        }
        return true;
}



static bool fill(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t c)
{
        int w;
        uint32_t *pvid;
        uint32_t ent;
        uint32_t llen;
        uint32_t width;
        uint32_t height;

        if (!nsfb_plot_clip_ctx(nsfb, rect))
                return true; /* fill lies outside current clipping region */

        ent = colour_to_pixel(c);
        width = rect->x1 - rect->x0;
        height = rect->y1 - rect->y0;
        llen = (nsfb->linelen >> 2) - width;

        pvid = get_xy_loc(nsfb, rect->x0, rect->y0);

        while (height-- > 0) {
                w = width;
                while (w >= 16) {
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       w-=16;
                }
                while (w >= 4) {
                       *pvid++ = ent; *pvid++ = ent;
                       *pvid++ = ent; *pvid++ = ent;
                       w-=4;
                }
                while (w > 0) {
                       *pvid++ = ent;
                       w--;
                }
                pvid += llen;
        }

        return true;
}




static bool point(nsfb_t *nsfb, int x, int y, nsfb_colour_t c)
{
        uint32_t *pvideo;

        /* check point lies within clipping region */
        if ((x < nsfb->clip.x0) ||
            (x >= nsfb->clip.x1) ||
            (y < nsfb->clip.y0) ||
            (y >= nsfb->clip.y1))
                return true;

        pvideo = get_xy_loc(nsfb, x, y);

        if ((c & 0xFF000000) != 0) {
                if ((c & 0xFF000000) != 0xFF000000) {
                        c = nsfb_plot_ablend(c, pixel_to_colour(*pvideo));
                }

                *pvideo = colour_to_pixel(c);
        }
        return true;
}

static bool
glyph1(nsfb_t *nsfb,
       nsfb_bbox_t *loc,
       const uint8_t *pixel,
       int pitch,
       nsfb_colour_t c)
{
        uint32_t *pvideo;
        int xloop, yloop;
        int xoff, yoff; /* x and y offset into image */
        int x = loc->x0;
        int y = loc->y0;
        int width = loc->x1 - loc->x0;
        int height = loc->y1 - loc->y0;
        uint32_t fgcol;
        const uint8_t *fntd;
        uint8_t row;

        if (!nsfb_plot_clip_ctx(nsfb, loc))
                return true;

        if (height > (loc->y1 - loc->y0))
                height = (loc->y1 - loc->y0);

        if (width > (loc->x1 - loc->x0))
                width = (loc->x1 - loc->x0);

        xoff = loc->x0 - x;
        yoff = loc->y0 - y;

        pvideo = get_xy_loc(nsfb, loc->x0, loc->y0);

        fgcol = colour_to_pixel(c);

        for (yloop = yoff; yloop < height; yloop++) {
                fntd = pixel + (yloop * (pitch>>3)) + (xoff>>3);
                row = (*fntd++) << (xoff & 3);
                for (xloop = xoff; xloop < width ; xloop++) {
                        if (((xloop % 8) == 0) && (xloop != 0)) {
                                row = *fntd++;
                        }

                        if ((row & 0x80) != 0) {
                                *(pvideo + xloop) = fgcol;
                        }
                        row = row << 1;

                }

                pvideo += (nsfb->linelen >> 2);
        }

        return true;
}

static bool
glyph8(nsfb_t *nsfb,
       nsfb_bbox_t *loc,
       const uint8_t *pixel,
       int pitch,
       nsfb_colour_t c)
{
        uint32_t *pvideo;
        nsfb_colour_t abpixel; /* alphablended pixel */
        int xloop, yloop;
        int xoff, yoff; /* x and y offset into image */
        int x = loc->x0;
        int y = loc->y0;
        int width = loc->x1 - loc->x0;
        int height = loc->y1 - loc->y0;
        uint32_t fgcol;

        if (!nsfb_plot_clip_ctx(nsfb, loc))
                return true;

        if (height > (loc->y1 - loc->y0))
                height = (loc->y1 - loc->y0);

        if (width > (loc->x1 - loc->x0))
                width = (loc->x1 - loc->x0);

        xoff = loc->x0 - x;
        yoff = loc->y0 - y;

        pvideo = get_xy_loc(nsfb, loc->x0, loc->y0);

        fgcol = c & 0xFFFFFF;

        for (yloop = 0; yloop < height; yloop++) {
                for (xloop = 0; xloop < width; xloop++) {
                        abpixel = (pixel[((yoff + yloop) * pitch) + xloop + xoff] << 24) | fgcol;
                        if ((abpixel & 0xFF000000) != 0) {
                                /* pixel is not transparent */
                                if ((abpixel & 0xFF000000) != 0xFF000000) {
                                        abpixel = nsfb_plot_ablend(abpixel,
                                                                   pixel_to_colour(*(pvideo + xloop)));
                                }

                                *(pvideo + xloop) = colour_to_pixel(abpixel);
                        }
                }
                pvideo += (nsfb->linelen >> 2);
        }

        return true;
}

static bool
bitmap(nsfb_t *nsfb,
       const nsfb_bbox_t *loc,
       const nsfb_colour_t *pixel,
       int bmp_width,
       int bmp_height,
       int bmp_stride,
       bool alpha)
{
        uint32_t *pvideo;
        nsfb_colour_t abpixel = 0; /* alphablended pixel */
        int xloop, yloop;
        int xoff, yoff; /* x and y offset into image */
        int x = loc->x0;
        int y = loc->y0;
        int width = loc->x1 - loc->x0;
        int height = loc->y1 - loc->y0;
        nsfb_bbox_t clipped; /* clipped display */

        /* TODO here we should scale the image from bmp_width to width, for
         * now simply crop.
         */
        if (width > bmp_width)
                width = bmp_width;

        if (height > bmp_height)
                height = bmp_height;

        /* The part of the scaled image actually displayed is cropped to the
         * current context.
         */
        clipped.x0 = x;
        clipped.y0 = y;
        clipped.x1 = x + width;
        clipped.y1 = y + height;

        if (!nsfb_plot_clip_ctx(nsfb, &clipped)) {
                return true;
        }

        if (height > (clipped.y1 - clipped.y0))
                height = (clipped.y1 - clipped.y0);

        if (width > (clipped.x1 - clipped.x0))
                width = (clipped.x1 - clipped.x0);

        xoff = clipped.x0 - x;
        yoff = (clipped.y0 - y) * bmp_width;
        height = height * bmp_stride + yoff;

        /* plot the image */
        pvideo = get_xy_loc(nsfb, clipped.x0, clipped.y0);

        if (alpha) {
                for (yloop = yoff; yloop < height; yloop += bmp_stride) {
                        for (xloop = 0; xloop < width; xloop++) {
                                abpixel = pixel[yloop + xloop + xoff];
                                if ((abpixel & 0xFF000000) != 0) {
                                        if ((abpixel & 0xFF000000) != 0xFF000000) {
                                                abpixel = nsfb_plot_ablend(abpixel,
                                                                           pixel_to_colour(*(pvideo + xloop)));
                                        }

                                        *(pvideo + xloop) = colour_to_pixel(abpixel);
                                }
                        }
                        pvideo += (nsfb->linelen >> 2);
                }
        } else {
                for (yloop = yoff; yloop < height; yloop += bmp_stride) {
                        for (xloop = 0; xloop < width; xloop++) {
                                abpixel = pixel[yloop + xloop + xoff];
                                *(pvideo + xloop) = colour_to_pixel(abpixel);
                        }
                        pvideo += (nsfb->linelen >> 2);
                }
        }
        return true;
}

static bool readrect(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t *buffer)
{
        uint32_t *pvideo;
        int xloop, yloop;
        int width;

        if (!nsfb_plot_clip_ctx(nsfb, rect)) {
                return true;
        }

        width = rect->x1 - rect->x0;

        pvideo = get_xy_loc(nsfb, rect->x0, rect->y0);

        for (yloop = rect->y0; yloop < rect->y1; yloop += 1) {
                for (xloop = 0; xloop < width; xloop++) {
                        *buffer = pixel_to_colour(*(pvideo + xloop));
                        buffer++;
                }
                pvideo += (nsfb->linelen >> 2);
        }
        return true;
}

const nsfb_plotter_fns_t _nsfb_24bpp_plotters = {
        .line = line,
        .fill = fill,
        .point = point,
        .bitmap = bitmap,
        .bitmap_tiles = bitmap_tiles,
        .glyph8 = glyph8,
        .glyph1 = glyph1,
        .readrect = readrect,
};

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

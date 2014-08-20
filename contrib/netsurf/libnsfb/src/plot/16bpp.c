/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdlib.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"
#include "plot.h"

#define UNUSED __attribute__((unused)) 

static inline uint16_t *get_xy_loc(nsfb_t *nsfb, int x, int y)
{
        return (void *)(nsfb->ptr + (y * nsfb->linelen) + (x << 1));
}

static inline nsfb_colour_t pixel_to_colour(UNUSED nsfb_t *nsfb, uint16_t pixel)
{
        return ((pixel & 0x1F) << 19) |
              ((pixel & 0x7E0) << 5) |
              ((pixel & 0xF800) >> 8);
}

/* convert a colour value to a 16bpp pixel value ready for screen output */
static inline uint16_t colour_to_pixel(UNUSED nsfb_t *nsfb, nsfb_colour_t c)
{
        return ((c & 0xF8) << 8) | ((c & 0xFC00 ) >> 5) | ((c & 0xF80000) >> 19);
}

#define PLOT_TYPE uint16_t
#define PLOT_LINELEN(ll) ((ll) >> 1)

#include "common.c"


static bool fill(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t c)
{
        int w;
        uint16_t *pvid16;
        uint16_t ent16;
        uint32_t *pvid32;
        uint32_t ent32;
        uint32_t llen;
        uint32_t width;
        uint32_t height;

        if (!nsfb_plot_clip_ctx(nsfb, rect))
                return true; /* fill lies outside current clipping region */

        ent16 = colour_to_pixel(nsfb, c);
        width = rect->x1 - rect->x0;
        height = rect->y1 - rect->y0;

        pvid16 = get_xy_loc(nsfb, rect->x0, rect->y0);

        if (((rect->x0 & 1) == 0) && ((width & 1) == 0)) {
                /* aligned to 32bit value and width is even */
                width = width >> 1;
                llen = (nsfb->linelen >> 2) - width;
                ent32 = ent16 | (ent16 << 16);
                pvid32 = (void *)pvid16;

                while (height-- > 0) {
                        w = width;
                        while (w >= 16) {
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                w-=16;
                        }
                        while (w >= 4) {
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                *pvid32++ = ent32; *pvid32++ = ent32;
                                w-=4;
                        }
                        while (w > 0) {
                                *pvid32++ = ent32;
                                w--;
                        }
                        // for (w = width; w > 0; w--) *pvid32++ = ent32;
                        pvid32 += llen;
                }

        } else {
                llen = (nsfb->linelen >> 1) - width;


                while (height-- > 0) {
                        for (w = width; w > 0; w--) *pvid16++ = ent16;
                        pvid16 += llen;
                }
        }
        return true;
}

const nsfb_plotter_fns_t _nsfb_16bpp_plotters = {
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

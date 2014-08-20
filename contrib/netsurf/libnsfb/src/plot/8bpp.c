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
#include <string.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"
#include "palette.h"
#include "plot.h"

static inline uint8_t *get_xy_loc(nsfb_t *nsfb, int x, int y)
{
        return (uint8_t *)(nsfb->ptr + (y * nsfb->linelen) + (x));
}


static inline nsfb_colour_t pixel_to_colour(nsfb_t *nsfb, uint8_t pixel)
{
        if (nsfb->palette == NULL)
                return 0;

        return nsfb->palette->data[pixel];
}

static uint8_t colour_to_pixel(nsfb_t *nsfb, nsfb_colour_t c)
{
        if (nsfb->palette == NULL)
                return 0;

        return nsfb_palette_best_match_dither(nsfb->palette, c);
}

#define PLOT_TYPE uint8_t
#define PLOT_LINELEN(ll) (ll)

#include "common.c"

static bool fill(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t c)
{
        int y;
        uint8_t ent;
        uint8_t *pvideo;

        if (!nsfb_plot_clip_ctx(nsfb, rect))
                return true; /* fill lies outside current clipping region */

        pvideo = get_xy_loc(nsfb, rect->x0, rect->y0);

        ent = colour_to_pixel(nsfb, c);

        for (y = rect->y0; y < rect->y1; y++) {
                memset(pvideo, ent, rect->x1 - rect->x0);
                pvideo += nsfb->linelen;
        }

        return true;
}

const nsfb_plotter_fns_t _nsfb_8bpp_plotters = {
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

/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include "common.c"

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

        ent = colour_to_pixel(nsfb, c);
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

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

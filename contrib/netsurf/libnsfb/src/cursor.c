/*
 * Copyright 2010 Vincent Sanders <vince@kyllikki.org>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

/** \file
 * cursor (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_cursor.h"

#include "nsfb.h"
#include "cursor.h"
#include "plot.h"
#include "surface.h"

bool nsfb_cursor_init(nsfb_t *nsfb)
{
    if (nsfb->cursor != NULL)
        return false;

    nsfb->cursor = calloc(1, sizeof(struct nsfb_cursor_s));
    if (nsfb->cursor == NULL) 
        return false;

    nsfb->cursor->loc.x0 = nsfb->width / 2;
    nsfb->cursor->loc.y0 = nsfb->height / 2;
    return true;
}

bool nsfb_cursor_set(nsfb_t *nsfb, const nsfb_colour_t *pixel,
        int bmp_width, int bmp_height, int bmp_stride,
        int hotspot_x, int hotspot_y)
{
    if (nsfb->cursor == NULL) 
        return false;

    nsfb->cursor->pixel = pixel;
    nsfb->cursor->bmp_width = bmp_width;
    nsfb->cursor->bmp_height = bmp_height;
    nsfb->cursor->bmp_stride = bmp_stride;
    nsfb->cursor->loc.x1 = nsfb->cursor->loc.x0 + nsfb->cursor->bmp_width;
    nsfb->cursor->loc.y1 = nsfb->cursor->loc.y0 + nsfb->cursor->bmp_height;

    nsfb->cursor->hotspot_x = hotspot_x;
    nsfb->cursor->hotspot_y = hotspot_y;
 
    return nsfb->surface_rtns->cursor(nsfb, nsfb->cursor);
}

bool nsfb_cursor_loc_set(nsfb_t *nsfb, const nsfb_bbox_t *loc)
{
    if (nsfb->cursor == NULL) 
        return false;

    nsfb->cursor->loc = *loc;
    nsfb->cursor->loc.x1 = nsfb->cursor->loc.x0 + nsfb->cursor->bmp_width;
    nsfb->cursor->loc.y1 = nsfb->cursor->loc.y0 + nsfb->cursor->bmp_height;

    return nsfb->surface_rtns->cursor(nsfb, nsfb->cursor);
}

bool nsfb_cursor_loc_get(nsfb_t *nsfb, nsfb_bbox_t *loc)
{
    if (nsfb->cursor == NULL) 
        return false;

    *loc = nsfb->cursor->loc;
    return true;
}

/* documented in cursor.h */
bool nsfb_cursor_plot(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    int sav_size;
    nsfb_bbox_t sclip; /* saved clipping area */

    nsfb->plotter_fns->get_clip(nsfb, &sclip);
    nsfb->plotter_fns->set_clip(nsfb, NULL);

    /* offset cursor rect for hotspot */
    cursor->loc.x0 -= cursor->hotspot_x;
    cursor->loc.y0 -= cursor->hotspot_y;
    cursor->loc.x1 -= cursor->hotspot_x;
    cursor->loc.y1 -= cursor->hotspot_y;

    cursor->savloc = cursor->loc;

    cursor->sav_width = cursor->savloc.x1 - cursor->savloc.x0;
    cursor->sav_height = cursor->savloc.y1 - cursor->savloc.y0;

    sav_size = cursor->sav_width * cursor->sav_height * sizeof(nsfb_colour_t);
    if (cursor->sav_size < sav_size) {
        cursor->sav = realloc(cursor->sav, sav_size);
        cursor->sav_size = sav_size;
    }

    nsfb->plotter_fns->readrect(nsfb, &cursor->savloc, cursor->sav);
    cursor->sav_width = cursor->savloc.x1 - cursor->savloc.x0;
    cursor->sav_height = cursor->savloc.y1 - cursor->savloc.y0;

    nsfb->plotter_fns->set_clip(nsfb, NULL);
    nsfb->plotter_fns->bitmap(nsfb, 
                              &cursor->loc,  
                              cursor->pixel, 
                              cursor->bmp_width, 
                              cursor->bmp_height, 
                              cursor->bmp_stride, 
                              true);

    /* undo hotspot offset */
    cursor->loc.x0 += cursor->hotspot_x;
    cursor->loc.y0 += cursor->hotspot_y;
    cursor->loc.x1 += cursor->hotspot_x;
    cursor->loc.y1 += cursor->hotspot_y;

    nsfb->plotter_fns->set_clip(nsfb, &sclip);

    cursor->plotted = true;

    return true;
}

bool nsfb_cursor_clear(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
	nsfb_bbox_t sclip; /* saved clipping area */

	nsfb->plotter_fns->get_clip(nsfb, &sclip);
	nsfb->plotter_fns->set_clip(nsfb, NULL);

        nsfb->plotter_fns->bitmap(nsfb,
                                  &cursor->savloc,
                                  cursor->sav,
                                  cursor->sav_width,
                                  cursor->sav_height,
                                  cursor->sav_width,
                                  false);

	nsfb->plotter_fns->set_clip(nsfb, &sclip);

        cursor->plotted = false;
	return true;

}

bool nsfb_cursor_destroy(struct nsfb_cursor_s *cursor)
{
	/* Note: cursor->pixel isn't owned by us */

	free(cursor->sav);
	free(cursor);

	return true;
}

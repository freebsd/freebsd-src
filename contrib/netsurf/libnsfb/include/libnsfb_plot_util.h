/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the exported interface for the libnsfb graphics library. 
 */

#ifndef _LIBNSFB_PLOT_UTIL_H
#define _LIBNSFB_PLOT_UTIL_H 1


/* alpha blend two pixels together */
static inline nsfb_colour_t 
nsfb_plot_ablend(nsfb_colour_t pixel, nsfb_colour_t scrpixel)
{
    int opacity = pixel >> 24;
    int transp = 0x100 - opacity;
    uint32_t rb, g;

    rb = ((pixel & 0xFF00FF) * opacity +
          (scrpixel & 0xFF00FF) * transp) >> 8;
    g  = ((pixel & 0x00FF00) * opacity +
          (scrpixel & 0x00FF00) * transp) >> 8;

    return (rb & 0xFF00FF) | (g & 0xFF00);
}


bool nsfb_plot_clip(const nsfb_bbox_t * restrict clip, nsfb_bbox_t * restrict rect);

bool nsfb_plot_clip_ctx(nsfb_t *nsfb, nsfb_bbox_t * restrict rect);

bool nsfb_plot_clip_line(const nsfb_bbox_t * restrict clip, nsfb_bbox_t * restrict line);

bool nsfb_plot_clip_line_ctx(nsfb_t *nsfb, nsfb_bbox_t * restrict line);

/** Obtain a bounding box which is the superset of two source boxes.
 *
 */
bool nsfb_plot_add_rect(const nsfb_bbox_t *box1, const nsfb_bbox_t *box2, nsfb_bbox_t *result);

/** Find if two boxes intersect. */
bool nsfb_plot_bbox_intersect(const nsfb_bbox_t *box1, const nsfb_bbox_t *box2);

#endif /* _LIBNSFB_PLOT_UTIL_H */

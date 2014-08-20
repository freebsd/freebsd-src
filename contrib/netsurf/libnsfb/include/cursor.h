/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the *internal* interface for the cursor. 
 */

#ifndef CURSOR_H
#define CURSOR_H 1

struct nsfb_cursor_s {
    bool plotted;
    nsfb_bbox_t loc;

    /* current cursor image */
    const nsfb_colour_t *pixel;
    int bmp_width;
    int bmp_height;
    int bmp_stride;
    int hotspot_x;
    int hotspot_y;

    /* current saved image */
    nsfb_bbox_t savloc;
    nsfb_colour_t *sav;
    int sav_size;
    int sav_width;
    int sav_height;

};

/** Plot the cursor saving the image underneath. */
bool nsfb_cursor_plot(nsfb_t *nsfb, struct nsfb_cursor_s *cursor);

/** Clear the cursor restoring the image underneath */
bool nsfb_cursor_clear(nsfb_t *nsfb, struct nsfb_cursor_s *cursor);

/** Destroy the cursor */
bool nsfb_cursor_destroy(struct nsfb_cursor_s *cursor);

#endif /* CURSOR_H */

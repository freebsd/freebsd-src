/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the exported interface for the libnsfb graphics library. 
 */

#ifndef _LIBNSFB_CURSOR_H
#define _LIBNSFB_CURSOR_H 1

/** Initialise the cursor.
 */
bool nsfb_cursor_init(nsfb_t *nsfb);

/** Set cursor parameters.
 *
 * Set a cursor bitmap, the cursor will be shown at the location set by
 * nsfb_cursor_loc_set. The pixel data may be referenced untill the cursor
 * is altered or cleared
 *
 * @param nsfb       The frambuffer context
 * @param pixel      The cursor bitmap data
 * @param bmp_width  The width of the cursor bitmap
 * @param bmp_height The height of the cursor bitmap
 * @param bmp_stride The cursor bitmap's row stride
 * @param hotspot_x  Coordinate within cursor image to place over cursor loc
 * @param hotspot_y  Coordinate within cursor image to place over cursor loc
 *
 * (hot_spot_x, hot_spot_y) is from top left.  (0, 0) means top left pixel of
 * cursor bitmap is to be rendered over the cursor location.
 */
bool nsfb_cursor_set(nsfb_t *nsfb, const nsfb_colour_t *pixel, int bmp_width, int bmp_height, int bmp_stride, int hotspot_x, int hotspot_y);

/** Set cursor location.
 *
 * @param nsfb The frambuffer context.
 * @param loc The location of the cursor
 */
bool nsfb_cursor_loc_set(nsfb_t *nsfb, const nsfb_bbox_t *loc);

/** get the cursor location */
bool nsfb_cursor_loc_get(nsfb_t *nsfb, nsfb_bbox_t *loc);


#endif

/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NETSURF_FRAMEBUFFER_OPTIONS_H_
#define _NETSURF_FRAMEBUFFER_OPTIONS_H_

/* currently nothing here */

#endif

/***** surface options *****/

NSOPTION_INTEGER(fb_depth, 32)
NSOPTION_INTEGER(fb_refresh, 70)
NSOPTION_STRING(fb_device, NULL)
NSOPTION_STRING(fb_input_devpath, NULL)
NSOPTION_STRING(fb_input_glob, NULL)

/***** toolkit options *****/

/** toolkit furniture size */
NSOPTION_INTEGER(fb_furniture_size, 18)
/** toolbar furniture size */
NSOPTION_INTEGER(fb_toolbar_size, 30)
/** toolbar layout */
NSOPTION_STRING(fb_toolbar_layout, NULL)
/** enable on screen keyboard */
NSOPTION_BOOL(fb_osk, false)

/***** font options *****/

/** render all fonts monochrome */
NSOPTION_BOOL(fb_font_monochrome, false)
/** size of font glyph cache in kilobytes. */
NSOPTION_INTEGER(fb_font_cachesize, 2048)

/* Font face paths. These are treated as absolute paths if they start
 * with a / otherwise the compile time resource path is searched. 
 */
NSOPTION_STRING(fb_face_sans_serif, NULL)
NSOPTION_STRING(fb_face_sans_serif_bold, NULL)
NSOPTION_STRING(fb_face_sans_serif_italic, NULL)
NSOPTION_STRING(fb_face_sans_serif_italic_bold, NULL)
NSOPTION_STRING(fb_face_serif, NULL)
NSOPTION_STRING(fb_face_serif_bold, NULL)
NSOPTION_STRING(fb_face_monospace, NULL)
NSOPTION_STRING(fb_face_monospace_bold, NULL)
NSOPTION_STRING(fb_face_cursive, NULL)
NSOPTION_STRING(fb_face_fantasy, NULL)

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

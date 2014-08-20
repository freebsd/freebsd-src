/*
 * Copyright 2011 Michael Drake <tlsa@netsurf-browser.org>
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

/** \file
 * Browser window creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_FRAME_TYPES_H_
#define _NETSURF_DESKTOP_FRAME_TYPES_H_

struct frame_dimension {
  	float value;
	enum {
	  	FRAME_DIMENSION_PIXELS,		/* '100', '200' */
	  	FRAME_DIMENSION_PERCENT, 	/* '5%', '20%' */
	  	FRAME_DIMENSION_RELATIVE	/* '*', '2*' */
	} unit;
};

typedef enum {
  	SCROLLING_AUTO,
  	SCROLLING_YES,
  	SCROLLING_NO
} frame_scrolling;

/* Handy struct names */
struct content_html_iframe;
struct content_html_frames;

#endif

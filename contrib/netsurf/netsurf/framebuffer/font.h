/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef NETSURF_FB_FONT_H
#define NETSURF_FB_FONT_H

#include "utils/utf8.h"

extern struct gui_utf8_table *framebuffer_utf8_table;

bool fb_font_init(void);
bool fb_font_finalise(void);

#ifdef FB_USE_FREETYPE
#include "framebuffer/font_freetype.h"
#else
#include "framebuffer/font_internal.h"
#endif

#endif /* NETSURF_FB_FONT_H */


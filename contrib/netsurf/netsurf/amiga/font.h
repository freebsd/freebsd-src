/*
 * Copyright 2008, 2009, 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_FONT_H
#define AMIGA_FONT_H

#include "desktop/plotters.h"
#include <graphics/rastport.h>
#include <graphics/text.h>

struct ami_font_node;

ULONG ami_unicode_text(struct RastPort *rp, const char *string,
	ULONG length, const plot_font_style_t *fstyle, ULONG x, ULONG y, bool aa);
void ami_font_setdevicedpi(int id);
void ami_init_fonts(void);
void ami_close_fonts(void);
void ami_font_close(struct ami_font_node *node);

/* Alternate entry points into font_scan */
void ami_font_initscanner(bool force, bool save);
void ami_font_finiscanner(void);
void ami_font_savescanner(void);

/* Simple diskfont functions for graphics.library use (not page rendering) */
struct TextFont *ami_font_open_disk_font(struct TextAttr *tattr);
void ami_font_close_disk_font(struct TextFont *tfont);
#endif

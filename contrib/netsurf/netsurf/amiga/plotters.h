/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_PLOTTERS_H
#define AMIGA_PLOTTERS_H
#include "desktop/plotters.h"
#include <proto/layers.h>
#include <proto/graphics.h>
#ifdef NS_AMIGA_CAIRO
#include <cairo/cairo.h>
#endif

struct gui_globals
{
	struct BitMap *bm;
	struct RastPort *rp;
	struct Layer_Info *layerinfo;
	APTR areabuf;
	APTR tmprasbuf;
	struct Rectangle rect;
	struct MinList *shared_pens;
#ifdef NS_AMIGA_CAIRO
	cairo_surface_t *surface;
	cairo_t *cr;
#endif
};

extern const struct plotter_table amiplot;

bool ami_clg(colour c);
bool ami_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style);
bool ami_line(int x0, int y0, int x1, int y1, const plot_style_t *style);
bool ami_polygon(const int *p, unsigned int n, const plot_style_t *style);
bool ami_clip(const struct rect *clip);
bool ami_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle);
bool ami_disc(int x, int y, int radius, const plot_style_t *style);
bool ami_arc(int x, int y, int radius, int angle1, int angle2,
	    		const plot_style_t *style);
bool ami_bitmap_tile(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bitmap_flags_t flags);
bool ami_group_start(const char *name);
bool ami_group_end(void);
bool ami_flush(void);
bool ami_path(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6]);

void ami_init_layers(struct gui_globals *gg, ULONG width, ULONG height);
void ami_free_layers(struct gui_globals *gg);
void ami_clearclipreg(struct gui_globals *gg);
void ami_plot_release_pens(struct MinList *shared_pens);
bool ami_plot_screen_is_palettemapped(void);

struct gui_globals *glob;
#endif

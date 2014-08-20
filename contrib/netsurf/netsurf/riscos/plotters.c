/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Target independent plotting (RISC OS screen implementation).
 */

#include <stdbool.h>
#include <math.h>
#include "oslib/colourtrans.h"
#include "oslib/draw.h"
#include "oslib/os.h"
#include "desktop/plotters.h"
#include "render/font.h"
#include "riscos/bitmap.h"
#include "riscos/image.h"
#include "riscos/gui.h"
#include "riscos/oslib_pre7.h"
#include "utils/log.h"


static bool ro_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool ro_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool ro_plot_draw_path(const draw_path * const path, int width,
		colour c, bool dotted, bool dashed);
static bool ro_plot_polygon(const int *p, unsigned int n, const plot_style_t *style);
static bool ro_plot_path(const float *p, unsigned int n, colour fill, float width,
		colour c, const float transform[6]);
static bool ro_plot_clip(const struct rect *clip);
static bool ro_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle);
static bool ro_plot_disc(int x, int y, int radius, const plot_style_t *style);
static bool ro_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		const plot_style_t *style);
static bool ro_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags);


struct plotter_table plot;

const struct plotter_table ro_plotters = {
	.rectangle = ro_plot_rectangle,
	.line = ro_plot_line,
	.polygon = ro_plot_polygon,
	.clip = ro_plot_clip,
	.text = ro_plot_text,
	.disc = ro_plot_disc,
	.arc = ro_plot_arc,
	.bitmap = ro_plot_bitmap,
	.path = ro_plot_path,
	.option_knockout = true,
};

int ro_plot_origin_x = 0;
int ro_plot_origin_y = 0;

/** One version of the A9home OS is incapable of drawing patterned lines */
bool ro_plot_patterned_lines = true;



bool ro_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	os_error *error;

	if (style->fill_type != PLOT_OP_TYPE_NONE) { 
		error = xcolourtrans_set_gcol(style->fill_colour << 8, 
						colourtrans_USE_ECFS_GCOL,
						os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
			     error->errnum, error->errmess));
			return false;
		}

		error = xos_plot(os_MOVE_TO,
				 ro_plot_origin_x + x0 * 2,
				 ro_plot_origin_y - y0 * 2 - 1);
		if (error) {
			LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}

		error = xos_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
				 ro_plot_origin_x + x1 * 2 - 1,
				 ro_plot_origin_y - y1 * 2);
		if (error) {
			LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		bool dotted = false; 
		bool dashed = false;
 
		const int path[] = { draw_MOVE_TO,
					(ro_plot_origin_x + x0 * 2) * 256,
					(ro_plot_origin_y - y0 * 2 - 1) * 256,
					draw_LINE_TO,
					(ro_plot_origin_x + (x1) * 2) * 256,
					(ro_plot_origin_y - y0 * 2 - 1) * 256,
					draw_LINE_TO,
					(ro_plot_origin_x + (x1) * 2) * 256,
					(ro_plot_origin_y - (y1) * 2 - 1) * 256,
					draw_LINE_TO,
					(ro_plot_origin_x + x0 * 2) * 256,
					(ro_plot_origin_y - (y1) * 2 - 1) * 256,
					draw_CLOSE_LINE,
					(ro_plot_origin_x + x0 * 2) * 256,
					(ro_plot_origin_y - y0 * 2 - 1) * 256,
					draw_END_PATH };

		if (style->stroke_type == PLOT_OP_TYPE_DOT) 
			dotted = true;

		if (style->stroke_type == PLOT_OP_TYPE_DASH) 
			dashed = true;

		ro_plot_draw_path((const draw_path *)path, 
				  style->stroke_width, 
				  style->stroke_colour,
				  dotted, dashed);
	}

	return true;
}


bool ro_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	const int path[] = { draw_MOVE_TO,
			(ro_plot_origin_x + x0 * 2) * 256,
			(ro_plot_origin_y - y0 * 2 - 1) * 256,
			draw_LINE_TO,
			(ro_plot_origin_x + x1 * 2) * 256,
			(ro_plot_origin_y - y1 * 2 - 1) * 256,
			draw_END_PATH };
	bool dotted = false; 
	bool dashed = false;

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) 
			dotted = true;

		if (style->stroke_type == PLOT_OP_TYPE_DASH) 
			dashed = true;

		return ro_plot_draw_path((const draw_path *)path, 
					 style->stroke_width, 
					 style->stroke_colour, 
					 dotted, dashed);
	}
	return true;
}


bool ro_plot_draw_path(const draw_path * const path, int width,
		colour c, bool dotted, bool dashed)
{
	static const draw_line_style line_style = { draw_JOIN_MITRED,
			draw_CAP_BUTT, draw_CAP_BUTT, 0, 0x7fffffff,
			0, 0, 0, 0 };
	draw_dash_pattern dash = { 0, 1, { 512 } };
	const draw_dash_pattern *dash_pattern = 0;
	os_error *error;

	if (width < 1)
		width = 1;

	if (ro_plot_patterned_lines) {
		if (dotted) {
			dash.elements[0] = 512 * width;
			dash_pattern = &dash;
		} else if (dashed) {
			dash.elements[0] = 1536 * width;
			dash_pattern = &dash;
		}
	}

	error = xcolourtrans_set_gcol(c << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	error = xdraw_stroke(path, 0, 0, 0, width * 2 * 256,
			&line_style, dash_pattern);
	if (error) {
		LOG(("xdraw_stroke: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	int path[n * 3 + 2];
	unsigned int i;
	os_error *error;

	for (i = 0; i != n; i++) {
		path[i * 3 + 0] = draw_LINE_TO;
		path[i * 3 + 1] = (ro_plot_origin_x + p[i * 2 + 0] * 2) * 256;
		path[i * 3 + 2] = (ro_plot_origin_y - p[i * 2 + 1] * 2) * 256;
	}
	path[0] = draw_MOVE_TO;
	path[n * 3] = draw_END_PATH;
	path[n * 3 + 1] = 0;

	error = xcolourtrans_set_gcol(style->fill_colour << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xdraw_fill((draw_path *) path, 0, 0, 0);
	if (error) {
		LOG(("xdraw_fill: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_path(const float *p, unsigned int n, colour fill, float width,
		colour c, const float transform[6])
{
	static const draw_line_style line_style = { draw_JOIN_MITRED,
			draw_CAP_BUTT, draw_CAP_BUTT, 0, 0x7fffffff,
			0, 0, 0, 0 };
	int *path = 0;
	unsigned int i;
	os_trfm trfm;
	os_error *error;

	if (n == 0)
		return true;

	if (p[0] != PLOTTER_PATH_MOVE) {
		LOG(("path doesn't start with a move"));
		goto error;
	}

	path = malloc(sizeof *path * (n + 10));
	if (!path) {
		LOG(("out of memory"));
		goto error;
	}

	for (i = 0; i < n; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			path[i] = draw_MOVE_TO;
			path[i + 1] = p[i + 1] * 2 * 256;
			path[i + 2] = -p[i + 2] * 2 * 256;
			i += 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			path[i] = draw_CLOSE_LINE;
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			path[i] = draw_LINE_TO;
			path[i + 1] = p[i + 1] * 2 * 256;
			path[i + 2] = -p[i + 2] * 2 * 256;
			i += 3;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			path[i] = draw_BEZIER_TO;
			path[i + 1] = p[i + 1] * 2 * 256;
			path[i + 2] = -p[i + 2] * 2 * 256;
			path[i + 3] = p[i + 3] * 2 * 256;
			path[i + 4] = -p[i + 4] * 2 * 256;
			path[i + 5] = p[i + 5] * 2 * 256;
			path[i + 6] = -p[i + 6] * 2 * 256;
			i += 7;
		} else {
			LOG(("bad path command %f", p[i]));
			goto error;
		}
	}
	path[i] = draw_END_PATH;
	path[i + 1] = 0;

	trfm.entries[0][0] = transform[0] * 0x10000;
	trfm.entries[0][1] = transform[1] * 0x10000;
	trfm.entries[1][0] = transform[2] * 0x10000;
	trfm.entries[1][1] = transform[3] * 0x10000;
	trfm.entries[2][0] = (ro_plot_origin_x + transform[4] * 2) * 256;
	trfm.entries[2][1] = (ro_plot_origin_y - transform[5] * 2) * 256;

	if (fill != NS_TRANSPARENT) {
		error = xcolourtrans_set_gcol(fill << 8, 0,
				os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
					error->errnum, error->errmess));
			goto error;
		}

		error = xdraw_fill((draw_path *) path, 0, &trfm, 0);
		if (error) {
			LOG(("xdraw_stroke: 0x%x: %s",
					error->errnum, error->errmess));
			goto error;
		}
	}

	if (c != NS_TRANSPARENT) {
		error = xcolourtrans_set_gcol(c << 8, 0,
				os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
					error->errnum, error->errmess));
			goto error;
		}

		error = xdraw_stroke((draw_path *) path, 0, &trfm, 0,
				width * 2 * 256, &line_style, 0);
		if (error) {
			LOG(("xdraw_stroke: 0x%x: %s",
					error->errnum, error->errmess));
			goto error;
		}
	}

	free(path);
	return true;

error:
	free(path);
	return false;
}




bool ro_plot_clip(const struct rect *clip)
{
	os_error *error;
	char buf[12];

	int clip_x0 = ro_plot_origin_x + clip->x0 * 2;
	int clip_y0 = ro_plot_origin_y - clip->y0 * 2 - 1;
	int clip_x1 = ro_plot_origin_x + clip->x1 * 2 - 1;
	int clip_y1 = ro_plot_origin_y - clip->y1 * 2;

	if (clip_x1 < clip_x0 || clip_y0 < clip_y1) {
		LOG(("bad clip rectangle %i %i %i %i",
				clip_x0, clip_y0, clip_x1, clip_y1));
		return false;
	}

	buf[0] = os_VDU_SET_GRAPHICS_WINDOW;
	buf[1] = clip_x0;
	buf[2] = clip_x0 >> 8;
	buf[3] = clip_y1;
	buf[4] = clip_y1 >> 8;
	buf[5] = clip_x1;
	buf[6] = clip_x1 >> 8;
	buf[7] = clip_y0;
	buf[8] = clip_y0 >> 8;

	error = xos_writen(buf, 9);
	if (error) {
		LOG(("xos_writen: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle)
{
	os_error *error;

	error = xcolourtrans_set_font_colours(font_CURRENT,
			fstyle->background << 8, fstyle->foreground << 8, 
			14, 0, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return nsfont_paint(fstyle, text, length,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2);
}


bool ro_plot_disc(int x, int y, int radius, const plot_style_t *style)
{
	os_error *error;
	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		error = xcolourtrans_set_gcol(style->fill_colour << 8, 0,
					      os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
			     error->errnum, error->errmess));
			return false;
		}
		error = xos_plot(os_MOVE_TO,
				 ro_plot_origin_x + x * 2,
				 ro_plot_origin_y - y * 2);
		if (error) {
			LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}
		error = xos_plot(os_PLOT_CIRCLE | os_PLOT_BY, radius * 2, 0);
		if (error) {
			LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}
        }

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {

		error = xcolourtrans_set_gcol(style->stroke_colour << 8, 0,
					      os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
			     error->errnum, error->errmess));
			return false;
		}
		error = xos_plot(os_MOVE_TO,
				 ro_plot_origin_x + x * 2,
				 ro_plot_origin_y - y * 2);
		if (error) {
			LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}
		error = xos_plot(os_PLOT_CIRCLE_OUTLINE | os_PLOT_BY,
				 radius * 2, 0);

		if (error) {
			LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
			return false;
		}
        }
	return true;
}

bool ro_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
	os_error *error;
	int sx, sy, ex, ey;
	double t;

	x = ro_plot_origin_x + x * 2;
	y = ro_plot_origin_y - y * 2;
	radius <<= 1;

	error = xcolourtrans_set_gcol(style->fill_colour << 8, 0,
	    		os_ACTION_OVERWRITE, 0, 0);

	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	t = ((double)angle1 * M_PI) / 180.0;
	sx = (x + (int)(radius * cos(t)));
	sy = (y + (int)(radius * sin(t)));

	t = ((double)angle2 * M_PI) / 180.0;
	ex = (x + (int)(radius * cos(t)));
	ey = (y + (int)(radius * sin(t)));

        error = xos_plot(os_MOVE_TO, x, y);	/* move to centre */
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	error = xos_plot(os_MOVE_TO, sx, sy);	/* move to start */
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	error = xos_plot(os_PLOT_ARC | os_PLOT_TO, ex, ey);	/* arc to end */
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}



bool ro_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags)
{
	const uint8_t *buffer;

	buffer = bitmap_get_buffer(bitmap);
	if (!buffer) {
		LOG(("bitmap_get_buffer failed"));
		return false;
	}

	return image_redraw(bitmap->sprite_area,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2,
			width, height,
			bitmap->width,
			bitmap->height,
			bg,
			flags & BITMAPF_REPEAT_X, flags & BITMAPF_REPEAT_Y,
			flags & BITMAPF_REPEAT_X || flags & BITMAPF_REPEAT_Y,
			bitmap_get_opaque(bitmap) ? IMAGE_PLOT_TINCT_OPAQUE :
			IMAGE_PLOT_TINCT_ALPHA);
}

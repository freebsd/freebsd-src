/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004-2008 John Tytgat <joty@netsurf-browser.org>
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
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
 * Export a content as a DrawFile (implementation).
 */

#ifdef WITH_DRAW_EXPORT

#include <assert.h>
#include <limits.h>
#include "oslib/draw.h"
#include "oslib/osfile.h"
#include "pencil.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/plotters.h"
#include "riscos/bitmap.h"
#include "riscos/gui.h"
#include "riscos/save_draw.h"
#include "utils/log.h"
#include "utils/types.h"
#include "utils/utils.h"

static bool ro_save_draw_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool ro_save_draw_line(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool ro_save_draw_polygon(const int *p, unsigned int n, const plot_style_t *style);
static bool ro_save_draw_path(const float *p, unsigned int n, colour fill,
		float width, colour c, const float transform[6]);
static bool ro_save_draw_clip(const struct rect *clip);
static bool ro_save_draw_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle);
static bool ro_save_draw_disc(int x, int y, int radius, const plot_style_t *style);
static bool ro_save_draw_arc(int x, int y, int radius, int angle1, int angle2,
    		const plot_style_t *style);
static bool ro_save_draw_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, bitmap_flags_t flags);
static bool ro_save_draw_group_start(const char *name);
static bool ro_save_draw_group_end(void);
static bool ro_save_draw_error(pencil_code code);


static const struct plotter_table ro_save_draw_plotters = {
	.rectangle = ro_save_draw_rectangle,
	.line = ro_save_draw_line,
	.polygon = ro_save_draw_polygon,
	.clip = ro_save_draw_clip,
	.text = ro_save_draw_text,
	.disc = ro_save_draw_disc,
	.arc = ro_save_draw_arc,
	.bitmap = ro_save_draw_bitmap,
	.group_start = ro_save_draw_group_start,
	.group_end = ro_save_draw_group_end,
	.path = ro_save_draw_path,
	.option_knockout = false,
};

static struct pencil_diagram *ro_save_draw_diagram;
static int ro_save_draw_width;
static int ro_save_draw_height;


/**
 * Export a content as a DrawFile.
 *
 * \param  h     content to export
 * \param  path  path to save DrawFile as
 * \return  true on success, false on error and error reported
 */

bool save_as_draw(hlcache_handle *h, const char *path)
{
	pencil_code code;
	char *drawfile_buffer;
	struct rect clip;
	struct content_redraw_data data;
	size_t drawfile_size;
	os_error *error;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &ro_save_draw_plotters
	};

	ro_save_draw_diagram = pencil_create();
	if (!ro_save_draw_diagram) {
		warn_user("NoMemory", 0);
		return false;
	}

	ro_save_draw_width = content_get_width(h);
	ro_save_draw_height = content_get_height(h);

	clip.x0 = clip.y0 = INT_MIN;
	clip.x1 = clip.y1 = INT_MAX;

	data.x = 0;
	data.y = -ro_save_draw_height;
	data.width = ro_save_draw_width;
	data.height = ro_save_draw_height;
	data.background_colour = 0xFFFFFF;
	data.scale = 1;
	data.repeat_x = false;
	data.repeat_y = false;

	if (!content_redraw(h, &data, &clip, &ctx)) {
		pencil_free(ro_save_draw_diagram);
		return false;
	}

	/*pencil_dump(ro_save_draw_diagram);*/

	code = pencil_save_drawfile(ro_save_draw_diagram, "NetSurf",
			&drawfile_buffer, &drawfile_size);
	if (code != pencil_OK) {
		warn_user("SaveError", 0);
		pencil_free(ro_save_draw_diagram);
		return false;
	}
	assert(drawfile_buffer);

	error = xosfile_save_stamped(path, osfile_TYPE_DRAW,
			(byte *) drawfile_buffer, 
			(byte *) drawfile_buffer + drawfile_size);
	if (error) {
		LOG(("xosfile_save_stamped failed: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		pencil_free(ro_save_draw_diagram);
		return false;
	}

	pencil_free(ro_save_draw_diagram);

	return true;
}

bool ro_save_draw_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	pencil_code code;
	const int path[] = { draw_MOVE_TO, x0 * 2, -y0 * 2 - 1,
			     draw_LINE_TO, x1 * 2, -y0 * 2 - 1,
			     draw_LINE_TO, x1 * 2, -y1 * 2 - 1,
			     draw_LINE_TO, x0 * 2, -y1 * 2 - 1,
			     draw_CLOSE_LINE,
			     draw_END_PATH };

        if (style->fill_type != PLOT_OP_TYPE_NONE) { 

		code = pencil_path(ro_save_draw_diagram, 
				   path,
				   sizeof path / sizeof path[0],
				   style->fill_colour << 8, 
				   pencil_TRANSPARENT, 
				   0, 
				   pencil_JOIN_MITRED,
				   pencil_CAP_BUTT, 
				   pencil_CAP_BUTT, 
				   0, 
				   0, 
				   false,
				   pencil_SOLID);
		if (code != pencil_OK)
			return ro_save_draw_error(code);
	}

        if (style->stroke_type != PLOT_OP_TYPE_NONE) { 

		code = pencil_path(ro_save_draw_diagram, 
				   path,
				   sizeof path / sizeof path[0],
				   pencil_TRANSPARENT, 
				   style->stroke_colour << 8, 
				   style->stroke_width, 
				   pencil_JOIN_MITRED,
				   pencil_CAP_BUTT, 
				   pencil_CAP_BUTT, 
				   0, 
				   0, 
				   false,
				   pencil_SOLID);

		if (code != pencil_OK)
			return ro_save_draw_error(code);
	}
	return true;
}


bool ro_save_draw_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	pencil_code code;
	const int path[] = { draw_MOVE_TO, x0 * 2, -y0 * 2 - 1,
			     draw_LINE_TO, x1 * 2, -y1 * 2 - 1,
			     draw_END_PATH };

	code = pencil_path(ro_save_draw_diagram, 
			   path,
			   sizeof path / sizeof path[0],
			   pencil_TRANSPARENT, 
			   style->stroke_colour << 8, 
			   style->stroke_width, 
			   pencil_JOIN_MITRED,
			   pencil_CAP_BUTT, 
			   pencil_CAP_BUTT, 
			   0, 0, false,
			   pencil_SOLID);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	pencil_code code;
	int path[n * 3 + 1];
	unsigned int i;

	for (i = 0; i != n; i++) {
		path[i * 3 + 0] = draw_LINE_TO;
		path[i * 3 + 1] = p[i * 2 + 0] * 2;
		path[i * 3 + 2] = -p[i * 2 + 1] * 2;
	}
	path[0] = draw_MOVE_TO;
	path[n * 3] = draw_END_PATH;

	code = pencil_path(ro_save_draw_diagram, 
			   path, n * 3 + 1,
			   style->fill_colour << 8, 
			   pencil_TRANSPARENT, 
			   0, 
			   pencil_JOIN_MITRED,
			   pencil_CAP_BUTT, 
			   pencil_CAP_BUTT, 
			   0, 
			   0, 
			   false,
			   pencil_SOLID);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_path(const float *p, unsigned int n, colour fill,
		float width, colour c, const float transform[6])
{
	if (n == 0)
		return true;

	if (p[0] != PLOTTER_PATH_MOVE) {
		LOG(("path doesn't start with a move"));
		return false;
	}

	int *path = malloc(sizeof *path * (n + 10));
	if (!path) {
		LOG(("out of memory"));
		return false;
	}

	unsigned int i;
	bool empty_path = true;
	for (i = 0; i < n; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			path[i] = draw_MOVE_TO;
			path[i + 1] = (transform[0] * p[i + 1] +
					transform[2] * -p[i + 2] +
					transform[4]) * 2;
			path[i + 2] = (transform[1] * p[i + 1] +
					transform[3] * -p[i + 2] +
					-transform[5]) * 2;
			i += 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			path[i] = draw_CLOSE_LINE;
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			path[i] = draw_LINE_TO;
			path[i + 1] = (transform[0] * p[i + 1] +
					transform[2] * -p[i + 2] +
					transform[4]) * 2;
			path[i + 2] = (transform[1] * p[i + 1] +
					transform[3] * -p[i + 2] +
					-transform[5]) * 2;
			i += 3;
			empty_path = false;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			path[i] = draw_BEZIER_TO;
			path[i + 1] = (transform[0] * p[i + 1] +
					transform[2] * -p[i + 2] +
					transform[4]) * 2;
			path[i + 2] = (transform[1] * p[i + 1] +
					transform[3] * -p[i + 2] +
					-transform[5]) * 2;
			path[i + 3] = (transform[0] * p[i + 3] +
					transform[2] * -p[i + 4] +
					transform[4]) * 2;
			path[i + 4] = (transform[1] * p[i + 3] +
					transform[3] * -p[i + 4] +
					-transform[5]) * 2;
			path[i + 5] = (transform[0] * p[i + 5] +
					transform[2] * -p[i + 6] +
					transform[4]) * 2;
			path[i + 6] = (transform[1] * p[i + 5] +
					transform[3] * -p[i + 6] +
					-transform[5]) * 2;
			i += 7;
			empty_path = false;
		} else {
			LOG(("bad path command %f", p[i]));
			free(path);
			return false;
		}
	}
	path[i] = draw_END_PATH;

	if (empty_path) {
		free(path);
		return true;
	}

	pencil_code code = pencil_path(ro_save_draw_diagram, path, i + 1,
			fill == NS_TRANSPARENT ? pencil_TRANSPARENT : fill << 8,
			c == NS_TRANSPARENT ? pencil_TRANSPARENT : c << 8,
			width, pencil_JOIN_MITRED,
			pencil_CAP_BUTT, pencil_CAP_BUTT, 0, 0, false,
			pencil_SOLID);
	free(path);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}




bool ro_save_draw_clip(const struct rect *clip)
{
	return true;
}


bool ro_save_draw_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle)
{
	pencil_code code;
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;

	nsfont_read_style(fstyle, &font_family, &font_size, &font_style);

	code = pencil_text(ro_save_draw_diagram, x * 2, -y * 2, font_family,
			font_style, font_size, text, length, 
			fstyle->foreground << 8);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_disc(int x, int y, int radius, const plot_style_t *style)
{
	return true;
}

bool ro_save_draw_arc(int x, int y, int radius, int angle1, int angle2,
		const plot_style_t *style)
{
	return true;
}

bool ro_save_draw_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, bitmap_flags_t flags)
{
	pencil_code code;
	const uint8_t *buffer;

	buffer = bitmap_get_buffer(bitmap);
	if (!buffer) {
		warn_user("NoMemory", 0);
		return false;
	}

	code = pencil_sprite(ro_save_draw_diagram, x * 2, (-y - height) * 2,
			width * 2, height * 2,
			((char *) bitmap->sprite_area) +
			bitmap->sprite_area->first);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_group_start(const char *name)
{
	pencil_code code;

	code = pencil_group_start(ro_save_draw_diagram, name);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_group_end(void)
{
	pencil_code code;

	code = pencil_group_end(ro_save_draw_diagram);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


/**
 * Report an error from pencil.
 *
 * \param  code  error code
 * \return  false
 */

bool ro_save_draw_error(pencil_code code)
{
	LOG(("code %i", code));

	switch (code) {
	case pencil_OK:
		assert(0);
		break;
	case pencil_OUT_OF_MEMORY:
		warn_user("NoMemory", 0);
		break;
	case pencil_FONT_MANAGER_ERROR:
		warn_user("SaveError", rufl_fm_error->errmess);
		break;
	case pencil_FONT_NOT_FOUND:
	case pencil_IO_ERROR:
	case pencil_IO_EOF:
		warn_user("SaveError", "generating the DrawFile failed");
		break;
	}

	return false;
}

#endif

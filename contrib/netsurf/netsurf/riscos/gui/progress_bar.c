/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Progress bar (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "swis.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"

#include "desktop/plotters.h"
#include "utils/log.h"
#include "utils/utils.h"

#include "riscos/gui.h"
#include "riscos/tinct.h"
#include "riscos/wimp_event.h"
#include "riscos/gui/progress_bar.h"

#define MARGIN 6

struct progress_bar {
	wimp_w w;			/**< progress bar window handle */
	unsigned int range;		/**< progress bar range */
	unsigned int value;		/**< progress bar value */
	char icon[13];			/**< current icon */
	int offset;			/**< progress bar rotation */
	os_box visible;			/**< progress bar position */
	int icon_x0;			/**< icon x0 */
	int icon_y0;			/**< icon y0 */
	osspriteop_header *icon_img;	/**< icon image */
	bool animating;			/**< progress bar is animating */
	bool recalculate;		/**< recalculation required */
	int cur_width;			/**< current calculated width */
	int cur_height;			/**< current calculated height */
	bool icon_obscured;		/**< icon is partially obscured */
};

static char progress_animation_sprite[] = "progress";
static osspriteop_header *progress_icon;
static unsigned int progress_width;
static unsigned int progress_height;

struct wimp_window_base progress_bar_definition = {
	{0, 0, 1, 1},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE | wimp_WINDOW_NO_BOUNDS,
	0xff,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	wimp_WINDOW_NEVER3D | 0x16u /* RISC OS 5.03+ */,
	{0, 0, 65535, 65535},
	0,
	0,
	wimpspriteop_AREA,
	1,
	1,
	{""},
	0
};


static void ro_gui_progress_bar_calculate(struct progress_bar *pb, int width,
		int height);
static void ro_gui_progress_bar_redraw(wimp_draw *redraw);
static void ro_gui_progress_bar_redraw_window(wimp_draw *redraw,
		struct progress_bar *pb);
static void ro_gui_progress_bar_animate(void *p);


/**
 * Initialise the progress bar
 *
 * \param  icons  the sprite area to use for icons
 */
void ro_gui_progress_bar_init(osspriteop_area *icons)
{
	const char *name = progress_animation_sprite;
	os_error *error;

	progress_bar_definition.sprite_area = icons;

	progress_icon = NULL;
	error = xosspriteop_select_sprite(osspriteop_USER_AREA,
			progress_bar_definition.sprite_area,
			(osspriteop_id) name, &progress_icon);
	if (!error) {
		xosspriteop_read_sprite_info(osspriteop_USER_AREA,
			progress_bar_definition.sprite_area,
			(osspriteop_id) name,
			(int *) &progress_width, (int *) &progress_height, 
			0, 0);
	}
}


/**
 * Create a new progress bar
 */
struct progress_bar *ro_gui_progress_bar_create(void)
{
	struct progress_bar *pb;
	os_error *error;

	pb = calloc(1, sizeof(*pb));
	if (!pb)
		return NULL;

	error = xwimp_create_window((wimp_window *)&progress_bar_definition,
				&pb->w);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(pb);
		return NULL;
	}

	ro_gui_wimp_event_register_redraw_window(pb->w,
			ro_gui_progress_bar_redraw);
	ro_gui_wimp_event_set_user_data(pb->w, pb);
	return pb;
}


/**
 * Destroy a progress bar and free all associated resources
 *
 * \param  pb  the progress bar to destroy
 */
void ro_gui_progress_bar_destroy(struct progress_bar *pb)
{
	os_error *error;
	assert(pb);

	if (pb->animating) {
		riscos_schedule(-1, ro_gui_progress_bar_animate, pb);
	}
	ro_gui_wimp_event_finalise(pb->w);
	error = xwimp_delete_window(pb->w);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
			error->errnum, error->errmess));
	}

	free(pb);
}


/**
 * Get the handle of the window that represents a progress bar
 *
 * \param  pb  the progress bar to get the window handle of
 * \return the progress bar's window handle
 */
wimp_w ro_gui_progress_bar_get_window(struct progress_bar *pb)
{
	assert(pb);

	return pb->w;
}


/**
 * Set the icon for a progress bar
 *
 * \param  pb  the progress bar to set the icon for
 * \param  icon  the icon to use, or NULL for no icon
 */
void ro_gui_progress_bar_set_icon(struct progress_bar *pb, const char *icon)
{
	assert(pb);

	if (!strcmp(icon, pb->icon))
		return;
	if (!icon)
		pb->icon[0] = '\0';
	else {
		strncpy(pb->icon, icon, 12);
		pb->icon[12] = '\0';
	}
	pb->recalculate = true;
	xwimp_force_redraw(pb->w, 0, 0, 32, 32);
	ro_gui_progress_bar_update(pb, pb->cur_width, pb->cur_height);
}


/**
 * Set the value of a progress bar
 *
 * \param  pb  the progress bar to set the value for
 * \param  value  the value to use
 */
void ro_gui_progress_bar_set_value(struct progress_bar *pb, unsigned int value)
{
	assert(pb);

	pb->value = value;
	if (pb->value > pb->range)
		pb->range = pb->value;
	ro_gui_progress_bar_update(pb, pb->cur_width, pb->cur_height);
}


/**
 * Get the value of a progress bar
 *
 * \param  pb  the progress bar to get the value of
 * \return the current value
 */
unsigned int ro_gui_progress_bar_get_value(struct progress_bar *pb)
{
	assert(pb);

	return pb->value;
}


/**
 * Set the range of a progress bar
 *
 * \param  pb  the progress bar to set the range for
 * \param  range  the range to use
 */
void ro_gui_progress_bar_set_range(struct progress_bar *pb, unsigned int range)
{
	assert(pb);

	pb->range = range;
	if (pb->value > pb->range)
		pb->value = pb->range;
	ro_gui_progress_bar_update(pb, pb->cur_width, pb->cur_height);
}


/**
 * Get the range of a progress bar
 *
 * \param  pb  the progress bar to get the range of
 * \return the current range
 */
unsigned int ro_gui_progress_bar_get_range(struct progress_bar *pb)
{
	assert(pb);

	return pb->range;
}


/**
 * Update the progress bar to a new dimension.
 *
 * \param  pb  the progress bar to update
 * \param  width  the new progress bar width
 * \param  height  the new progress bar height
 */
void ro_gui_progress_bar_update(struct progress_bar *pb, int width, int height)
{
  	wimp_draw redraw;
	os_error *error;
	osbool more;
  	os_box cur;

	/* don't allow negative dimensions */
	width = max(width, 0);
	height = max(height, 0);

 	/* update the animation state */
	if ((pb->value == 0) || (pb->value == pb->range)) {
		if (pb->animating) {
			riscos_schedule(-1, ro_gui_progress_bar_animate, pb);
		}
		pb->animating = false;
	} else {
	  	if (!pb->animating) {
			riscos_schedule(200, ro_gui_progress_bar_animate, pb);
		}
		pb->animating = true;
	}

  	/* get old and new positions */
  	cur = pb->visible;
  	pb->recalculate = true;
  	ro_gui_progress_bar_calculate(pb, width, height);

  	/* see if the progress bar hasn't moved. we don't need to consider
  	 * the left edge moving as this is handled by the icon setting
  	 * function */
  	if (cur.x1 == pb->visible.x1)
  		return;

  	/* if size has decreased then we must force a redraw */
  	if (cur.x1 > pb->visible.x1) {
  		xwimp_force_redraw(pb->w, pb->visible.x1, pb->visible.y0,
  				cur.x1, pb->visible.y1);
  		return;
  	}

  	/* perform a minimal redraw update */
	redraw.w = pb->w;
	redraw.box = pb->visible;
	redraw.box.x0 = cur.x1;
	error = xwimp_update_window(&redraw, &more);
	if (more)
		ro_gui_progress_bar_redraw_window(&redraw, pb);
}


/**
 * Process a WIMP redraw request
 *
 * \param  redraw  the redraw request to process
 */
void ro_gui_progress_bar_redraw(wimp_draw *redraw)
{
	struct progress_bar *pb;
	os_error *error;
	osbool more;

	pb = (struct progress_bar *)ro_gui_wimp_event_get_user_data(redraw->w);
	assert(pb);

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
	if (more)
		ro_gui_progress_bar_redraw_window(redraw, pb);
}


/**
 * Animate the progress bar
 *
 * \param  p  the progress bar to animate
 */
void ro_gui_progress_bar_animate(void *p)
{
  	wimp_draw redraw;
	os_error *error;
	osbool more;
	struct progress_bar *pb = p;

	if (!progress_icon)
		return;
	pb->offset -= 6;
	if (pb->offset < 0)
		pb->offset += progress_width * 2;

	if (pb->animating) {
		riscos_schedule(200, ro_gui_progress_bar_animate, pb);
	}

	redraw.w = pb->w;
	redraw.box = pb->visible;
	error = xwimp_update_window(&redraw, &more);
	if (more)
		ro_gui_progress_bar_redraw_window(&redraw, pb);
}


/**
 * Calculate the position of the progress bar
 *
 * \param  pb  the progress bar to recalculate
 * \param  width  the width of the progress bar
 * \param  height  the height of the progress bar
 * \return the address of the associated icon, or NULL
 */
void ro_gui_progress_bar_calculate(struct progress_bar *pb, int width,
		int height)
{
	os_error *error;
	int icon_width, icon_height;
	int icon_x0 = 0, icon_y0 = 0, progress_x0, progress_x1, progress_ymid = 0;
	osspriteop_header *icon = NULL;
	bool icon_redraw = false;

	/* try to use cached values */
	if ((!pb->recalculate) && (pb->cur_width == width) &&
			(pb->cur_height == height))
		return;

	/* update cache status */
	pb->recalculate = false;
	pb->cur_width = width;
	pb->cur_height = height;

	/* get the window dimensions */
	width -= MARGIN * 2;
	icon_width = icon_height = 0;
	progress_x0 = MARGIN;

	/* get the icon information */
	if (progress_bar_definition.sprite_area != wimpspriteop_AREA) {
		progress_ymid = height / 2;
		error = xosspriteop_read_sprite_info(osspriteop_USER_AREA,
				progress_bar_definition.sprite_area,
				(osspriteop_id)pb->icon,
				&icon_width, &icon_height, 0, 0);
		error = xosspriteop_select_sprite(osspriteop_USER_AREA,
				progress_bar_definition.sprite_area,
				(osspriteop_id)pb->icon, &icon);
		if (!error) {
			progress_x0 += 32 + MARGIN;
			width -= 32 + MARGIN;
			icon_x0 = MARGIN + 16 - icon_width;
			icon_y0 = progress_ymid - icon_height;
			if (width < -MARGIN) {
				icon_x0 += width + MARGIN;
				icon_redraw = true;
			}
		}
	}

	/* update the icon */
	if ((pb->icon_obscured) || (icon_redraw)) {
		if (icon_x0 != pb->icon_x0)
			xwimp_force_redraw(pb->w, 0, 0, 32 + MARGIN, 65536);
	}
	pb->icon_obscured = icon_redraw;

	progress_x1 = progress_x0;
	if ((pb->range > 0) && (width > 0))
		progress_x1 += (width * pb->value) / pb->range;

	pb->visible.x0 = progress_x0;
	pb->visible.y0 = MARGIN;
	pb->visible.x1 = progress_x1;
	pb->visible.y1 = height - MARGIN;
	pb->icon_x0 = icon_x0;
	pb->icon_y0 = icon_y0;
	pb->icon_img = icon;
}


/**
 * Redraw a section of a progress bar window
 *
 * \param  redraw  the section of the window to redraw
 * \param  pb  the progress bar to redraw
 */
void ro_gui_progress_bar_redraw_window(wimp_draw *redraw,
		struct progress_bar *pb)
{
	os_error *error;
	osbool more = true;
	struct rect clip;
	int progress_ymid;

	/* initialise the plotters */
  	ro_plot_origin_x = 0;
  	ro_plot_origin_y = 0;

	/* recalculate the progress bar */
	ro_gui_progress_bar_calculate(pb, redraw->box.x1 - redraw->box.x0,
			redraw->box.y1 - redraw->box.y0);
	progress_ymid = redraw->box.y0 + pb->visible.y0 +
			((pb->visible.y1 - pb->visible.y0) >> 1);

	/* redraw the window */
	while (more) {
		if (pb->icon)
			_swix(Tinct_PlotAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(7),
					pb->icon_img,
					redraw->box.x0 + pb->icon_x0,
					redraw->box.y0 + pb->icon_y0,
					tinct_ERROR_DIFFUSE);
		if (!pb->icon_obscured) {
		  	clip.x0 = max(redraw->clip.x0,
		  			redraw->box.x0 + pb->visible.x0) >> 1;
			clip.y0 = -min(redraw->clip.y1,
		  			redraw->box.y0 + pb->visible.y1) >> 1;
			clip.x1 = min(redraw->clip.x1,
					redraw->box.x0 + pb->visible.x1) >> 1;
		  	clip.y1 = -max(redraw->clip.y0,
					redraw->box.y0 + pb->visible.y0) >> 1;
		  	if ((clip.x0 < clip.x1) && (clip.y0 < clip.y1)) {
				if (progress_icon) {
			  		ro_plotters.clip(&clip);
					_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
							progress_icon,
							redraw->box.x0 - pb->offset,
							progress_ymid - progress_height,
							tinct_FILL_HORIZONTALLY);
				} else {
				  	ro_plotters.rectangle(clip.x0, clip.y0, 
						       clip.x1, clip.y1,
						       plot_style_fill_red);
			  	}
			}
		}
		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
	}
}

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

#include <stdbool.h>
#include "swis.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "utils/nsoption.h"
#include "riscos/configure/configure.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/tinct.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/utils.h"


#define IMAGE_FOREGROUND_FIELD 3
#define IMAGE_FOREGROUND_MENU 4
#define IMAGE_BACKGROUND_FIELD 6
#define IMAGE_BACKGROUND_MENU 7
#define IMAGE_CURRENT_DISPLAY 8
#define IMAGE_SPEED_TEXT 11
#define IMAGE_SPEED_FIELD 12
#define IMAGE_SPEED_DEC 13
#define IMAGE_SPEED_INC 14
#define IMAGE_SPEED_CS 15
#define IMAGE_DISABLE_ANIMATION 16
#define IMAGE_DEFAULT_BUTTON 17
#define IMAGE_CANCEL_BUTTON 18
#define IMAGE_OK_BUTTON 19

static bool ro_gui_options_image_click(wimp_pointer *pointer);
static bool ro_gui_options_image_ok(wimp_w w);
static void ro_gui_options_image_redraw(wimp_draw *redraw);
static bool ro_gui_options_image_update(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_selection *s, menu_action a);
static void ro_gui_options_image_read(wimp_w w, unsigned int *bg,
		unsigned int *fg);
static void ro_gui_options_update_shading(wimp_w w);

static osspriteop_area *example_images;
int example_users = 0;
unsigned int tinct_options[] = {tinct_USE_OS_SPRITE_OP, 0, tinct_DITHER,
		tinct_ERROR_DIFFUSE};

bool ro_gui_options_image_initialise(wimp_w w)
{
	char pathname[256];
	int i;

	/* load the sprite file */
	if (example_users == 0) {
		snprintf(pathname, 256, "%s.Resources.Image", NETSURF_DIR);
		pathname[255] = '\0';
		example_images = ro_gui_load_sprite_file(pathname);
		if (!example_images)
			return false;
	}
	example_users++;

	/* set the current values */
	for (i = 0; (i < 4); i++) {
		if ((unsigned int)nsoption_int(plot_fg_quality) == tinct_options[i])
			ro_gui_set_icon_string(w, IMAGE_FOREGROUND_FIELD,
					image_quality_menu->entries[i].
						data.indirected_text.text, true);
		if ((unsigned int)nsoption_int(plot_bg_quality) == tinct_options[i])
			ro_gui_set_icon_string(w, IMAGE_BACKGROUND_FIELD,
					image_quality_menu->entries[i].
						data.indirected_text.text, true);
	}
	ro_gui_set_icon_decimal(w, IMAGE_SPEED_FIELD,
				nsoption_int(minimum_gif_delay), 2);
	ro_gui_set_icon_selected_state(w, IMAGE_DISABLE_ANIMATION,
				       !nsoption_bool(animate_images));
	ro_gui_options_update_shading(w);

	/* register icons */
	ro_gui_wimp_event_register_menu_gright(w, IMAGE_FOREGROUND_FIELD,
			IMAGE_FOREGROUND_MENU, image_quality_menu);
	ro_gui_wimp_event_register_menu_gright(w, IMAGE_BACKGROUND_FIELD,
			IMAGE_BACKGROUND_MENU, image_quality_menu);
	ro_gui_wimp_event_register_text_field(w, IMAGE_SPEED_TEXT);
	ro_gui_wimp_event_register_numeric_field(w, IMAGE_SPEED_FIELD,
			IMAGE_SPEED_INC, IMAGE_SPEED_DEC, 0, 6000, 10, 2);
	ro_gui_wimp_event_register_checkbox(w, IMAGE_DISABLE_ANIMATION);
	ro_gui_wimp_event_register_text_field(w, IMAGE_SPEED_CS);
	ro_gui_wimp_event_register_redraw_window(w,
			ro_gui_options_image_redraw);
	ro_gui_wimp_event_register_mouse_click(w,
			ro_gui_options_image_click);
	ro_gui_wimp_event_register_menu_selection(w,
			ro_gui_options_image_update);
	ro_gui_wimp_event_register_cancel(w, IMAGE_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, IMAGE_OK_BUTTON,
			ro_gui_options_image_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpImageConfig");
	ro_gui_wimp_event_memorise(w);

	return true;
}

void ro_gui_options_image_finalise(wimp_w w)
{
	example_users--;
	if (example_users == 0) {
	  	free(example_images);
	  	example_images = NULL;
	}
	ro_gui_wimp_event_finalise(w);
}

bool ro_gui_options_image_update(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_selection *s, menu_action a)
{
	ro_gui_redraw_icon(w, IMAGE_CURRENT_DISPLAY);

	return true;
}

void ro_gui_options_image_redraw(wimp_draw *redraw)
{
	osbool more;
	int origin_x, origin_y;
	os_error *error;
	wimp_icon_state icon_state;
	osspriteop_header *bg = NULL, *fg = NULL;
	unsigned int bg_tinct = 0, fg_tinct = 0;

	/* get the icon location */
	icon_state.w = redraw->w;
	icon_state.i = IMAGE_CURRENT_DISPLAY;
	error = xwimp_get_icon_state(&icon_state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	/* find the sprites */
	if (example_images) {
		ro_gui_options_image_read(redraw->w, &bg_tinct, &fg_tinct);
		fg_tinct |= 0xeeeeee00;
		xosspriteop_select_sprite(osspriteop_USER_AREA,
				example_images, (osspriteop_id)"img_bg", &bg);
		xosspriteop_select_sprite(osspriteop_USER_AREA,
				example_images, (osspriteop_id)"img_fg", &fg);
	}

	/* perform the redraw */
	more = wimp_redraw_window(redraw);
	while (more) {
		origin_x = redraw->box.x0 - redraw->xscroll +
				icon_state.icon.extent.x0 + 2;
		origin_y = redraw->box.y1 - redraw->yscroll +
				icon_state.icon.extent.y0 + 2;
		if (bg)
			_swix(Tinct_Plot, _INR(2,4) | _IN(7),
					bg, origin_x, origin_y, bg_tinct);
		if (fg)
			_swix(Tinct_PlotAlpha, _INR(2,4) | _IN(7),
					fg, origin_x, origin_y, fg_tinct);
		more = wimp_get_rectangle(redraw);
	}
}

void ro_gui_options_image_read(wimp_w w, unsigned int *bg, unsigned int *fg)
{
	const char *text;
	int i;

	text = ro_gui_get_icon_string(w, IMAGE_FOREGROUND_FIELD);
	for (i = 0; i < 4; i++)
		if (!strcmp(text, image_quality_menu->entries[i].
				data.indirected_text.text))
			*fg = tinct_options[i];

	text = ro_gui_get_icon_string(w, IMAGE_BACKGROUND_FIELD);
	for (i = 0; i < 4; i++)
		if (!strcmp(text, image_quality_menu->entries[i].
				data.indirected_text.text))
			*bg = tinct_options[i];
}

bool ro_gui_options_image_click(wimp_pointer *pointer)
{
	unsigned int old_fg, old_bg, bg, fg;

	ro_gui_options_image_read(pointer->w, &old_bg, &old_fg);
	switch (pointer->i) {
		case IMAGE_DEFAULT_BUTTON:
			ro_gui_set_icon_string(pointer->w,
					IMAGE_FOREGROUND_FIELD,
					image_quality_menu->entries[3].
						data.indirected_text.text, true);
  			ro_gui_set_icon_string(pointer->w,
					IMAGE_BACKGROUND_FIELD,
					image_quality_menu->entries[2].
						data.indirected_text.text, true);
			ro_gui_set_icon_decimal(pointer->w, IMAGE_SPEED_FIELD,
					10, 2);
			ro_gui_set_icon_selected_state(pointer->w,
					IMAGE_DISABLE_ANIMATION, false);
		case IMAGE_DISABLE_ANIMATION:
			ro_gui_options_update_shading(pointer->w);
			break;
		case IMAGE_CANCEL_BUTTON:
			ro_gui_wimp_event_restore(pointer->w);
			break;
		default:
			return false;
	}

	ro_gui_options_image_read(pointer->w, &bg, &fg);
	if ((bg != old_bg) || (fg != old_fg))
		ro_gui_options_image_update(pointer->w, pointer->i,
				NULL, NULL, NO_ACTION);

	return false;
}

void ro_gui_options_update_shading(wimp_w w)
{
	bool shaded;

	shaded = ro_gui_get_icon_selected_state(w, IMAGE_DISABLE_ANIMATION);
	ro_gui_set_icon_shaded_state(w, IMAGE_SPEED_TEXT, shaded);
	ro_gui_set_icon_shaded_state(w, IMAGE_SPEED_FIELD, shaded);
	ro_gui_set_icon_shaded_state(w, IMAGE_SPEED_DEC, shaded);
	ro_gui_set_icon_shaded_state(w, IMAGE_SPEED_INC, shaded);
	ro_gui_set_icon_shaded_state(w, IMAGE_SPEED_CS, shaded);
}

bool ro_gui_options_image_ok(wimp_w w)
{
	ro_gui_options_image_read(w, 
				  (unsigned int *)&nsoption_int(plot_bg_quality),
				  (unsigned int *)&nsoption_int(plot_fg_quality));

	nsoption_set_int(minimum_gif_delay,
			 ro_gui_get_icon_decimal(w, IMAGE_SPEED_FIELD, 2));

	nsoption_set_bool(animate_images,
			  !ro_gui_get_icon_selected_state(w,
					IMAGE_DISABLE_ANIMATION));
	ro_gui_save_options();

	return true;
}

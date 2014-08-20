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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "utils/config.h"
#include "utils/nsoption.h"
#include "riscos/configure/configure.h"
#include "riscos/configure.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/theme.h"
#include "riscos/toolbar.h"
#include "riscos/url_complete.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define THEME_PANE_AREA 0
#define THEME_DEFAULT_BUTTON 2
#define THEME_CANCEL_BUTTON 3
#define THEME_OK_BUTTON 4

struct toolbar_display {
	struct toolbar *toolbar;
	struct theme_descriptor *descriptor;
	int icon_number;
	struct toolbar_display *next;
};

static wimp_window theme_pane_definition = {
	{0, 0, 16, 16},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_VSCROLL | wimp_WINDOW_AUTO_REDRAW,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	0,
	{0, -16384, 16384, 0},
	wimp_ICON_TEXT | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED,
	wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT,
	wimpspriteop_AREA,
	1,
	1,
	{""},
	0,
	{}
};


static wimp_w theme_pane;
static struct theme_descriptor *theme_list = NULL;
static struct toolbar_display *toolbars = NULL;
static char theme_radio_validation[] = "Sradiooff,radioon";
static char theme_null_validation[] = "";
static char theme_line_validation[] = "R2";

static bool ro_gui_options_theme_ok(wimp_w w);
static bool ro_gui_options_theme_click(wimp_pointer *pointer);
static void ro_gui_options_theme_load(void);
static void ro_gui_options_theme_free(void);

bool ro_gui_options_theme_initialise(wimp_w w)
{
	wimp_window_state state;
	wimp_icon_state icon_state;
	os_error *error;
	struct theme_descriptor *theme_choice;
	struct toolbar_display *toolbar;

	/* only allow one instance for now*/
	if (theme_pane)
		return false;
	error = xwimp_create_window(&theme_pane_definition, &theme_pane);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	icon_state.w = w;
	icon_state.i = THEME_PANE_AREA;
	error = xwimp_get_icon_state(&icon_state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	state.w = theme_pane;
	state.visible.x1 = state.visible.x0 + icon_state.icon.extent.x1 - 16 -
			ro_get_vscroll_width(theme_pane);
	state.visible.x0 += icon_state.icon.extent.x0 + 16;
	state.visible.y0 = state.visible.y1 + icon_state.icon.extent.y0 + 16;
	state.visible.y1 += icon_state.icon.extent.y1 - 28;
	LOG(("Y0 = %i, y1 = %i", icon_state.icon.extent.y0, icon_state.icon.extent.y1));
	error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state), w,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_BS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_RS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_TS_EDGE_SHIFT);
	if (error) {
		LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

  	/* load themes */
	ro_gui_options_theme_load();

	/* set the current selection */
	theme_choice = ro_gui_theme_find(nsoption_charp(theme));
	if (!theme_choice)
		theme_choice = ro_gui_theme_find("Aletheia");
	for (toolbar = toolbars; toolbar; toolbar = toolbar->next)
		ro_gui_set_icon_selected_state(theme_pane, toolbar->icon_number,
				(toolbar->descriptor == theme_choice));
	ro_gui_wimp_event_memorise(theme_pane);
	ro_gui_wimp_event_set_help_prefix(theme_pane, "HelpThemePConfig");

	ro_gui_wimp_event_register_mouse_click(w, ro_gui_options_theme_click);
	ro_gui_wimp_event_register_cancel(w, THEME_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, THEME_OK_BUTTON,
			ro_gui_options_theme_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpThemeConfig");
	ro_gui_wimp_event_memorise(w);

	return true;
}

void ro_gui_options_theme_finalise(wimp_w w)
{
	os_error *error;

	ro_gui_options_theme_free();
	if (theme_pane) {
		ro_gui_wimp_event_finalise(theme_pane);
		error = xwimp_delete_window(theme_pane);
		if (error) {
			LOG(("xwimp_delete_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		theme_pane = 0;
	}
	ro_gui_wimp_event_finalise(w);
}

bool ro_gui_options_theme_ok(wimp_w w)
{
	struct toolbar_display *toolbar;
	struct theme_descriptor *theme_new = NULL;

	/* find the current selection */
	for (toolbar = toolbars; toolbar; toolbar = toolbar->next) {
		if (ro_gui_get_icon_selected_state(theme_pane, toolbar->icon_number)) {
			theme_new = toolbar->descriptor;
		  	break;
		}
	}

	/* set the options */
	if (theme_new) {
		nsoption_set_charp(theme, strdup(theme_new->leafname));
		ro_gui_theme_apply(theme_new);
	} else {
		nsoption_set_charp(theme, NULL);
        }
	ro_gui_save_options();

	/* store the pane status */
	ro_gui_wimp_event_memorise(theme_pane);
	return true;
}

bool ro_gui_options_theme_click(wimp_pointer *pointer)
{
	struct theme_descriptor *theme_default;
	struct toolbar_display *toolbar;

	switch (pointer->i) {
		case THEME_DEFAULT_BUTTON:
			theme_default = ro_gui_theme_find("Aletheia");
			for (toolbar = toolbars; toolbar; toolbar = toolbar->next)
				ro_gui_set_icon_selected_state(theme_pane,
						toolbar->icon_number,
						(toolbar->descriptor == theme_default));
			break;
		case THEME_CANCEL_BUTTON:
			ro_gui_wimp_event_restore(theme_pane);
			break;
		case THEME_OK_BUTTON:
			ro_gui_wimp_event_memorise(theme_pane);
			break;
	}
	return false;
}

void ro_gui_options_theme_load(void)
{
	os_error *error;
	os_box extent = { 0, 0, 0, 0 };
	struct theme_descriptor *descriptor;
	struct toolbar_display *link;
	struct toolbar_display *toolbar_display;
	struct toolbar *toolbar;
	wimp_icon_create new_icon;
	wimp_window_state state;
	int parent_width, nested_y, min_extent, base_extent;
	int item_height;
	int *radio_icons, *radio_set;
	int theme_count;

	/* delete our old list and get/open a new one */
	ro_gui_options_theme_free();
	theme_list = ro_gui_theme_get_available();
	ro_gui_theme_open(theme_list, true);

	/* create toolbars for each theme */
	theme_count = 0;
	descriptor = theme_list;
	while (descriptor != NULL) {
		/* try to create a toolbar */
		toolbar = ro_toolbar_create(descriptor, NULL,
				THEME_STYLE_BROWSER_TOOLBAR,
				TOOLBAR_FLAGS_DISPLAY, NULL, NULL, NULL);
		if (toolbar != NULL) {
			ro_toolbar_add_buttons(toolbar, brower_toolbar_buttons,
					       nsoption_charp(toolbar_browser));
			ro_toolbar_add_url(toolbar);
			ro_toolbar_add_throbber(toolbar);
			ro_toolbar_rebuild(toolbar);
			toolbar_display = calloc(sizeof(struct toolbar_display), 1);
			if (!toolbar_display) {
				LOG(("No memory for calloc()"));
				warn_user("NoMemory", 0);
				return;
			}
			toolbar_display->toolbar = toolbar;
			toolbar_display->descriptor = descriptor;
			if (!toolbars) {
				toolbars = toolbar_display;
			} else {
				link = toolbars;
				while (link->next) link = link->next;
				link->next = toolbar_display;
			}
			theme_count++;
		}
		descriptor = descriptor->next;
	}

	/* nest the toolbars */
	state.w = theme_pane;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	parent_width = state.visible.x1 - state.visible.x0;
	min_extent = state.visible.y0 - state.visible.y1;
	nested_y = 0;
	base_extent = state.visible.y1 - state.yscroll;
	extent.x1 = parent_width;
	link = toolbars;
	new_icon.w = theme_pane;
	new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
			wimp_ICON_VCENTRED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
	while (link) {
		/* update the toolbar */
		item_height = 44 + 44 + 16;
		if (link->next) item_height += 16;
		ro_toolbar_process(link->toolbar, parent_width, false);
		extent.y0 = nested_y -
				ro_toolbar_height(link->toolbar) -
				item_height;
		if (link->next) extent.y0 -= 16;
		if (extent.y0 > min_extent) extent.y0 = min_extent;
		xwimp_set_extent(theme_pane, &extent);

		/* create the descriptor icons and separator line */
		new_icon.icon.extent.x0 = 8;
		new_icon.icon.extent.x1 = parent_width - 8;
		new_icon.icon.flags &= ~wimp_ICON_BORDER;
		new_icon.icon.flags |= wimp_ICON_SPRITE;
		new_icon.icon.extent.y1 = nested_y -
				ro_toolbar_height(link->toolbar) - 8;
		new_icon.icon.extent.y0 = nested_y -
				ro_toolbar_height(link->toolbar) - 52;
		new_icon.icon.data.indirected_text_and_sprite.text =
			(char *)&link->descriptor->name;
		new_icon.icon.data.indirected_text_and_sprite.size =
			strlen(link->descriptor->name) + 1;
		new_icon.icon.data.indirected_text_and_sprite.validation =
				theme_radio_validation;
		new_icon.icon.flags |= (wimp_BUTTON_RADIO <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
		xwimp_create_icon(&new_icon, &link->icon_number);
		new_icon.icon.flags &= ~wimp_ICON_SPRITE;
		new_icon.icon.extent.x0 = 52;
		new_icon.icon.extent.y1 -= 44;
		new_icon.icon.extent.y0 -= 44;
		new_icon.icon.data.indirected_text.text =
			(char *)&link->descriptor->author;
		new_icon.icon.data.indirected_text.size =
			strlen(link->descriptor->author) + 1;
		new_icon.icon.data.indirected_text.validation =
				theme_null_validation;
		new_icon.icon.flags &= ~(wimp_BUTTON_RADIO <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
		xwimp_create_icon(&new_icon, 0);
		if (link->next) {
			new_icon.icon.flags |= wimp_ICON_BORDER;
			new_icon.icon.extent.x0 = -8;
			new_icon.icon.extent.x1 = parent_width + 8;
			new_icon.icon.extent.y1 -= 52;
			new_icon.icon.extent.y0 = new_icon.icon.extent.y1 - 8;
			new_icon.icon.data.indirected_text.text =
					theme_null_validation;
			new_icon.icon.data.indirected_text.validation =
					theme_line_validation;
			new_icon.icon.data.indirected_text.size = 1;
			xwimp_create_icon(&new_icon, 0);
		}

		/* nest the toolbar window */
		state.w = ro_toolbar_get_window(link->toolbar);
		state.yscroll = 0;
		state.visible.y1 = nested_y + base_extent;
		state.visible.y0 = state.visible.y1 -
				ro_toolbar_height(link->toolbar) + 2;
		xwimp_open_window_nested(PTR_WIMP_OPEN(&state), theme_pane,
				wimp_CHILD_LINKS_PARENT_WORK_AREA
						<< wimp_CHILD_BS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_WORK_AREA
						<< wimp_CHILD_TS_EDGE_SHIFT);

		/* continue processing */
		nested_y -= ro_toolbar_height(link->toolbar) +
				item_height;
		link = link->next;
	}

	/* set the icons as radios */
	radio_icons = (int *)calloc(theme_count + 1, sizeof(int));
	radio_set = radio_icons;
	for (link = toolbars; link; link = link->next)
		*radio_set++ = link->icon_number;
	*radio_set = -1;
	ro_gui_wimp_event_register_radio(theme_pane, radio_icons);

	/* update our display */
	xwimp_force_redraw(theme_pane, 0, -16384, 16384, 16384);
}

void ro_gui_options_theme_free(void)
{
	struct toolbar_display *toolbar;
	struct toolbar_display *next_toolbar;

	/* free all our toolbars */
	next_toolbar = toolbars;
	while ((toolbar = next_toolbar) != NULL) {
		next_toolbar = toolbar->next;
		xwimp_delete_icon(theme_pane, toolbar->icon_number);
		xwimp_delete_icon(theme_pane, toolbar->icon_number + 1);
		if (next_toolbar)
			xwimp_delete_icon(theme_pane,
					toolbar->icon_number + 2);
		ro_toolbar_destroy(toolbar->toolbar);
		free(toolbar);
	}
	toolbars = NULL;

	/* close all our themes */
	if (theme_list)
		ro_gui_theme_close(theme_list, true);
	theme_list = NULL;
}

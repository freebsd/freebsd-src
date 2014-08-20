/*
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
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
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "utils/nsoption.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"


#define INTERFACE_STRIP_EXTNS_OPTION 2
#define INTERFACE_CONFIRM_OVWR_OPTION 3
#define INTERFACE_URL_COMPLETE_OPTION 6
#define INTERFACE_HISTORY_TOOLTIP_OPTION 7
#define INTERFACE_THUMBNAIL_ICONISE_OPTION 10
#define INTERFACE_DEFAULT_BUTTON 11
#define INTERFACE_CANCEL_BUTTON 12
#define INTERFACE_OK_BUTTON 13
#define INTERFACE_USE_EXTERNAL_HOTLIST 16
#define INTERFACE_EXTERNAL_HOTLIST_APP 18


static bool ro_gui_options_interface_click(wimp_pointer *pointer);
static void ro_gui_options_interface_default(wimp_pointer *pointer);
static bool ro_gui_options_interface_ok(wimp_w w);

bool ro_gui_options_interface_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_selected_state(w, INTERFACE_STRIP_EXTNS_OPTION,
                                       nsoption_bool(strip_extensions));
	ro_gui_set_icon_selected_state(w, INTERFACE_CONFIRM_OVWR_OPTION,
                                       nsoption_bool(confirm_overwrite));
	ro_gui_set_icon_selected_state(w, INTERFACE_URL_COMPLETE_OPTION,
                                       nsoption_bool(url_suggestion));
	ro_gui_set_icon_selected_state(w, INTERFACE_HISTORY_TOOLTIP_OPTION,
                                       nsoption_bool(history_tooltip));
	ro_gui_set_icon_selected_state(w, INTERFACE_THUMBNAIL_ICONISE_OPTION,
                                       nsoption_bool(thumbnail_iconise));
	ro_gui_set_icon_selected_state(w, INTERFACE_USE_EXTERNAL_HOTLIST,
                                       nsoption_bool(external_hotlists));
	ro_gui_set_icon_string(w, INTERFACE_EXTERNAL_HOTLIST_APP,
                               (nsoption_charp(external_hotlist_app)) ?
                               nsoption_charp(external_hotlist_app) : "", false);

	ro_gui_set_icon_shaded_state(w, INTERFACE_EXTERNAL_HOTLIST_APP,
                                     !nsoption_bool(external_hotlists));

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_mouse_click(w,
			ro_gui_options_interface_click);
	ro_gui_wimp_event_register_button(w, INTERFACE_DEFAULT_BUTTON,
			ro_gui_options_interface_default);
	ro_gui_wimp_event_register_cancel(w, INTERFACE_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, INTERFACE_OK_BUTTON,
			ro_gui_options_interface_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpInterfaceConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}


bool ro_gui_options_interface_click(wimp_pointer *pointer)
{
	bool	shaded;

	switch (pointer->i) {
		case INTERFACE_USE_EXTERNAL_HOTLIST:
			shaded = !ro_gui_get_icon_selected_state(pointer->w,
					INTERFACE_USE_EXTERNAL_HOTLIST);
			ro_gui_set_icon_shaded_state(pointer->w,
					INTERFACE_EXTERNAL_HOTLIST_APP, shaded);
			return false;
			break;
	}
	return false;
}



void ro_gui_options_interface_default(wimp_pointer *pointer)
{
	ro_gui_set_icon_selected_state(pointer->w,
			INTERFACE_STRIP_EXTNS_OPTION, true);
	ro_gui_set_icon_selected_state(pointer->w,
			INTERFACE_CONFIRM_OVWR_OPTION, true);
	ro_gui_set_icon_selected_state(pointer->w,
			INTERFACE_URL_COMPLETE_OPTION, true);
	ro_gui_set_icon_selected_state(pointer->w,
			INTERFACE_HISTORY_TOOLTIP_OPTION, true);
	ro_gui_set_icon_selected_state(pointer->w,
			INTERFACE_THUMBNAIL_ICONISE_OPTION, true);
	ro_gui_set_icon_selected_state(pointer->w,
			INTERFACE_USE_EXTERNAL_HOTLIST, false);
	ro_gui_set_icon_string(pointer->w, INTERFACE_EXTERNAL_HOTLIST_APP,
			"", false);
}

bool ro_gui_options_interface_ok(wimp_w w)
{
	nsoption_set_bool(strip_extensions, 
			ro_gui_get_icon_selected_state(w, 
					INTERFACE_STRIP_EXTNS_OPTION));
	nsoption_set_bool(confirm_overwrite, 
                          ro_gui_get_icon_selected_state(w,
                                        INTERFACE_CONFIRM_OVWR_OPTION));
	nsoption_set_bool(url_suggestion, 
                          ro_gui_get_icon_selected_state(w,
                                        INTERFACE_URL_COMPLETE_OPTION));
	nsoption_set_bool(history_tooltip, 
                          ro_gui_get_icon_selected_state(w,
                                        INTERFACE_HISTORY_TOOLTIP_OPTION));
	nsoption_set_bool(thumbnail_iconise, 
                          ro_gui_get_icon_selected_state(w,
                                        INTERFACE_THUMBNAIL_ICONISE_OPTION));
	nsoption_set_bool(external_hotlists, 
                          ro_gui_get_icon_selected_state(w,
                                        INTERFACE_USE_EXTERNAL_HOTLIST));
	nsoption_set_charp(external_hotlist_app,
                           strdup(ro_gui_get_icon_string(w,
                                        INTERFACE_EXTERNAL_HOTLIST_APP)));

	ro_gui_save_options();
	return true;
}

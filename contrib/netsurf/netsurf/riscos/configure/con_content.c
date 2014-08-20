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
#include "utils/nsoption.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define CONTENT_BLOCK_ADVERTISEMENTS 2
#define CONTENT_BLOCK_POPUPS 3
#define CONTENT_NO_PLUGINS 4
#define CONTENT_TARGET_BLANK 7
#define CONTENT_DEFAULT_BUTTON 8
#define CONTENT_CANCEL_BUTTON 9
#define CONTENT_OK_BUTTON 10
#define CONTENT_NO_JAVASCRIPT 11

static void ro_gui_options_content_default(wimp_pointer *pointer);
static bool ro_gui_options_content_ok(wimp_w w);

bool ro_gui_options_content_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_selected_state(w, CONTENT_BLOCK_ADVERTISEMENTS,
                                       nsoption_bool(block_advertisements));
	ro_gui_set_icon_selected_state(w, CONTENT_BLOCK_POPUPS,
                                       nsoption_bool(block_popups));
	ro_gui_set_icon_selected_state(w, CONTENT_NO_PLUGINS,
                                       nsoption_bool(no_plugins));
	ro_gui_set_icon_selected_state(w, CONTENT_TARGET_BLANK,
                                       nsoption_bool(target_blank));
	ro_gui_set_icon_selected_state(w, CONTENT_NO_JAVASCRIPT,
                                       !nsoption_bool(enable_javascript));

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_checkbox(w, CONTENT_BLOCK_ADVERTISEMENTS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_BLOCK_POPUPS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_NO_PLUGINS);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_TARGET_BLANK);
	ro_gui_wimp_event_register_checkbox(w, CONTENT_NO_JAVASCRIPT);
	ro_gui_wimp_event_register_button(w, CONTENT_DEFAULT_BUTTON,
			ro_gui_options_content_default);
	ro_gui_wimp_event_register_cancel(w, CONTENT_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, CONTENT_OK_BUTTON,
			ro_gui_options_content_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpContentConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_content_default(wimp_pointer *pointer)
{
	/* set the default values */
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_BLOCK_ADVERTISEMENTS,
			false);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_BLOCK_POPUPS,
			false);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_NO_PLUGINS,
			false);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_TARGET_BLANK,
			true);
	ro_gui_set_icon_selected_state(pointer->w, CONTENT_NO_JAVASCRIPT,
			false);
}

bool ro_gui_options_content_ok(wimp_w w)
{
	nsoption_set_bool(block_advertisements,
			  ro_gui_get_icon_selected_state(w, CONTENT_BLOCK_ADVERTISEMENTS));

	nsoption_set_bool(block_popups,
			  ro_gui_get_icon_selected_state(w, CONTENT_BLOCK_POPUPS));
	nsoption_set_bool(no_plugins,
			  ro_gui_get_icon_selected_state(w, CONTENT_NO_PLUGINS));

	nsoption_set_bool(target_blank,
			  ro_gui_get_icon_selected_state(w, CONTENT_TARGET_BLANK));

	nsoption_set_bool(enable_javascript,
			!ro_gui_get_icon_selected_state(w, CONTENT_NO_JAVASCRIPT));

	ro_gui_save_options();
  	return true;
}

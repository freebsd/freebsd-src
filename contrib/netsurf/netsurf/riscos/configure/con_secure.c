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


#define SECURITY_REFERRER 2
#define SECURITY_DURATION_FIELD 6
#define SECURITY_DURATION_INC 7
#define SECURITY_DURATION_DEC 8
#define SECURITY_DEFAULT_BUTTON 10
#define SECURITY_CANCEL_BUTTON 11
#define SECURITY_OK_BUTTON 12

static void ro_gui_options_security_default(wimp_pointer *pointer);
static bool ro_gui_options_security_ok(wimp_w w);

bool ro_gui_options_security_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_selected_state(w, SECURITY_REFERRER,
                                       nsoption_bool(send_referer));
	ro_gui_set_icon_integer(w, SECURITY_DURATION_FIELD,
                                nsoption_int(expire_url));

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_checkbox(w, SECURITY_REFERRER);
	ro_gui_wimp_event_register_numeric_field(w, SECURITY_DURATION_FIELD,
			SECURITY_DURATION_DEC, SECURITY_DURATION_INC,
			0, 365, 1, 0);
	ro_gui_wimp_event_register_button(w, SECURITY_DEFAULT_BUTTON,
			ro_gui_options_security_default);
	ro_gui_wimp_event_register_cancel(w, SECURITY_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, SECURITY_OK_BUTTON,
			ro_gui_options_security_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpSecurityConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_security_default(wimp_pointer *pointer)
{
	/* set the default values */
	ro_gui_set_icon_integer(pointer->w, SECURITY_DURATION_FIELD, 28);
	ro_gui_set_icon_selected_state(pointer->w, SECURITY_REFERRER, true);
}

bool ro_gui_options_security_ok(wimp_w w)
{
	nsoption_set_bool(send_referer,
			  ro_gui_get_icon_selected_state(w, SECURITY_REFERRER));

	nsoption_set_int(expire_url,
			 ro_gui_get_icon_decimal(w,SECURITY_DURATION_FIELD, 0));

	ro_gui_save_options();
  	return true;
}

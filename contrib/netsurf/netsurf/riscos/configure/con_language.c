/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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
#include "riscos/menus.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define LANGUAGE_INTERFACE_FIELD 3
#define LANGUAGE_INTERFACE_GRIGHT 4
#define LANGUAGE_WEB_PAGES_FIELD 6
#define LANGUAGE_WEB_PAGES_GRIGHT 7
#define LANGUAGE_DEFAULT_BUTTON 8
#define LANGUAGE_CANCEL_BUTTON 9
#define LANGUAGE_OK_BUTTON 10

static void ro_gui_options_language_default(wimp_pointer *pointer);
static bool ro_gui_options_language_ok(wimp_w w);
static const char *ro_gui_options_language_name(const char *code);

bool ro_gui_options_language_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_string(w, LANGUAGE_INTERFACE_FIELD,
		ro_gui_options_language_name(nsoption_charp(language) ?
			nsoption_charp(language) : "en"), true);
	ro_gui_set_icon_string(w, LANGUAGE_WEB_PAGES_FIELD,
		ro_gui_options_language_name(nsoption_charp(accept_language) ?
			nsoption_charp(accept_language) : "en"), true);

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_menu_gright(w, LANGUAGE_INTERFACE_FIELD,
			LANGUAGE_INTERFACE_GRIGHT, languages_menu);
	ro_gui_wimp_event_register_menu_gright(w, LANGUAGE_WEB_PAGES_FIELD,
			LANGUAGE_WEB_PAGES_GRIGHT, languages_menu);
	ro_gui_wimp_event_register_button(w, LANGUAGE_DEFAULT_BUTTON,
			ro_gui_options_language_default);
	ro_gui_wimp_event_register_cancel(w, LANGUAGE_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, LANGUAGE_OK_BUTTON,
			ro_gui_options_language_ok);
	ro_gui_wimp_event_set_help_prefix(w, "HelpLanguageConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_language_default(wimp_pointer *pointer)
{
	const char *code;

	code = ro_gui_default_language();
	ro_gui_set_icon_string(pointer->w, LANGUAGE_INTERFACE_FIELD,
			ro_gui_options_language_name(code ?
					code : "en"), true);
	ro_gui_set_icon_string(pointer->w, LANGUAGE_WEB_PAGES_FIELD,
			ro_gui_options_language_name(code ?
					code : "en"), true);
}

bool ro_gui_options_language_ok(wimp_w w)
{
	const char *code;
	char *temp;

	code = ro_gui_menu_find_menu_entry_key(languages_menu,
			ro_gui_get_icon_string(w, LANGUAGE_INTERFACE_FIELD));
	if (code) {
		code += 5;	/* skip 'lang_' */
		if ((!nsoption_charp(language)) || 
                    (strcmp(nsoption_charp(language), code))) {
			temp = strdup(code);
			if (temp) {
				nsoption_set_charp(language, temp);
			} else {
				LOG(("No memory to duplicate language code"));
				warn_user("NoMemory", 0);
			}
		}
	}
	code = ro_gui_menu_find_menu_entry_key(languages_menu,
			ro_gui_get_icon_string(w, LANGUAGE_WEB_PAGES_FIELD));
	if (code) {
		code += 5;	/* skip 'lang_' */
		if ((!nsoption_charp(accept_language)) ||
                    (strcmp(nsoption_charp(accept_language), code))) {
			temp = strdup(code);
			if (temp) {
				nsoption_set_charp(accept_language,temp);
			} else {
				LOG(("No memory to duplicate language code"));
				warn_user("NoMemory", 0);
			}
		}
	}
	ro_gui_save_options();
	return true;
}


/**
 * Convert a 2-letter ISO language code to the language name.
 *
 * \param  code  2-letter ISO language code
 * \return  language name, or code if unknown
 */
const char *ro_gui_options_language_name(const char *code)
{
	char key[] = "lang_xx";
	key[5] = code[0];
	key[6] = code[1];

	return messages_get(key);
}

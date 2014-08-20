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


#define CONNECTION_PROXY_FIELD 3
#define CONNECTION_PROXY_MENU 4
#define CONNECTION_PROXY_HOST_LABEL 5
#define CONNECTION_PROXY_HOST 6
#define CONNECTION_PROXY_PORT_LABEL 7
#define CONNECTION_PROXY_PORT 8
#define CONNECTION_PROXY_USERNAME_LABEL 9
#define CONNECTION_PROXY_USERNAME 10
#define CONNECTION_PROXY_PASSWORD_LABEL 11
#define CONNECTION_PROXY_PASSWORD 12
#define CONNECTION_MAX_FETCH_FIELD 16
#define CONNECTION_MAX_FETCH_DEC 17
#define CONNECTION_MAX_FETCH_INC 18
#define CONNECTION_HOST_FETCH_FIELD 20
#define CONNECTION_HOST_FETCH_DEC 21
#define CONNECTION_HOST_FETCH_INC 22
#define CONNECTION_CACHE_FETCH_FIELD 24
#define CONNECTION_CACHE_FETCH_DEC 25
#define CONNECTION_CACHE_FETCH_INC 26
#define CONNECTION_DEFAULT_BUTTON 27
#define CONNECTION_CANCEL_BUTTON 28
#define CONNECTION_OK_BUTTON 29

#define http_proxy_type (nsoption_bool(http_proxy) ? (nsoption_int(http_proxy_auth) + 1) : 0)

static int ro_gui_options_connection_proxy_type(wimp_w w);
static void ro_gui_options_connection_default(wimp_pointer *pointer);
static bool ro_gui_options_connection_ok(wimp_w w);
static bool ro_gui_options_connection_update(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_selection *s, menu_action a);

bool ro_gui_options_connection_initialise(wimp_w w)
{
	int proxy_type;

	/* set the current values */
	proxy_type = (nsoption_bool(http_proxy) ? (nsoption_int(http_proxy_auth) + 1) : 0);
	ro_gui_set_icon_string(w, CONNECTION_PROXY_FIELD,
			proxy_type_menu->entries[proxy_type].
				data.indirected_text.text, true);
	ro_gui_set_icon_string(w, CONNECTION_PROXY_HOST,
                               nsoption_charp(http_proxy_host) ?
                               nsoption_charp(http_proxy_host) : "", true);
	ro_gui_set_icon_integer(w, CONNECTION_PROXY_PORT,
                                nsoption_int(http_proxy_port));
	ro_gui_set_icon_string(w, CONNECTION_PROXY_USERNAME,
                               nsoption_charp(http_proxy_auth_user) ?
                               nsoption_charp(http_proxy_auth_user) : "", true);
	ro_gui_set_icon_string(w, CONNECTION_PROXY_PASSWORD,
                               nsoption_charp(http_proxy_auth_pass) ?
                               nsoption_charp(http_proxy_auth_pass) : "", true);
	ro_gui_set_icon_integer(w, CONNECTION_MAX_FETCH_FIELD,
                                nsoption_int(max_fetchers));
	ro_gui_set_icon_integer(w, CONNECTION_HOST_FETCH_FIELD,
                                nsoption_int(max_fetchers_per_host));
	ro_gui_set_icon_integer(w, CONNECTION_CACHE_FETCH_FIELD,
                                nsoption_int(max_cached_fetch_handles));
	ro_gui_options_connection_update(w, -1, NULL, NULL, NO_ACTION);

	/* register icons */
	ro_gui_wimp_event_register_menu_gright(w, CONNECTION_PROXY_FIELD,
			CONNECTION_PROXY_MENU, proxy_type_menu);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_HOST_LABEL);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_HOST);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_PORT_LABEL);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_PORT);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_USERNAME_LABEL);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_USERNAME);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_PASSWORD_LABEL);
	ro_gui_wimp_event_register_text_field(w, CONNECTION_PROXY_PASSWORD);
	ro_gui_wimp_event_register_numeric_field(w, CONNECTION_MAX_FETCH_FIELD,
			CONNECTION_MAX_FETCH_INC, CONNECTION_MAX_FETCH_DEC,
			1, 99, 1, 0);
	ro_gui_wimp_event_register_numeric_field(w, CONNECTION_HOST_FETCH_FIELD,
			CONNECTION_HOST_FETCH_INC, CONNECTION_HOST_FETCH_DEC,
			1, 99, 1, 0);
	ro_gui_wimp_event_register_numeric_field(w, CONNECTION_CACHE_FETCH_FIELD,
			CONNECTION_CACHE_FETCH_INC, CONNECTION_CACHE_FETCH_DEC,
			1, 99, 1, 0);
	ro_gui_wimp_event_register_menu_selection(w,
			ro_gui_options_connection_update);
	ro_gui_wimp_event_register_button(w, CONNECTION_DEFAULT_BUTTON,
			ro_gui_options_connection_default);
	ro_gui_wimp_event_register_cancel(w, CONNECTION_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, CONNECTION_OK_BUTTON,
			ro_gui_options_connection_ok);

	ro_gui_wimp_event_set_help_prefix(w, "HelpConnectConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

bool ro_gui_options_connection_update(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_selection *s, menu_action a)
{
	int proxy_type;
	bool host, user;

	/* update the shaded state */
	proxy_type = ro_gui_options_connection_proxy_type(w);
	host = (proxy_type > 0);
	user = (proxy_type > 1);

	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_HOST_LABEL, !host);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_HOST, !host);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_PORT_LABEL, !host);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_PORT, !host);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_USERNAME_LABEL, !user);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_USERNAME, !user);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_PASSWORD_LABEL, !user);
	ro_gui_set_icon_shaded_state(w, CONNECTION_PROXY_PASSWORD, !user);

	return true;
}

int ro_gui_options_connection_proxy_type(wimp_w w)
{
	const char *text;
	int i;

	text = ro_gui_get_icon_string(w, CONNECTION_PROXY_FIELD);
	for (i = 0; (i < 4); i++)
		if (!strcmp(text, proxy_type_menu->entries[i].
				data.indirected_text.text))
			return i;
	assert(false);
}

void ro_gui_options_connection_default(wimp_pointer *pointer)
{
	ro_gui_set_icon_string(pointer->w, CONNECTION_PROXY_FIELD,
			proxy_type_menu->entries[0].
				data.indirected_text.text, true);
	ro_gui_set_icon_string(pointer->w, CONNECTION_PROXY_HOST, "", true);
	ro_gui_set_icon_integer(pointer->w, CONNECTION_PROXY_PORT, 8080);
	ro_gui_set_icon_string(pointer->w, CONNECTION_PROXY_USERNAME, "", true);
	ro_gui_set_icon_string(pointer->w, CONNECTION_PROXY_PASSWORD, "", true);
	ro_gui_set_icon_integer(pointer->w, CONNECTION_MAX_FETCH_FIELD, 24);
	ro_gui_set_icon_integer(pointer->w, CONNECTION_HOST_FETCH_FIELD, 5);
	ro_gui_set_icon_integer(pointer->w, CONNECTION_CACHE_FETCH_FIELD, 6);
	ro_gui_options_connection_update(pointer->w, -1, NULL, NULL, NO_ACTION);
}

bool ro_gui_options_connection_ok(wimp_w w)
{
	int proxy_type;

	proxy_type = ro_gui_options_connection_proxy_type(w);
	if (proxy_type == 0) {
		nsoption_set_bool(http_proxy, false);
	} else {
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, proxy_type - 1);
	}

	nsoption_set_charp(http_proxy_host,
			   strdup(ro_gui_get_icon_string(w,
					CONNECTION_PROXY_HOST)));

	nsoption_set_int(http_proxy_port,
			 ro_gui_get_icon_decimal(w, CONNECTION_PROXY_PORT, 0));

	nsoption_set_charp(http_proxy_auth_user,
			   strdup(ro_gui_get_icon_string(w,
					CONNECTION_PROXY_USERNAME)));

	nsoption_set_charp(http_proxy_auth_pass,
			   strdup(ro_gui_get_icon_string(w,
					CONNECTION_PROXY_PASSWORD)));

	nsoption_set_int(max_fetchers,
			 ro_gui_get_icon_decimal(w,
					CONNECTION_MAX_FETCH_FIELD, 0));

	nsoption_set_int(max_fetchers_per_host,
			 ro_gui_get_icon_decimal(w,
					CONNECTION_HOST_FETCH_FIELD, 0));

	nsoption_set_int(max_cached_fetch_handles,
			 ro_gui_get_icon_decimal(w,
					CONNECTION_CACHE_FETCH_FIELD, 0));

	ro_gui_save_options();
	return true;
}

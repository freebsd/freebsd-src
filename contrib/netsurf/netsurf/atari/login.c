/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <cflib.h>

#include "utils/config.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "utils/errors.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/url.h"
#include "content/urldb.h"
#include "content/fetch.h"
#include "atari/misc.h"
#include "atari/login.h"
#include "atari/res/netsurf.rsh"


bool login_form_do(nsurl * url, char * realm, char ** out)
{
	char user[255];
	char pass[255];
	//const char * auth;
	short exit_obj = 0;
	OBJECT * tree;

	user[0] = 0;
	pass[0] = 0;

	// TODO: use auth details for predefined login data
	// auth = urldb_get_auth_details(url, realm);
	tree = gemtk_obj_get_tree(LOGIN);

	assert(tree != NULL);

	exit_obj = simple_mdial(tree, 0);

	if(exit_obj == LOGIN_BT_LOGIN) {
		get_string(tree, LOGIN_TB_USER, user);
		get_string(tree, LOGIN_TB_PASSWORD, pass);
		int size = strlen((char*)&user) + strlen((char*)&pass) + 2 ;
		*out = malloc(size);
		snprintf(*out, size, "%s:%s", user, pass);
	} else {
		*out = NULL;
	}
	return((exit_obj == LOGIN_BT_LOGIN));
}


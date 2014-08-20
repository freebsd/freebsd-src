/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include "amiga/cookies.h"
#include "desktop/cookie_manager.h"
#include "amiga/tree.h"

void ami_cookies_initialise(void)
{
	cookies_window = ami_tree_create(TREE_COOKIES, NULL);

	if(!cookies_window) return;
}

void ami_cookies_free()
{
	ami_tree_destroy(cookies_window);
	cookies_window = NULL;
}

/*
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#include <stdio.h>

#include "utils/filepath.h"

#include "framebuffer/findfile.h"

char **respaths; /** resource search path vector */

/** Create an array of valid paths to search for resources.
 *
 * The idea is that all the complex path computation to find resources
 * is performed here, once, rather than every time a resource is
 * searched for.
 */
char **
fb_init_resource(const char *resource_path)
{
	char **pathv; /* resource path string vector */
	char **respath; /* resource paths vector */
	const char *lang = NULL;

	pathv = filepath_path_to_strvec(resource_path);

	respath = filepath_generate(pathv, &lang);

	filepath_free_strvec(pathv);

	return respath;
}



/*
 * Local Variables:
 * c-basic-offset: 8
 * End:
 */


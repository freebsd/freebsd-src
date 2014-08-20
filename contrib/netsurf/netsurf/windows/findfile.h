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

#ifndef _NETSURF_WINDOWS_FINDFILE_H_
#define _NETSURF_WINDOWS_FINDFILE_H_

extern char *nsws_find_resource(char *buf, const char *filename, const char *def);

char **nsws_init_resource(const char *resource_path);

char *path_to_url(const char *path);
char *url_to_path(const char *url);


#endif /* _NETSURF_WINDOWS_FINDFILE_H_ */

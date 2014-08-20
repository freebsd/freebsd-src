/*
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_FINDFILE_H
#define NS_ATARI_FINDFILE_H

extern char *atari_find_resource(char *buf, const char *filename, const char *def);
char *local_file_to_url(const char *filename);

char *path_to_url(const char *path_in);
char *url_to_path(const char *url);

#endif /* NETSURF_ATARI_FINDFILE_H */

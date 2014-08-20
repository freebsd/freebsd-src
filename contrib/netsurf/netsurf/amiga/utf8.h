/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_UTF8_H
#define AMIGA_UTF8_H

extern struct gui_utf8_table *ami_utf8_table;

char *ami_utf8_easy(const char *string);
void ami_utf8_free(char *ptr);
char *ami_to_utf8_easy(const char *string);

nserror utf8_from_local_encoding(const char *string, size_t len, char **result);
nserror utf8_to_local_encoding(const char *string, size_t len, char **result);

#endif

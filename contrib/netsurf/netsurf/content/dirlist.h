/*
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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

/** \file
 * Generate HTML content for displaying directory listings (interface).
 *
 * These functions should in general be called via the content interface.
 */

#ifndef _NETSURF_CONTENT_DIRLIST_H_
#define _NETSURF_CONTENT_DIRLIST_H_

#include <stdbool.h>

#define DIRLIST_NO_NAME_COLUMN 1
#define DIRLIST_NO_TYPE_COLUMN 1 << 1
#define DIRLIST_NO_SIZE_COLUMN 1 << 2
#define DIRLIST_NO_DATE_COLUMN 1 << 3
#define DIRLIST_NO_TIME_COLUMN 1 << 4

bool dirlist_generate_top(char *buffer, int buffer_length);
bool dirlist_generate_hide_columns(int flags, char *buffer, int buffer_length);
bool dirlist_generate_title(const char *title, char *buffer, int buffer_length);
bool dirlist_generate_parent_link(const char *parent, char *buffer,
		int buffer_length);
bool dirlist_generate_headings(char *buffer, int buffer_length);
bool dirlist_generate_row(bool even, bool directory, char *url, char *name,
		const char *mimetype, long long size, char *date, char *time,
		char *buffer, int buffer_length);
bool dirlist_generate_bottom(char *buffer, int buffer_length);

#endif

/*
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
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
 * Locale-specific variants of various routines (interface)
 */

#ifndef _NETSURF_UTILS_LOCALE_H_
#define _NETSURF_UTILS_LOCALE_H_

/* <ctype.h> functions */
int ls_isalpha(int c);
int ls_isalnum(int c);
int ls_iscntrl(int c);
int ls_isdigit(int c);
int ls_isgraph(int c);
int ls_islower(int c);
int ls_isprint(int c);
int ls_ispunct(int c);
int ls_isspace(int c);
int ls_isupper(int c);
int ls_isxdigit(int c);
int ls_tolower(int c);
int ls_toupper(int c);

#endif


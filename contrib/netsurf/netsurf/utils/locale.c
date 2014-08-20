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
 * Locale-specific variants of various routines (implementation)
 */

#include <ctype.h>
#include <locale.h>

#include "utils/locale.h"

/* <ctype.h> functions */
#define MAKELSCTYPE(x) int ls_##x(int c)				\
{									\
	int ret;							\
	setlocale(LC_ALL, "");						\
	ret = x(c);							\
	setlocale(LC_ALL, "C");						\
	return ret;							\
}

MAKELSCTYPE(isalpha)
MAKELSCTYPE(isalnum)
MAKELSCTYPE(iscntrl)
MAKELSCTYPE(isdigit)
MAKELSCTYPE(isgraph)
MAKELSCTYPE(islower)
MAKELSCTYPE(isprint)
MAKELSCTYPE(ispunct)
MAKELSCTYPE(isspace)
MAKELSCTYPE(isupper)
MAKELSCTYPE(isxdigit)
MAKELSCTYPE(tolower)
MAKELSCTYPE(toupper)

#undef MAKELSCTYPE


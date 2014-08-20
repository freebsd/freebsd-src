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

#ifndef AMIGA_IFF_CSET_H
#define AMIGA_IFF_CSET_H
#include <exec/types.h>

/* This structure is for the IFF CSET chunk, registered by Martin Taillefer */

struct CSet {
	LONG	CodeSet;	/* 0=ECMA Latin 1 (std Amiga charset) */
	/* CBM will define additional values  */
	LONG	Reserved[7];
	};

#define ID_CSET  MAKE_ID('C','S','E','T')

#endif

/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "testament.h"

/* NB: AmigaOS revision numbers start at 1 (not 0) and are monotonically
 * incremental (v1.20 is higher than v1.3 and not the same as v1.2).
 * Consequently, this version pair may not match the user-facing one in
 * desktop/version.c.
 */
#define NETSURF_VERSION_MAJOR "3"
#if defined(CI_BUILD)
#define NETSURF_VERSION_MINOR CI_BUILD
#else
#define NETSURF_VERSION_MINOR "2"
#endif


static const __attribute__((used)) char *verstag = "\0$VER: NetSurf " NETSURF_VERSION_MAJOR "." NETSURF_VERSION_MINOR " (" WT_COMPILEDATE ")\0";
const char * const verdate = WT_COMPILEDATE;
const char * const verarexx = NETSURF_VERSION_MAJOR "." NETSURF_VERSION_MINOR;
const char * const wt_revid = WT_REVID;

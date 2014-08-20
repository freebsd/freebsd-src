/*
 * Copyright 2008 John Tytgat <joty@netsurf-browser.org>
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
 * Backward compatible defines to make NetSurf buildable with pre-OSLib 7
 * releases.
 */

#ifndef _NETSURF_RISCOS_OSLIB_PRE7_H_
#define _NETSURF_RISCOS_OSLIB_PRE7_H_

#include "oslib/colourtrans.h"

/**
 * After OSLib 6.90, there was a rename of colourtrans defines in order
 * to avoid namespace clashes:
 *   svn diff -c 238 https://ro-oslib.svn.sourceforge.net/svnroot/ro-oslib/trunk/\!OSLib/Source/Core/oslib/ColourTrans.swi
 * Foresee some backwards compatibility until we've switched to OSLib 7.
*/
#ifndef colourtrans_SET_BG_GCOL
# define colourtrans_SET_BG_GCOL colourtrans_SET_BG
#endif
#ifndef colourtrans_USE_ECFS_GCOL
# define colourtrans_USE_ECFS_GCOL colourtrans_USE_ECFS
#endif

#endif

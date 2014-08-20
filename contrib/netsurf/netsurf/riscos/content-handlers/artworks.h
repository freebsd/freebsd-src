/*
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Content for image/x-artworks (RISC OS interface).
 */

#ifndef _NETSURF_RISCOS_ARTWORKS_H_
#define _NETSURF_RISCOS_ARTWORKS_H_

#include "utils/config.h"
#include "utils/errors.h"

#ifdef WITH_ARTWORKS

nserror artworks_init(void);

#else

#define artworks_init() NSERROR_OK

#endif

#endif

/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Content for image/x-riscos-sprite (RISC OS interface).
 */

#ifndef _NETSURF_RISCOS_SPRITE_H_
#define _NETSURF_RISCOS_SPRITE_H_

#include "utils/config.h"
#include "utils/errors.h"

#ifdef WITH_SPRITE

nserror sprite_init(void);

#else

#define sprite_init() NSERROR_OK

#endif

byte sprite_bpp(const osspriteop_header *s);

#endif

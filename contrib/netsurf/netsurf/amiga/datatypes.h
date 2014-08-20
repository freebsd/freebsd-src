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

#ifndef NETSURF_AMIGA_DATATYPES_H_
#define NETSURF_AMIGA_DATATYPES_H_

#include "utils/config.h"
#include "utils/errors.h"

#ifdef WITH_AMIGA_DATATYPES

nserror amiga_datatypes_init(void);

nserror amiga_dt_picture_init(void);

nserror amiga_dt_anim_init(void);

nserror amiga_dt_sound_init(void);

#else

#define amiga_datatypes_init() NSERROR_OK

#endif /* WITH_AMIGA_DATATYPES */

#endif

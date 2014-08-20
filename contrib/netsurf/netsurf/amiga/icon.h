/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Content for image/x-amiga-icon (icon.library interface).
 */

#ifndef AMIGA_ICON_H
#define AMIGA_ICON_H

#include "utils/config.h"
#include "utils/errors.h"

#ifdef WITH_AMIGA_ICON

nserror amiga_icon_init(void);
void amiga_icon_fini(void);

#else

#define amiga_icon_init() NSERROR_OK
#define amiga_icon_fini() ((void) 0)

#endif /* WITH_AMIGA_ICON */

struct hlcache_handle;

void amiga_icon_superimpose_favicon(char *path, struct hlcache_handle *icon, char *type);
void amiga_icon_superimpose_favicon_internal(struct hlcache_handle *icon, struct DiskObject *dobj);
struct DiskObject *amiga_icon_from_bitmap(struct bitmap *bm);
void amiga_icon_free(struct DiskObject *dobj);
#endif

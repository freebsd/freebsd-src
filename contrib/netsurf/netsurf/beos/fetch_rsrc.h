/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
 *
 * This file is part of NetSurf.
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
 * rsrc: URL method handler
 */

#ifndef NETSURF_BEOS_FETCH_DATA_H
#define NETSURF_BEOS_FETCH_DATA_H

void fetch_rsrc_register(void);
void fetch_rsrc_unregister(void);

class BResources;
BResources *get_app_resources();

#include "beos/res.h"

#endif

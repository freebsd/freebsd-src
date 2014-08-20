/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * RISC OS option setting (interface).
 */


#ifndef _NETSURF_RISCOS_CONFIGURE_H_
#define _NETSURF_RISCOS_CONFIGURE_H_

#include <stdbool.h>
#include "oslib/os.h"
#include "oslib/wimp.h"

void ro_gui_configure_initialise(void);
void ro_gui_configure_show(void);

#endif

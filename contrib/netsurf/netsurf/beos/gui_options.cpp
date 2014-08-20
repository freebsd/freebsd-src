/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#define __STDBOOL_H__	1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
}
#include "beos/gui.h"
#include "beos/scaffolding.h"
#include "beos/gui_options.h"

#include <View.h>
#include <Window.h>

BWindow *wndPreferences;

void nsbeos_options_init(void) {
	/* set the widgets to reflect the current options */
	nsbeos_options_load();
}

void nsbeos_options_load(void) {
#warning WRITEME
}


void nsbeos_options_save(void) {
#warning WRITEME
}

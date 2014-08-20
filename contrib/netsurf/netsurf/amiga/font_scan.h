/*
 * Copyright 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_FONT_SCAN_H
#define AMIGA_FONT_SCAN_H

#include <libwapcaplet/libwapcaplet.h>

void ami_font_scan_init(const char *filename, bool force_scan, bool save,
		lwc_string **glypharray);
void ami_font_scan_fini(lwc_string **glypharray);
void ami_font_scan_save(const char *filename, lwc_string **glypharray);
const char *ami_font_scan_lookup(uint16 *code, lwc_string **glypharray);

#endif

/*
 * Copyright 2006 John-Mark Bell <jmb202@ecs.soton.ac.uk>
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
 * Single/Multi-line UTF-8 text area (interface)
 */

#ifndef _NETSURF_RISCOS_TEXTAREA_H_
#define _NETSURF_RISCOS_TEXTAREA_H_
#include <stdbool.h>
#include <stdint.h>
#include "rufl.h"
#include "oslib/wimp.h"

/* Text area flags */
#define TEXTAREA_MULTILINE	0x01	/**< Text area is multiline */
#define TEXTAREA_READONLY	0x02	/**< Text area is read only */

uintptr_t ro_textarea_create(wimp_w parent, wimp_i icon, unsigned int flags,
		const char *font_family, unsigned int font_size,
		rufl_style font_style);
bool ro_textarea_update(uintptr_t self);
void ro_textarea_destroy(uintptr_t self);
bool ro_textarea_set_text(uintptr_t self, const char *text);
int ro_textarea_get_text(uintptr_t self, char *buf, unsigned int len);
void ro_textarea_insert_text(uintptr_t self, unsigned int index,
		const char *text);
void ro_textarea_replace_text(uintptr_t self, unsigned int start,
		unsigned int end, const char *text);
void ro_textarea_set_caret(uintptr_t self, unsigned int caret);
void ro_textarea_set_caret_xy(uintptr_t self, int x, int y);
unsigned int ro_textarea_get_caret(uintptr_t self);


#endif

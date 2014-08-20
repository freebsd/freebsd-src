/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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
 * A collection of grubby utilities for working with OSLib's wimp API.
 */

#ifndef riscos_wimputils_h_
#define riscos_wimputils_h_

#include <oslib/wimp.h>

/* Magical union to permit aliasing of wimp_window_state and wimp_open
 * Do not use this directly. Use the macros, instead. */
typedef union window_open_state {
	wimp_window_state state;
	wimp_open open;
} window_open_state;

/* Convert a pointer to a wimp_window_state into a pointer to a wimp_open */
#define PTR_WIMP_OPEN(pstate) ((wimp_open *) (window_open_state *) (pstate))

/* Similarly for wimp_message_list */
typedef struct ns_wimp_message_list {
	/* Nasty hack to ensure that we have at least one field in the struct */
	int first;
	int rest[];
} ns_wimp_message_list;

typedef union message_list {
	wimp_message_list wimp;
	ns_wimp_message_list ns;
} message_list;

#define PTR_WIMP_MESSAGE_LIST(l) ((wimp_message_list *) (message_list *) (l))

/* Also for VDU variable lists */
typedef struct ns_os_vdu_var_list {
	os_vdu_var first;
	os_vdu_var rest[];
} ns_os_vdu_var_list;

typedef union vdu_var_list {
	os_vdu_var_list os;
	ns_os_vdu_var_list ns;
} vdu_var_list;

#define PTR_OS_VDU_VAR_LIST(l) ((os_vdu_var_list *) (vdu_var_list *) (l))

#endif

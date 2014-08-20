/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

#ifndef _NETSURF_RISCOS_PRINT_H_
#define _NETSURF_RISCOS_PRINT_H_

#include "utils/config.h"

#include <stdbool.h>
#include "oslib/wimp.h"

struct gui_window;

extern struct gui_window *ro_print_current_window;

void ro_print_save_bounce(wimp_message *m);
void ro_print_error(wimp_message *m);
void ro_print_type_odd(wimp_message *m);
bool ro_print_ack(wimp_message *m);
void ro_print_dataload_bounce(wimp_message *m);
void ro_print_cleanup(void);

#endif

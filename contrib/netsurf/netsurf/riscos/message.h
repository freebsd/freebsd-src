/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Automated RISC OS message routing (interface).
 */


#ifndef _NETSURF_RISCOS_MESSAGE_H_
#define _NETSURF_RISCOS_MESSAGE_H_

#include <stdbool.h>
#include "oslib/wimp.h"

bool ro_message_send_message(wimp_event_no event, wimp_message *message,
		wimp_t task, void (*callback)(wimp_message *message));
bool ro_message_send_message_to_window(wimp_event_no event, wimp_message *message,
		wimp_w to_w, wimp_i to_i, void (*callback)(wimp_message *message),
		wimp_t *to_t);
bool ro_message_register_handler(wimp_message *message,
		unsigned int message_code,
		void (*callback)(wimp_message *message));
bool ro_message_register_route(unsigned int message_code,
		void (*callback)(wimp_message *message));
bool ro_message_handle_message(wimp_event_no event, wimp_message *message);

#endif

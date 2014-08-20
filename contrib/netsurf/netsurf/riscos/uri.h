/*
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
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

#ifndef _NETSURF_RISCOS_URI_H_
#define _NETSURF_RISCOS_URI_H_

#include "utils/config.h"

#include "oslib/wimp.h"

void ro_uri_message_received(wimp_message *message);
bool ro_uri_launch(const char *uri);
void ro_uri_bounce(wimp_message *message);

#endif

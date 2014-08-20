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

#ifndef _NETSURF_UTILS_FILENAME_H_
#define _NETSURF_UTILS_FILENAME_H_

#include <stdbool.h>

#ifdef riscos
#define TEMP_FILENAME_PREFIX "<Wimp$ScrapDir>/WWW/NetSurf/Cache"
#else
#define TEMP_FILENAME_PREFIX "/tmp/WWW/NetSurf/Cache"
#endif

const char *filename_request(void);
bool filename_claim(const char *filename);
void filename_release(const char *filename);
bool filename_initialise(void);
void filename_flush(void);

#endif

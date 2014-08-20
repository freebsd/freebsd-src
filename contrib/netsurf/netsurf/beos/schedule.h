/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#ifndef NETSURF_BEOS_CALLBACK_H
#define NETSURF_BEOS_CALLBACK_H 1

extern bigtime_t earliest_callback_timeout;

extern "C" nserror beos_schedule(int t, void (*callback)(void *p), void *p);

extern "C" bool schedule_run(void);


#endif /* NETSURF_BEOS_CALLBACK_H */

/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef FRAMEBUFFER_SCHEDULE_H
#define FRAMEBUFFER_SCHEDULE_H

/**
 * Schedule a callback.
 *
 * \param  tival     interval before the callback should be made in ms
 * \param  callback  callback function
 * \param  p         user parameter, passed to callback function
 *
 * The callback function will be called as soon as possible after t ms have
 * passed.
 */

nserror framebuffer_schedule(int tival, void (*callback)(void *p), void *p);

/**
 * Process scheduled callbacks up to current time.
 *
 * @return The number of milliseconds untill the next scheduled event
 * or -1 for no event.
 */
int schedule_run(void);

void list_schedule(void);

#endif

/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *           2012 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_SCHEDULE_H
#define NS_ATARI_SCHEDULE_H

/**
 * Process events up to current time.
 */
int schedule_run(void);

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
nserror atari_schedule(int cs_ival, void (*callback)(void *p), void *p);

/**
 * LOG all current scheduled events.
 */
void list_schedule(void);

#endif

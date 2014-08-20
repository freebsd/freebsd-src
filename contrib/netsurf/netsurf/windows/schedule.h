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

#ifndef WINDOWS_SCHEDULE_H
#define WINDOWS_SCHEDULE_H

/**
 * Schedule a callback.
 *
 * \param  ival interval before the callback should be made in ms
 * \param  callback callback function
 * \param  p user parameter, passed to callback function
 *
 * The callback function will be called as soon as possible after t ms have
 * passed.
 */
nserror win32_schedule(int ival, void (*callback)(void *p), void *p);

/**
 * Process scheduled callbacks up to current time.
 *
 * This walks the list of outstanding scheduled events and dispatches
 * them if they have met their scheduled time. Due to legacy issues
 * there are a couple of subtleties with how this operates:
 *
 * - Generally there are so few entries on the list the overhead of
 *     ordering the list exceeds the cost of simply enumerating them.
 *
 * - The scheduled time is the time *after* which we should call the
 *     operation back, this can result in the next scheduled time
 *     being zero. This is exceedingly rare as the core schedules in
 *     10ms (cs) quanta and we almost always get called to schedule
 *     after the event time.
 *
 * - The callbacks can cause the schedule list to be re-arranged added
 *     to or even completely deleted. This means we must reset the
 *     list enumeration to the beginning every time an event is
 *     dispatched.
 *
 * @return The number of milliseconds untill the next scheduled event
 * or -1 for no event.
 */
int schedule_run(void);

/**
 * LOG all current scheduled events.
 */
void list_schedule(void);

#endif

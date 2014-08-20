/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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


#ifndef ATARI_REDRAW_SLOTS_H
#define ATARI_REDRAW_SLOTS_H

#include <mt_gem.h>
#include "utils/types.h"


/*
	MAX_REDRW_SLOTS
	This is the number of redraw requests that the slotlist can store.
	If a redraw is scheduled and all slots are used, the rectangle will
	be merged to one of the existing slots.
 */
#define MAX_REDRW_SLOTS	32

/*
	This struct holds scheduled redraw requests.
*/
struct rect;
struct s_redrw_slots
{
	struct rect areas[MAX_REDRW_SLOTS];
	short size;
	short volatile areas_used;
};

void redraw_slots_init(struct s_redrw_slots * slots, short size);
void redraw_slot_schedule(struct s_redrw_slots * slots, short x0, short y0,
                          short x1, short y1, bool force);
void redraw_slot_schedule_grect(struct s_redrw_slots * slots, GRECT *area,
                                bool force);
void redraw_slots_remove_area(struct s_redrw_slots * slots, int i);
void redraw_slots_free(struct s_redrw_slots * slots);

#endif

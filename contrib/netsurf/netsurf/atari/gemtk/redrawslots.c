/*
 * Copyright 2011 Ole Loots <ole@monochrom.net>
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

#include <stdbool.h>
#define WITH_RECT
#include "gemtk.h"
#undef WITH_RECT

void redraw_slots_init(struct s_redrw_slots * slots, short size)
{
    // TODO: allocate slots dynamically!
	slots->size = MIN( MAX_REDRW_SLOTS , size);
	slots->areas_used = 0;
}

void redraw_slots_free(struct s_redrw_slots * slots)
{
    // TOOD: free areas...
}


static inline bool rect_intersect( struct rect * box1, struct rect * box2 )
{
	if (box2->x1 < box1->x0)
 	       return false;

	if (box2->y1 < box1->y0)
        	return false;

	if (box2->x0 > box1->x1)
        	return false;

	if (box2->y0 > box1->y1)
        	return false;

	return true;
}


void redraw_slot_schedule_grect(struct s_redrw_slots * slots, GRECT *area,
                                bool force)
{
    redraw_slot_schedule(slots, area->g_x, area->g_y,
                         area->g_x + area->g_w, area->g_y + area->g_h, force);
}

/*
	schedule redraw coords.
*/
void redraw_slot_schedule(struct s_redrw_slots * slots, short x0, short y0,
                          short x1, short y1, bool force)
{
	int i = 0;
	struct rect area;

	area.x0 = x0;
	area.y0 = y0;
	area.x1 = x1;
	area.y1 = y1;

    if (force == false) {
        for (i=0; i<slots->areas_used; i++) {
            if (slots->areas[i].x0 <= x0
                && slots->areas[i].x1 >= x1
                && slots->areas[i].y0 <= y0
                && slots->areas[i].y1 >= y1) {
                /* the area is already queued for redraw */
                return;
            } else {
                if (rect_intersect(&slots->areas[i], &area )) {
                    slots->areas[i].x0 = MIN(slots->areas[i].x0, x0);
                    slots->areas[i].y0 = MIN(slots->areas[i].y0, y0);
                    slots->areas[i].x1 = MAX(slots->areas[i].x1, x1);
                    slots->areas[i].y1 = MAX(slots->areas[i].y1, y1);
                    return;
                }
            }
        }
	}

	if (slots->areas_used < slots->size) {
		slots->areas[slots->areas_used].x0 = x0;
		slots->areas[slots->areas_used].x1 = x1;
		slots->areas[slots->areas_used].y0 = y0;
		slots->areas[slots->areas_used].y1 = y1;
		slots->areas_used++;
	} else {
		/*
			we are out of available slots, merge box with last slot
			this is dumb... but also a very rare case.
		*/
		slots->areas[slots->size-1].x0 = MIN(slots->areas[i].x0, x0);
		slots->areas[slots->size-1].y0 = MIN(slots->areas[i].y0, y0);
		slots->areas[slots->size-1].x1 = MAX(slots->areas[i].x1, x1);
		slots->areas[slots->size-1].y1 = MAX(slots->areas[i].y1, y1);
	}
done:
	return;
}

void redraw_slots_remove_area(struct s_redrw_slots * slots, int i)
{
    int x;
    for(x = i+1; i<slots->areas_used; x++){
        slots->areas[x-1] = slots->areas[x];
    }
    slots->areas_used--;
}

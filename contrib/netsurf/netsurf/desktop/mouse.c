/*
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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
 * Browser window creation and manipulation (implementation).
 */

#include "desktop/browser.h"
#include "utils/log.h"

/**
 * Debug function logs a browser mouse state.
 *
 * \param  mouse  browser_mouse_state to dump
 */
void browser_mouse_state_dump(browser_mouse_state mouse)
{
	LOG(("mouse state: %s %s %s %s %s %s %s %s %s %s %s %s %s %s",
			mouse & BROWSER_MOUSE_PRESS_1 		? "P1" : "  ",
			mouse & BROWSER_MOUSE_PRESS_2 		? "P2" : "  ",
			mouse & BROWSER_MOUSE_CLICK_1 		? "C1" : "  ",
			mouse & BROWSER_MOUSE_CLICK_2 		? "C2" : "  ",
			mouse & BROWSER_MOUSE_DOUBLE_CLICK	? "DC" : "  ",
			mouse & BROWSER_MOUSE_TRIPLE_CLICK	? "TC" : "  ",
			mouse & BROWSER_MOUSE_DRAG_1		? "D1" : "  ",
			mouse & BROWSER_MOUSE_DRAG_2 		? "D2" : "  ",
			mouse & BROWSER_MOUSE_DRAG_ON 		? "DO" : "  ",
			mouse & BROWSER_MOUSE_HOLDING_1 	? "H1" : "  ",
			mouse & BROWSER_MOUSE_HOLDING_2 	? "H2" : "  ",
			mouse & BROWSER_MOUSE_MOD_1	 	? "M1" : "  ",
			mouse & BROWSER_MOUSE_MOD_2	 	? "M2" : "  ",
			mouse & BROWSER_MOUSE_MOD_3	 	? "M3" : "  "));
}

/*
 * Copyright 2011 Vincent Sanders <vince@simtec.co.uk>
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

#include <windows.h>

#include "windows/window.h"

/* documented in windows/window.h */
struct gui_window *
nsws_get_gui_window(HWND hwnd)
{
	struct gui_window *gw = NULL;
	HWND phwnd = hwnd;

	/* scan the window hierachy for gui window */
	while (phwnd != NULL) {
		gw = GetProp(phwnd, TEXT("GuiWnd"));
		if (gw != NULL)
			break;
		phwnd = GetParent(phwnd);
	}

	if (gw == NULL) {
		/* try again looking for owner windows instead */
		phwnd = hwnd;
		while (phwnd != NULL) {
			gw = GetProp(phwnd, TEXT("GuiWnd"));
			if (gw != NULL)
				break;
			phwnd = GetWindow(phwnd, GW_OWNER);
		}
	}

	return gw;
}

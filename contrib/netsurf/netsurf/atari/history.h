/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_HISTORY_H
#define NS_ATARI_HISTORY_H

struct core_window;

struct atari_global_history_s {
	GUIWIN * window;
	//struct atari_treeview_window * tv;/*< The hotlist treeview handle.  */
	struct core_window *tv;
	bool init;
};

extern struct atari_global_history_s atari_global_history;

void atari_global_history_init(void);
void atari_global_history_open(void);
void atari_global_history_close(void);
void atari_global_history_destroy(void);
void atari_global_history_redraw(void);

#endif

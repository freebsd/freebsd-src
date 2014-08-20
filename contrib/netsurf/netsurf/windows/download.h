/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#ifndef _NETSURF_WINDOWS_DOWNLOAD_H_
#define _NETSURF_WINDOWS_DOWNLOAD_H_

#include <time.h>
#include <windows.h>
#include "desktop/gui.h"

extern struct gui_download_table *win32_download_table;

typedef enum {
       DOWNLOAD_NONE,
       DOWNLOAD_WORKING,
       DOWNLOAD_ERROR,
       DOWNLOAD_COMPLETE,
       DOWNLOAD_CANCELED
} download_status;

typedef enum {
       DOWNLOAD_PAUSE  = 1 << 0,
       DOWNLOAD_RESUME = 1 << 1,
       DOWNLOAD_CANCEL = 1 << 2,
       DOWNLOAD_CLEAR  = 1 << 3
} download_actions;

struct gui_download_window {
	HWND			hwnd;
	char			*title;
	char			*filename;
	char			*domain;
	char			*time_left;
	char			*total_size;
	char			*original_total_size;
	int			size;
	int			downloaded;
	unsigned int 		progress;
	int			time_remaining;
	struct timeval		start_time;
	int			speed;
	int			error;
	struct gui_window 	*window;
	FILE			*file;
	download_status 	status;
};

void nsws_download_window_init(struct gui_window *);

#endif

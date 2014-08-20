/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_DOWNLOAD_H
#define NS_ATARI_DOWNLOAD_H

extern struct gui_download_table *atari_download_table;

#define MAX_SLEN_LBL_DONE 64
#define MAX_SLEN_LBL_PERCENT 5
#define MAX_SLEN_LBL_SPEED 13
#define MAX_SLEN_LBL_FILE 256
#define MAX_SLEN_BT_ABORT 5

#define DOWNLOAD_BAR_MAX 300

typedef enum {
	NSATARI_DOWNLOAD_NONE,
	NSATARI_DOWNLOAD_WORKING,
	NSATARI_DOWNLOAD_ERROR,
	NSATARI_DOWNLOAD_COMPLETE,
	NSATARI_DOWNLOAD_CANCELED
} nsatari_download_status;

struct gui_download_window {
	struct download_context *ctx;
	struct gui_window * parent;
	GUIWIN *guiwin;
	short aes_handle;
	OBJECT *tree;
	nsatari_download_status status;
	char *destination;
	FILE * fd;
	char lbl_done[MAX_SLEN_LBL_DONE];
	char lbl_percent[MAX_SLEN_LBL_PERCENT];
	char lbl_speed[MAX_SLEN_LBL_SPEED];
	char lbl_file[MAX_SLEN_LBL_FILE];
	uint32_t start;
	uint32_t lastrdw;
	uint32_t size_total;
	uint32_t size_downloaded;
	char * fbuf;
	size_t fbufsize;
	bool abort;
	bool close_on_finish;
};

#endif

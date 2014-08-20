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

#ifndef NS_ATARI_MISC_H
#define NS_ATARI_MISC_H

#include "cflib.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/textinput.h"
#include "atari/gui.h"

#define SBUF8_TO_LBUF8(sbuf,lbuf)\
	lbuf[0] = (long)sbuf[0];\
	lbuf[1] = (long)sbuf[1];\
	lbuf[2] = (long)sbuf[2];\
	lbuf[3] = (long)sbuf[3];\
	lbuf[4] = (long)sbuf[4];\
	lbuf[5] = (long)sbuf[5];\
	lbuf[6] = (long)sbuf[6];\
	lbuf[7] = (long)sbuf[7];

#define RECT_TO_GRECT(r,g) \
	(g)->g_x = (r->x0 < r->x1) ? r->x0 : r->x1 ; \
	(g)->g_y = (r->y0 < r->y1) ? r->y0 : r->y1 ; \
	(g)->g_w = (r->x0 < r->x1) ? r->x1 - r->x0 : r->x0 - r->x1 ; \
	(g)->g_h = (r->y0 < r->y1) ? r->y1 - r->y0 : r->y0 - r->y1 ;



/* Modes for find_gui_window: */
#define BY_WINDOM_HANDLE 0x0
#define BY_GEM_HANDLE    0x1

typedef int (*scan_process_callback)(int pid, void *data);

struct gui_window * find_guiwin_by_aes_handle(short handle);
bool is_process_running(const char * name);
void gem_set_cursor( MFORM_EX * cursor );
hlcache_handle *load_icon( const char *name, hlcache_handle_callback cb,
                           void * pw );
void dbg_grect(const char * str, GRECT * r);
void dbg_pxy(const char * str, short * pxy);
void dbg_rect(const char * str, int * pxy);

const char * file_select(const char * title, const char * name);

/**
 * Convert NKC (atari normalized key code) to netsurf
 * Input key code and/or to ucs4 (depends on keycode).
*/
long nkc_to_input_key(short nkc, long * ucs4_out);

bool path_add_part(char *path, int length, const char *newpart);

#endif

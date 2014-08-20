/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#ifndef NETSURF_MONKEY_BROWSER_H
#define NETSURF_MONKEY_BROWSER_H

#include "desktop/browser.h"
#include "content/hlcache.h"

extern struct gui_window_table *monkey_window_table;
extern struct gui_download_table *monkey_download_table;

struct gui_window {
  struct gui_window *r_next;
  struct gui_window *r_prev;
  
  uint32_t win_num;
  struct browser_window *bw;
  
  int width, height;
  int scrollx, scrolly;
  
  char *host;  /* Ignore this, it's in case RING*() gets debugging for fetchers */
  
};

struct gui_window *monkey_find_window_by_num(uint32_t win_num);
struct gui_window *monkey_find_window_by_content(hlcache_handle *content);
void monkey_window_process_reformats(void);

void monkey_window_handle_command(int argc, char **argv);
void monkey_kill_browser_windows(void);

#endif /* NETSURF_MONKEY_BROWSER_H */

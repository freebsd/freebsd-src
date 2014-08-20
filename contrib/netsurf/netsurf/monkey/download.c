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

#include <stdio.h>

#include "desktop/gui.h"
#include "desktop/download.h"
#include "utils/ring.h"

#include "monkey/browser.h"

static uint32_t dwin_ctr = 0;

struct gui_download_window {
  struct gui_download_window *r_next;
  struct gui_download_window *r_prev;
  struct gui_window *g;
  uint32_t dwin_num;
  char *host; /* ignore */
};

static struct gui_download_window *dw_ring = NULL;

static struct gui_download_window *
gui_download_window_create(download_context *ctx,
                           struct gui_window *parent)
{
  struct gui_download_window *ret = calloc(sizeof(*ret), 1);
  if (ret == NULL)
    return NULL;
  ret->g = parent;
  ret->dwin_num = dwin_ctr++;
  
  RING_INSERT(dw_ring, ret);
  
  fprintf(stdout, "DOWNLOAD_WINDOW CREATE DWIN %u WIN %u\n", 
          ret->dwin_num, parent->win_num);
  
  return ret;
}

static nserror 
gui_download_window_data(struct gui_download_window *dw, 
                         const char *data, unsigned int size)
{
  fprintf(stdout, "DOWNLOAD_WINDOW DATA DWIN %u SIZE %u DATA %s\n",
          dw->dwin_num, size, data);
  return NSERROR_OK;
}

static void
gui_download_window_error(struct gui_download_window *dw,
                          const char *error_msg)
{
  fprintf(stdout, "DOWNLOAD_WINDOW ERROR DWIN %u ERROR %s\n",
          dw->dwin_num, error_msg);
}

static void
gui_download_window_done(struct gui_download_window *dw)
{
  fprintf(stdout, "DOWNLOAD_WINDOW DONE DWIN %u\n",
          dw->dwin_num);
  RING_REMOVE(dw_ring, dw);
  free(dw);
}

static struct gui_download_table download_table = {
	.create = gui_download_window_create,
	.data = gui_download_window_data,
	.error = gui_download_window_error,
	.done = gui_download_window_done,
};

struct gui_download_table *monkey_download_table = &download_table;

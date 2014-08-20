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

#include "monkey/browser.h"
#include "desktop/thumbnail.h"

bool thumbnail_create(hlcache_handle *content, struct bitmap *bitmap,
		nsurl *url)
{
  struct gui_window *win = monkey_find_window_by_content(content);
  if (win == NULL) {
    fprintf(stdout, "GENERIC THUMBNAIL URL %s\n", nsurl_access(url));
  } else {
    fprintf(stdout, "WINDOW THUMBNAIL WIN %u URL %s\n", win->win_num, nsurl_access(url));
  }
  return false;
}

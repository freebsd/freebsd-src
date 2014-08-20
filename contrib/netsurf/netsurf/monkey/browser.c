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

/* Browser-related callbacks */

#include <stdio.h>


#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "utils/ring.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "monkey/browser.h"
#include "monkey/plot.h"

static uint32_t win_ctr = 0;

static struct gui_window *gw_ring = NULL;

struct gui_window *
monkey_find_window_by_num(uint32_t win_num)
{
  struct gui_window *ret = NULL;
  
  RING_ITERATE_START(struct gui_window, gw_ring, c_ring) {
    if (c_ring->win_num == win_num) {
      ret = c_ring;
      RING_ITERATE_STOP(gw_ring, c_ring);
    }
  } RING_ITERATE_END(gw_ring, c_ring);
  
  return ret;
}

struct gui_window *
monkey_find_window_by_content(hlcache_handle *content)
{
  struct gui_window *ret = NULL;
  
  RING_ITERATE_START(struct gui_window, gw_ring, c_ring) {
    if (c_ring->bw->current_content == content) {
      ret = c_ring;
      RING_ITERATE_STOP(gw_ring, c_ring);
    }
  } RING_ITERATE_END(gw_ring, c_ring);
  
  return ret;
}

void
monkey_window_process_reformats(void)
{
  RING_ITERATE_START(struct gui_window, gw_ring, c_ring) {
    if (c_ring == NULL)
      RING_ITERATE_STOP(gw_ring, c_ring);
    if (c_ring->bw->reformat_pending) {
      browser_window_reformat(c_ring->bw,
                              false,
                              c_ring->width,
                              c_ring->height);
    }
  } RING_ITERATE_END(gw_ring, c_ring);
}

void
monkey_kill_browser_windows(void)
{
  while (gw_ring != NULL) {
    browser_window_destroy(gw_ring->bw);
  }
}

static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
  struct gui_window *ret = calloc(sizeof(*ret), 1);
  if (ret == NULL)
    return NULL;
  
  ret->win_num = win_ctr++;
  ret->bw = bw;
  
  ret->width = 800;
  ret->height = 600;
  
  fprintf(stdout, "WINDOW NEW WIN %u FOR %p EXISTING %p NEWTAB %s CLONE %s\n",
          ret->win_num, bw, existing, flags & GW_CREATE_TAB ? "TRUE" : "FALSE",
          flags & GW_CREATE_CLONE ? "TRUE" : "FALSE");
  fprintf(stdout, "WINDOW SIZE WIN %u WIDTH %d HEIGHT %d\n",
          ret->win_num, ret->width, ret->height);
  
  RING_INSERT(gw_ring, ret);
  
  return ret;
}

static void
gui_window_destroy(struct gui_window *g)
{
  fprintf(stdout, "WINDOW DESTROY WIN %u\n", g->win_num);
  RING_REMOVE(gw_ring, g);
  free(g);
}

static void
gui_window_set_title(struct gui_window *g, const char *title)
{
  fprintf(stdout, "WINDOW TITLE WIN %u STR %s\n", g->win_num, title);
}

static void
gui_window_redraw_window(struct gui_window *g)
{
  fprintf(stdout, "WINDOW REDRAW WIN %u\n", g->win_num);
}

static void
gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
                          bool scaled)
{
  fprintf(stdout, "WINDOW GET_DIMENSIONS WIN %u WIDTH %d HEIGHT %d\n",
          g->win_num, g->width, g->height);
  *width = g->width;
  *height = g->height;
}

static void
gui_window_new_content(struct gui_window *g)
{
  fprintf(stdout, "WINDOW NEW_CONTENT WIN %u\n", g->win_num);
}

static void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
  fprintf(stdout, "WINDOW NEW_ICON WIN %u\n", g->win_num);
}

static void
gui_window_start_throbber(struct gui_window *g)
{
  fprintf(stdout, "WINDOW START_THROBBER WIN %u\n", g->win_num);
}

static void
gui_window_stop_throbber(struct gui_window *g)
{
  fprintf(stdout, "WINDOW STOP_THROBBER WIN %u\n", g->win_num);
}

static void
gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
  g->scrollx = sx;
  g->scrolly = sy;
  fprintf(stdout, "WINDOW SET_SCROLL WIN %u X %d Y %d\n", g->win_num, sx, sy);
}

static void
gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
  fprintf(stdout, "WINDOW UPDATE_BOX WIN %u X %d Y %d WIDTH %d HEIGHT %d\n",
          g->win_num, rect->x0, rect->y0,
          (rect->x1 - rect->x0), (rect->y1 - rect->y0));
  
}

static void
gui_window_update_extent(struct gui_window *g)
{
  if (!g->bw->current_content)
    return;

  fprintf(stdout, "WINDOW UPDATE_EXTENT WIN %u WIDTH %d HEIGHT %d\n", 
          g->win_num,
          content_get_width(g->bw->current_content),
          content_get_height(g->bw->current_content));
}

static void
gui_window_set_status(struct gui_window *g, const char *text)
{
  fprintf(stdout, "WINDOW SET_STATUS WIN %u STR %s\n", g->win_num, text);
}

static void
gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
  const char *ptr_name = "UNKNOWN";
  
  switch (shape) {
  case GUI_POINTER_POINT:
    ptr_name = "POINT";
    break;
  case GUI_POINTER_CARET:
    ptr_name = "CARET";
    break;
  case GUI_POINTER_UP:
    ptr_name = "UP";
    break;
  case GUI_POINTER_DOWN:
    ptr_name = "DOWN";
    break;
  case GUI_POINTER_LEFT:
    ptr_name = "LEFT";
    break;
  case GUI_POINTER_RIGHT:
    ptr_name = "RIGHT";
    break;
  case GUI_POINTER_LD:
    ptr_name = "LD";
    break;
  case GUI_POINTER_RD:
    ptr_name = "RD";
    break;
  case GUI_POINTER_LU:
    ptr_name = "LU";
    break;
  case GUI_POINTER_RU:
    ptr_name = "RU";
    break;
  case GUI_POINTER_CROSS:
    ptr_name = "CROSS";
    break;
  case GUI_POINTER_MOVE:
    ptr_name = "MOVE";
    break;
  case GUI_POINTER_WAIT:
    ptr_name = "WAIT";
    break;
  case GUI_POINTER_HELP:
    ptr_name = "HELP";
    break;
  case GUI_POINTER_MENU:
    ptr_name = "MENU";
    break;
  case GUI_POINTER_PROGRESS:
    ptr_name = "PROGRESS";
    break;
  case GUI_POINTER_NO_DROP:
    ptr_name = "NO_DROP";
    break;
  case GUI_POINTER_NOT_ALLOWED:
    ptr_name = "NOT_ALLOWED";
    break;
  case GUI_POINTER_DEFAULT:
    ptr_name = "DEFAULT";
    break;
  default:
    break;
  }
  fprintf(stdout, "WINDOW SET_POINTER WIN %u POINTER %s\n", g->win_num, ptr_name);
}

static void
gui_window_set_url(struct gui_window *g, const char *url)
{
  fprintf(stdout, "WINDOW SET_URL WIN %u URL %s\n", g->win_num, url);
}

static bool
gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
  fprintf(stdout, "WINDOW GET_SCROLL WIN %u X %d Y %d\n",
          g->win_num, g->scrollx, g->scrolly);
  *sx = g->scrollx;
  *sy = g->scrolly;
  return true;
}

static bool
gui_window_scroll_start(struct gui_window *g)
{
  fprintf(stdout, "WINDOW SCROLL_START WIN %u\n", g->win_num);
  g->scrollx = g->scrolly = 0;
  return true;
}

static void
gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
                          int x1, int y1)
{
  fprintf(stdout, "WINDOW SCROLL_VISIBLE WIN %u X0 %d Y0 %d X1 %d Y1 %d\n",
          g->win_num, x0, y0, x1, y1);
}

static void
gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip)
{
  fprintf(stdout, "WINDOW PLACE_CARET WIN %u X %d Y %d HEIGHT %d\n",
          g->win_num, x, y, height);
}

static void
gui_window_remove_caret(struct gui_window *g)
{
  fprintf(stdout, "WINDOW REMOVE_CARET WIN %u\n", g->win_num);
}

static bool
gui_window_drag_start(struct gui_window *g, gui_drag_type type,
                      const struct rect *rect)
{
  fprintf(stdout, "WINDOW SCROLL_START WIN %u TYPE %i\n", g->win_num, type);
  return false;
}

static void
gui_window_save_link(struct gui_window *g, const char *url, 
                     const char *title)
{
  fprintf(stdout, "WINDOW SAVE_LINK WIN %u URL %s TITLE %s\n",
          g->win_num, url, title);
}



/**** Handlers ****/

static void
monkey_window_handle_new(int argc, char **argv)
{
  nsurl *url = NULL;
  nserror error = NSERROR_OK;

  if (argc > 3)
    return;

  if (argc == 3) {
    error = nsurl_create(argv[2], &url);
  }
  if (error == NSERROR_OK) {
    error = browser_window_create(BW_CREATE_HISTORY,
				  url,
				  NULL,
				  NULL,
				  NULL);
    if (url != NULL) {
      nsurl_unref(url);
    }
  }
  if (error != NSERROR_OK) {
    warn_user(messages_get_errorcode(error), 0);
  }
}

static void
monkey_window_handle_destroy(int argc, char **argv)
{
  struct gui_window *gw;
  uint32_t nr = atoi((argc > 2) ? argv[2] : "-1");
  
  gw = monkey_find_window_by_num(nr);
  
  if (gw == NULL) {
    fprintf(stdout, "ERROR WINDOW NUM BAD\n");
  } else {
    browser_window_destroy(gw->bw);
  }
}

static void
monkey_window_handle_go(int argc, char **argv)
{
  struct gui_window *gw;
  
  if (argc < 4 || argc > 5) {
    fprintf(stdout, "ERROR WINDOW GO ARGS BAD\n");
    return;
  }
  
  gw = monkey_find_window_by_num(atoi(argv[2]));
  
  if (gw == NULL) {
    fprintf(stdout, "ERROR WINDOW NUM BAD\n");
  } else {
    nsurl *url;
    nsurl *ref_url = NULL;
    nserror error;

    error = nsurl_create(argv[3], &url);
    if (error != NSERROR_OK) {
      warn_user(messages_get_errorcode(error), 0);
    } else {
      if (argc == 5) {
	error = nsurl_create(argv[4], &ref_url);
      }

      browser_window_navigate(gw->bw,
			      url,
			      ref_url,
			      BW_NAVIGATE_HISTORY,
			      NULL,
			      NULL,
			      NULL);
      nsurl_unref(url);
      if (ref_url != NULL) {
	nsurl_unref(ref_url);
      }
    }
  }
}

static void
monkey_window_handle_redraw(int argc, char **argv)
{
  struct gui_window *gw;
  struct rect clip;
  struct redraw_context ctx = {
    .interactive = true,
    .background_images = true,
    .plot = &monkey_plotters
  };
  
  if (argc != 3 && argc != 7) {
    fprintf(stdout, "ERROR WINDOW REDRAW ARGS BAD\n");
    return;
  }

  gw = monkey_find_window_by_num(atoi(argv[2]));
  
  if (gw == NULL) {
    fprintf(stdout, "ERROR WINDOW NUM BAD\n");
    return;
  }
  
  clip.x0 = 0;
  clip.y0 = 0;
  clip.x1 = gw->width;
  clip.y1 = gw->height;
  
  if (argc == 7) {
    clip.x0 = atoi(argv[3]);
    clip.y0 = atoi(argv[4]);
    clip.x1 = atoi(argv[5]);
    clip.y1 = atoi(argv[6]);
  }
  
  LOG(("Issue redraw"));
  fprintf(stdout, "WINDOW REDRAW WIN %d START\n", atoi(argv[2]));
  browser_window_redraw(gw->bw, gw->scrollx, gw->scrolly, &clip, &ctx);  
  fprintf(stdout, "WINDOW REDRAW WIN %d STOP\n", atoi(argv[2]));
}

static void
monkey_window_handle_reload(int argc, char **argv)
{
  struct gui_window *gw;
  if (argc != 3 && argc != 4) {
    fprintf(stdout, "ERROR WINDOW RELOAD ARGS BAD\n");
  }
  
  gw = monkey_find_window_by_num(atoi(argv[2]));
  
  if (gw == NULL) {
    fprintf(stdout, "ERROR WINDOW NUM BAD\n");
  } else {
    browser_window_reload(gw->bw, argc == 4);
  }
}


void
monkey_window_handle_command(int argc, char **argv)
{
  if (argc == 1)
    return;
  
  if (strcmp(argv[1], "NEW") == 0) {
    monkey_window_handle_new(argc, argv);
  } else if (strcmp(argv[1], "DESTROY") == 0) {
    monkey_window_handle_destroy(argc, argv);
  } else if (strcmp(argv[1], "GO") == 0) {
    monkey_window_handle_go(argc, argv);
  } else if (strcmp(argv[1], "REDRAW") == 0) {
    monkey_window_handle_redraw(argc, argv);
  } else if (strcmp(argv[1], "RELOAD") == 0) {
    monkey_window_handle_reload(argc, argv);
  } else {
    fprintf(stdout, "ERROR WINDOW COMMAND UNKNOWN %s\n", argv[1]);
  }
  
}

static struct gui_window_table window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.redraw = gui_window_redraw_window,
	.update = gui_window_update_box,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.update_extent = gui_window_update_extent,

	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_icon = gui_window_set_icon,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
	.remove_caret = gui_window_remove_caret,
	.drag_start = gui_window_drag_start,
	.save_link = gui_window_save_link,
	.scroll_visible = gui_window_scroll_visible,
	.scroll_start = gui_window_scroll_start,
	.new_content = gui_window_new_content,
	.start_throbber = gui_window_start_throbber,
	.stop_throbber = gui_window_stop_throbber,
};

struct gui_window_table *monkey_window_table = &window_table;

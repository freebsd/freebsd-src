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

#include <assert.h>

#include "desktop/browser.h"
#include "desktop/gui.h"
#include "monkey/schedule.h"
#include "monkey/browser.h"
#include "content/fetchers/curl.h"
#include "monkey/dispatch.h"
#include "monkey/poll.h"

#ifdef DEBUG_POLL_LOOP
#include "utils/log.h"
#else
#define LOG(X)
#endif


#include <glib.h>

typedef struct {
  GSource gs;
  GPollFD pf;
} MonkeySource;

static gboolean monkey_source_prepare(GSource    *source,
                               gint       *timeout_)
{
  *timeout_ = -1;
  return FALSE;
}


static gboolean monkey_source_check(GSource    *source)
{
  MonkeySource *ms = (MonkeySource *)source;
  if (ms->pf.revents & G_IO_IN) {
    return TRUE;
  }
  return FALSE;
}

static gboolean monkey_source_dispatch(GSource    *source,
                                GSourceFunc callback,
                                gpointer    user_data)
{
  monkey_process_command();
  return TRUE;
}

static void monkey_source_finalize(GSource    *source)
{
  
}

GSourceFuncs monkey_source_funcs = {
  .prepare = monkey_source_prepare,
  .check = monkey_source_check,
  .dispatch = monkey_source_dispatch,
  .finalize = monkey_source_finalize,
};

void
monkey_prepare_input(void)
{
  MonkeySource *gs = (MonkeySource *)g_source_new(&monkey_source_funcs, sizeof *gs);
  gs->pf.fd = 0;
  gs->pf.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll((GSource *)gs, &gs->pf);
  g_source_attach((GSource *)gs, NULL);
}

void
monkey_poll(bool active)
{
  CURLMcode code;
  fd_set read_fd_set, write_fd_set, exc_fd_set;
  int max_fd;
  GPollFD *fd_list[1000];
  unsigned int fd_count = 0;
  bool block = true;
        
  schedule_run();

  if (browser_reformat_pending)
    block = false;

  if (active) {
    FD_ZERO(&read_fd_set);
    FD_ZERO(&write_fd_set);
    FD_ZERO(&exc_fd_set);
    code = curl_multi_fdset(fetch_curl_multi,
                            &read_fd_set,
                            &write_fd_set,
                            &exc_fd_set,
                            &max_fd);
    assert(code == CURLM_OK);
    LOG(("maxfd from curl is %d", max_fd));
    for (int i = 0; i <= max_fd; i++) {
      if (FD_ISSET(i, &read_fd_set)) {
        GPollFD *fd = malloc(sizeof *fd);
        fd->fd = i;
        fd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
        g_main_context_add_poll(0, fd, 0);
        fd_list[fd_count++] = fd;
        LOG(("Want to read %d", i));
      }
      if (FD_ISSET(i, &write_fd_set)) {
        GPollFD *fd = malloc(sizeof *fd);
        fd->fd = i;
        fd->events = G_IO_OUT | G_IO_ERR;
        g_main_context_add_poll(0, fd, 0);
        fd_list[fd_count++] = fd;
        LOG(("Want to write %d", i));
      }
      if (FD_ISSET(i, &exc_fd_set)) {
        GPollFD *fd = malloc(sizeof *fd);
        fd->fd = i;
        fd->events = G_IO_ERR;
        g_main_context_add_poll(0, fd, 0);
        fd_list[fd_count++] = fd;
        LOG(("Want to check %d", i));
      }
    }
  }
  
  LOG(("Iterate %sactive %sblocking", active?"":"in", block?"":"non-"));
  if (block) {
    fprintf(stdout, "GENERIC POLL BLOCKING\n");
  }
  g_main_context_iteration(g_main_context_default(), block);

  for (unsigned int i = 0; i != fd_count; i++) {
    g_main_context_remove_poll(0, fd_list[i]);
    free(fd_list[i]);
  }

  schedule_run();

  if (browser_reformat_pending)
    monkey_window_process_reformats();
}


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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/nsoption.h"
#include "content/urldb.h"
#include "content/fetchers/resource.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "utils/log.h"
#include "utils/filepath.h"
#include "utils/url.h"

#include "monkey/poll.h"
#include "monkey/dispatch.h"
#include "monkey/browser.h"
#include "monkey/cert.h"
#include "monkey/401login.h"
#include "monkey/filetype.h"
#include "monkey/fetch.h"
#include "monkey/schedule.h"

char **respaths; /** resource search path vector */

/* Stolen from gtk/gui.c */
static char **
nsmonkey_init_resource(const char *resource_path)
{
  const gchar * const *langv;
  char **pathv; /* resource path string vector */
  char **respath; /* resource paths vector */

  pathv = filepath_path_to_strvec(resource_path);

  langv = g_get_language_names();

  respath = filepath_generate(pathv, langv);

  filepath_free_strvec(pathv);

  return respath;
}

static void monkey_quit(void)
{
  urldb_save_cookies(nsoption_charp(cookie_jar));
  urldb_save(nsoption_charp(url_file));
  free(nsoption_charp(cookie_file));
  free(nsoption_charp(cookie_jar));
  monkey_fetch_filetype_fin();
}

static void
gui_launch_url(const char *url)
{
  fprintf(stdout, "GENERIC LAUNCH URL %s\n", url);
}

static void quit_handler(int argc, char **argv)
{
  netsurf_quit = true;
}

/**
 * Set option defaults for monkey frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
  /* currently no default overrides */
  return NSERROR_OK;
}

/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
  /* set log stream to be non-buffering */
  setbuf(fptr, NULL);

  return true;
}

static struct gui_browser_table monkey_browser_table = {
  .poll = monkey_poll,
  .schedule = monkey_schedule,

  .quit = monkey_quit,
  .launch_url = gui_launch_url,
  .cert_verify = gui_cert_verify,
  .login = gui_401login_open,
};

int
main(int argc, char **argv)
{
  char *messages;
  char *options;
  char buf[PATH_MAX];
  nserror ret;
  struct gui_table monkey_gui_table = {
    .browser = &monkey_browser_table,
    .window = monkey_window_table,
    .download = monkey_download_table,
    .fetch = monkey_fetch_table,
  };

  /* Unbuffer stdin/out/err */
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* Prep the search paths */
  respaths = nsmonkey_init_resource("${HOME}/.netsurf/:${NETSURFRES}:"MONKEY_RESPATH":./monkey/res");

  /* initialise logging. Not fatal if it fails but not much we can do
   * about it either.
   */
  nslog_init(nslog_stream_configure, &argc, argv);

  /* user options setup */
  ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
  if (ret != NSERROR_OK) {
    die("Options failed to initialise");
  }
  options = filepath_find(respaths, "Choices");
  nsoption_read(options, nsoptions);
  free(options);
  nsoption_commandline(&argc, argv, nsoptions);

  /* common initialisation */
  messages = filepath_find(respaths, "Messages");
  ret = netsurf_init(messages, &monkey_gui_table);
  free(messages);
  if (ret != NSERROR_OK) {
    die("NetSurf failed to initialise");
  }

  filepath_sfinddef(respaths, buf, "mime.types", "/etc/");
  monkey_fetch_filetype_init(buf);

  urldb_load(nsoption_charp(url_file));
  urldb_load_cookies(nsoption_charp(cookie_file));

  monkey_prepare_input();
  monkey_register_handler("QUIT", quit_handler);
  monkey_register_handler("WINDOW", monkey_window_handle_command);

  fprintf(stdout, "GENERIC STARTED\n");
  netsurf_main_loop();
  fprintf(stdout, "GENERIC CLOSING_DOWN\n");
  monkey_kill_browser_windows();

  netsurf_exit();
  fprintf(stdout, "GENERIC FINISHED\n");

  /* finalise options */
  nsoption_finalise(nsoptions, nsoptions_default);

  return 0;
}

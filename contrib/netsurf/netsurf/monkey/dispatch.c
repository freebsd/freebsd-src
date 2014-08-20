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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/ring.h"

#include "desktop/netsurf.h"
#include "monkey/dispatch.h"

typedef struct cmdhandler {
  struct cmdhandler *r_next, *r_prev;
  const char *cmd;
  handle_command_fn fn;
} monkey_cmdhandler_t;

static monkey_cmdhandler_t *handler_ring = NULL;

void
monkey_register_handler(const char *cmd, handle_command_fn fn)
{
  monkey_cmdhandler_t *ret = calloc(sizeof(*ret), 1);
  if (ret == NULL)
    die("Unable to allocate handler");
  ret->cmd = strdup(cmd);
  ret->fn = fn;
  RING_INSERT(handler_ring, ret);
}

void
monkey_process_command(void)
{
  char buffer[PATH_MAX];
  int argc = 0;
  char **argv = NULL;
  char *p, *r = NULL;
  handle_command_fn fn = NULL;
  
  if (fgets(buffer, PATH_MAX, stdin) == NULL) {
    netsurf_quit = true;
  }
  
  buffer[strlen(buffer)-1] = '\0';
  
  argv = malloc(sizeof *argv);
  argc = 1;
  *argv = buffer;
  
  for (p = r = buffer; *p != '\0'; p++) {
    if (*p == ' ') {
      argv = realloc(argv, sizeof(*argv) * (argc + 1));
      argv[argc++] = r = p + 1;
      *p = '\0';
    }
  }
  
  RING_ITERATE_START(monkey_cmdhandler_t, handler_ring, handler) {
    if (strcmp(argv[0], handler->cmd) == 0) {
      fn = handler->fn;
      RING_ITERATE_STOP(handler_ring, handler);
    }
  } RING_ITERATE_END(handler_ring, handler);
  
  if (fn != NULL) {
    fn(argc, argv);
  }

  free(argv);
}

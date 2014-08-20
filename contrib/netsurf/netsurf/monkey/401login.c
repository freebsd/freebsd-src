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

#include "utils/ring.h"

#include <stdlib.h>
#include <stdio.h>

#include "monkey/401login.h"

typedef struct monkey401 {
  struct monkey401 *r_next, *r_prev;
  uint32_t num;
  lwc_string *host; /* Ignore */
  nserror (*cb)(bool,void*);
  void *pw;
} monkey401_t;

static monkey401_t *m4_ring = NULL;
static uint32_t m4_ctr = 0;

void gui_401login_open(nsurl *url, const char *realm,
                       nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
  monkey401_t *m4t = calloc(sizeof(*m4t), 1);
  if (m4t == NULL) {
    cb(false, cbpw);
    return;
  }
  m4t->cb = cb;
  m4t->pw = cbpw;
  m4t->num = m4_ctr++;
  
  RING_INSERT(m4_ring, m4t);
  
  fprintf(stdout, "401LOGIN OPEN M4 %u URL %s REALM %s\n",
          m4t->num, nsurl_access(url), realm);
}



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

#include <stdlib.h>
#include <stdio.h>

#include "utils/ring.h"
#include "utils/nsurl.h"
#include "desktop/gui.h"

#include "monkey/cert.h"

typedef struct monkey_cert {
  struct monkey_cert *r_next, *r_prev;
  uint32_t num;
  char *host; /* Ignore */
  nserror (*cb)(bool,void*);
  void *pw;
} monkey_cert_t;

static monkey_cert_t *cert_ring = NULL;
static uint32_t cert_ctr = 0;

void 
gui_cert_verify(nsurl *url, const struct ssl_cert_info *certs, 
                unsigned long num, nserror (*cb)(bool proceed, void *pw),
                void *cbpw)
{
  monkey_cert_t *m4t = calloc(sizeof(*m4t), 1);
  if (m4t == NULL) {
    cb(false, cbpw);
    return;
  }
  m4t->cb = cb;
  m4t->pw = cbpw;
  m4t->num = cert_ctr++;
  
  RING_INSERT(cert_ring, m4t);
  
  fprintf(stdout, "SSLCERT VERIFY CERT %u URL %s\n",
          m4t->num, nsurl_access(url));
}



/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id:$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "sync.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "async.h"
#include "descriptor.h"
#include "physical.h"
#include "tcp.h"

static int
OpenConnection(const char *name, char *host, char *port)
{
  struct sockaddr_in dest;
  int sock;
  struct servent *sp;

  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = inet_addr(host);
  dest.sin_addr = GetIpAddr(host);
  if (dest.sin_addr.s_addr == INADDR_NONE) {
    log_Printf(LogWARN, "%s: %s: unknown host\n", name, host);
    return (-1);
  }
  dest.sin_port = htons(atoi(port));
  if (dest.sin_port == 0) {
    sp = getservbyname(port, "tcp");
    if (sp) {
      dest.sin_port = sp->s_port;
    } else {
      log_Printf(LogWARN, "%s: %s: unknown service\n", name, port);
      return (-1);
    }
  }
  log_Printf(LogPHASE, "%s: Connecting to %s:%s\n", name, host, port);

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return (sock);
  }
  if (connect(sock, (struct sockaddr *)&dest, sizeof dest) < 0) {
    log_Printf(LogWARN, "%s: connect: %s\n", name, strerror(errno));
    close(sock);
    return (-1);
  }
  return (sock);
}

static int
tcp_Open(struct physical *p)
{
  char *cp, *host, *port;

  if ((cp = strchr(p->name.full, ':')) != NULL) {
    *cp = '\0';
    host = p->name.full;
    port = cp + 1;
    if (*host && *port) {
      p->fd = OpenConnection(p->link.name, host, port);
      *cp = ':';		/* Don't destroy name.full */
      if (p->fd >= 0) {
        log_Printf(LogDEBUG, "%s: Opened socket %s\n", p->link.name,
                   p->name.full);
        physical_SetupStack(p, 1);
        return 1;
      }
    } else
      *cp = ':';		/* Don't destroy name.full */
  }

  return 0;
}

const struct device tcpdevice = {
  TTY_DEVICE,
  "tcp",
  tcp_Open,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/*
 * Copyright 1999 Internet Business Solutions Ltd., Switzerland
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
 *
 */

#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include <errno.h>
#include <radlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "defs.h"
#include "log.h"
#include "descriptor.h"
#include "prompt.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "slcompress.h"
#include "throughput.h"
#include "lqr.h"
#include "hdlc.h"
#include "mbuf.h"
#include "ipcp.h"
#include "route.h"
#include "command.h"
#include "filter.h"
#include "server.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "radius.h"
#include "bundle.h"

void
radius_Init(struct radius *r)
{
  r->valid = 0;
  *r->cfg.file = '\0';;
}

void
radius_Destroy(struct radius *r)
{
  r->valid = 0;
  route_DeleteAll(&r->routes);
}

int
radius_Authenticate(struct radius *r, struct bundle *bundle, const char *name,
                    const char *key, const char *challenge)
{
  struct rad_handle *h;
  sigset_t alrm, prevset;
  const void *data;
  int got, len, argc, addrs;
  char *argv[MAXARGS], *nuke;
  struct in_range dest;
  struct in_addr gw;

  radius_Destroy(r);

  if (!*r->cfg.file)
    return 0;

  if ((h = rad_open()) == NULL) {
    log_Printf(LogERROR, "rad_open: %s\n", strerror(errno));
    return 0;
  }

  if (rad_config(h, r->cfg.file) != 0) {
    log_Printf(LogERROR, "rad_config: %s\n", rad_strerror(h));
    rad_close(h);
    return 0;
  }

  if (rad_create_request(h, RAD_ACCESS_REQUEST) != 0) {
    log_Printf(LogERROR, "rad_create_request: %s\n", rad_strerror(h));
    rad_close(h);
    return 0;
  }

  if (rad_put_string(h, RAD_USER_NAME, name) != 0 ||
      rad_put_int(h, RAD_SERVICE_TYPE, RAD_FRAMED) != 0 ||
      rad_put_int(h, RAD_FRAMED_PROTOCOL, RAD_PPP) != 0) {
    log_Printf(LogERROR, "rad_put: %s\n", rad_strerror(h));
    rad_close(h);
    return 0;
  }

  if (challenge != NULL) {					/* CHAP */
    if (rad_put_string(h, RAD_CHAP_PASSWORD, key) != 0 ||
        rad_put_string(h, RAD_CHAP_CHALLENGE, challenge) != 0) {
      log_Printf(LogERROR, "CHAP: rad_put_string: %s\n", rad_strerror(h));
      rad_close(h);
      return 0;
    }
  } else if (rad_put_string(h, RAD_USER_PASSWORD, key) != 0) {	/* PAP */
    /* We're talking PAP */
    log_Printf(LogERROR, "PAP: rad_put_string: %s\n", rad_strerror(h));
    rad_close(h);
    return 0;
  }

  /*
   * Having to do this is bad news.  The right way is to grab the
   * descriptor that rad_send_request() selects on and add it to
   * our own selection list (making a full ``struct descriptor''),
   * then to ``continue'' the call when the descriptor is ready.
   * This requires altering libradius....
   */
  sigemptyset(&alrm);
  sigaddset(&alrm, SIGALRM);
  sigprocmask(SIG_BLOCK, &alrm, &prevset);
  got = rad_send_request(h);
  sigprocmask(SIG_SETMASK, &prevset, NULL);

  switch (got) {
    case RAD_ACCESS_ACCEPT:
      break;

    case RAD_ACCESS_CHALLENGE:
      /* we can't deal with this (for now) ! */
      log_Printf(LogPHASE, "Can't handle radius CHALLENGEs !\n");
      rad_close(h);
      return 0;

    case -1:
      log_Printf(LogPHASE, "radius: %s\n", rad_strerror(h));
      rad_close(h);
      return 0;

    default:
      log_Printf(LogERROR, "rad_send_request: Failed %d: %s\n",
                 got, rad_strerror(h));
      rad_close(h);
      return 0;

    case RAD_ACCESS_REJECT:
      log_Printf(LogPHASE, "radius: Rejected !\n");
      rad_close(h);
      return 0;
  }

  /* So we've been accepted !  Let's see what we've got in our reply :-I */
  r->ip.s_addr = r->mask.s_addr = INADDR_NONE;
  r->mtu = 0;
  r->vj = 0;
  while ((got = rad_get_attr(h, &data, &len)) > 0) {
    switch (got) {
      case RAD_FRAMED_IP_ADDRESS:
        r->ip = rad_cvt_addr(data);
        log_Printf(LogDEBUG, "radius: Got IP %s\n", inet_ntoa(r->ip));
        break;

      case RAD_FRAMED_IP_NETMASK:
        r->mask = rad_cvt_addr(data);
        log_Printf(LogDEBUG, "radius: Got MASK %s\n", inet_ntoa(r->mask));
        break;

      case RAD_FRAMED_MTU:
        r->mtu = rad_cvt_int(data);
        log_Printf(LogDEBUG, "radius: Got MTU %lu\n", r->mtu);
        break;

      case RAD_FRAMED_ROUTING:
        /* Disabled for now - should we automatically set up some filters ? */
        /* rad_cvt_int(data); */
        /* bit 1 = Send routing packets */
        /* bit 2 = Receive routing packets */
        break;

      case RAD_FRAMED_COMPRESSION:
        r->vj = rad_cvt_int(data) == 1 ? 1 : 0;
        log_Printf(LogDEBUG, "radius: Got VJ %sabled\n", r->vj ? "en" : "dis");
        break;

      case RAD_FRAMED_ROUTE:
        /*
         * We expect a string of the format ``dest[/bits] gw [metrics]''
         * Any specified metrics are ignored.  MYADDR and HISADDR are
         * understood for ``dest'' and ``gw'' and ``0.0.0.0'' is the same
         * as ``HISADDR''.
         */

        if ((nuke = rad_cvt_string(data, len)) == NULL) {
          log_Printf(LogERROR, "rad_cvt_string: %s\n", rad_strerror(h));
          rad_close(h);
          return 0;
        }

        dest.ipaddr.s_addr = dest.mask.s_addr = INADDR_ANY;
        dest.width = 0;
        argc = command_Interpret(nuke, strlen(nuke), argv);
        if (argc < 2)
          log_Printf(LogWARN, "radius: %s: Invalid route\n",
                     argc == 1 ? argv[0] : "\"\"");
        else if ((strcasecmp(argv[0], "default") != 0 &&
                  !ParseAddr(&bundle->ncp.ipcp, argv[0], &dest.ipaddr,
                             &dest.mask, &dest.width)) ||
                 !ParseAddr(&bundle->ncp.ipcp, argv[1], &gw, NULL, NULL))
          log_Printf(LogWARN, "radius: %s %s: Invalid route\n",
                     argv[0], argv[1]);
        else {
          if (dest.width == 32 && strchr(argv[0], '/') == NULL)
            /* No mask specified - use the natural mask */
            dest.mask.s_addr = addr2mask(dest.ipaddr.s_addr);
          addrs = 0;

          if (!strncasecmp(argv[0], "HISADDR", 7))
            addrs = ROUTE_DSTHISADDR;
          else if (!strncasecmp(argv[0], "MYADDR", 6))
            addrs = ROUTE_DSTMYADDR;

          if (gw.s_addr == INADDR_ANY) {
            addrs |= ROUTE_GWHISADDR;
            gw = bundle->ncp.ipcp.peer_ip;
          } else if (strcasecmp(argv[1], "HISADDR") == 0)
            addrs |= ROUTE_GWHISADDR;

          route_Add(&r->routes, addrs, dest.ipaddr, dest.mask, gw);
        }
        free(nuke);
        break;
    }
  }

  if (got == -1) {
    log_Printf(LogERROR, "rad_get_attr: %s\n", rad_strerror(h));
    rad_close(h);
    return 0;
  }

  log_Printf(LogPHASE, "radius: SUCCESS\n");

  rad_close(h);
  return r->valid = 1;
}

void
radius_Show(struct radius *r, struct prompt *p)
{
  prompt_Printf(p, " Radius config: %s", *r->cfg.file ? r->cfg.file : "none");
  if (r->valid) {
    prompt_Printf(p, "\n            IP: %s\n", inet_ntoa(r->ip));
    prompt_Printf(p, "       Netmask: %s\n", inet_ntoa(r->mask));
    prompt_Printf(p, "           MTU: %lu\n", r->mtu);
    prompt_Printf(p, "            VJ: %sabled\n", r->vj ? "en" : "dis");
    if (r->routes)
      route_ShowSticky(p, r->routes, "        Routes", 16);
  } else
    prompt_Printf(p, " (not authenticated)\n");
}

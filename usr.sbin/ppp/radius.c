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
 * $FreeBSD: src/usr.sbin/ppp/radius.c,v 1.11.2.1 2000/03/21 10:23:16 brian Exp $
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <net/route.h>

#ifdef LOCALRAD
#include "radlib.h"
#else
#include <radlib.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <ttyent.h>
#include <unistd.h>
#include <netdb.h>

#include "layer.h"
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
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "radius.h"
#include "auth.h"
#include "async.h"
#include "physical.h"
#include "chat.h"
#include "cbcp.h"
#include "chap.h"
#include "datalink.h"
#include "bundle.h"

/*
 * rad_continue_send_request() has given us `got' (non-zero).  Deal with it.
 */
static void
radius_Process(struct radius *r, int got)
{
  char *argv[MAXARGS], *nuke;
  struct bundle *bundle;
  int argc, addrs;
  size_t len;
  struct in_range dest;
  struct in_addr gw;
  const void *data;

  r->cx.fd = -1;		/* Stop select()ing */

  switch (got) {
    case RAD_ACCESS_ACCEPT:
      log_Printf(LogPHASE, "Radius: ACCEPT received\n");
      break;

    case RAD_ACCESS_REJECT:
      log_Printf(LogPHASE, "Radius: REJECT received\n");
      auth_Failure(r->cx.auth);
      rad_close(r->cx.rad);
      return;

    case RAD_ACCESS_CHALLENGE:
      /* we can't deal with this (for now) ! */
      log_Printf(LogPHASE, "Radius: CHALLENGE received (can't handle yet)\n");
      auth_Failure(r->cx.auth);
      rad_close(r->cx.rad);
      return;

    case -1:
      log_Printf(LogPHASE, "radius: %s\n", rad_strerror(r->cx.rad));
      auth_Failure(r->cx.auth);
      rad_close(r->cx.rad);
      return;

    default:
      log_Printf(LogERROR, "rad_send_request: Failed %d: %s\n",
                 got, rad_strerror(r->cx.rad));
      auth_Failure(r->cx.auth);
      rad_close(r->cx.rad);
      return;
  }

  /* So we've been accepted !  Let's see what we've got in our reply :-I */
  r->ip.s_addr = r->mask.s_addr = INADDR_NONE;
  r->mtu = 0;
  r->vj = 0;
  while ((got = rad_get_attr(r->cx.rad, &data, &len)) > 0) {
    switch (got) {
      case RAD_FRAMED_IP_ADDRESS:
        r->ip = rad_cvt_addr(data);
        log_Printf(LogPHASE, "        IP %s\n", inet_ntoa(r->ip));
        break;

      case RAD_FRAMED_IP_NETMASK:
        r->mask = rad_cvt_addr(data);
        log_Printf(LogPHASE, "        Netmask %s\n", inet_ntoa(r->mask));
        break;

      case RAD_FRAMED_MTU:
        r->mtu = rad_cvt_int(data);
        log_Printf(LogPHASE, "        MTU %lu\n", r->mtu);
        break;

      case RAD_FRAMED_ROUTING:
        /* Disabled for now - should we automatically set up some filters ? */
        /* rad_cvt_int(data); */
        /* bit 1 = Send routing packets */
        /* bit 2 = Receive routing packets */
        break;

      case RAD_FRAMED_COMPRESSION:
        r->vj = rad_cvt_int(data) == 1 ? 1 : 0;
        log_Printf(LogPHASE, "        VJ %sabled\n", r->vj ? "en" : "dis");
        break;

      case RAD_FRAMED_ROUTE:
        /*
         * We expect a string of the format ``dest[/bits] gw [metrics]''
         * Any specified metrics are ignored.  MYADDR and HISADDR are
         * understood for ``dest'' and ``gw'' and ``0.0.0.0'' is the same
         * as ``HISADDR''.
         */

        if ((nuke = rad_cvt_string(data, len)) == NULL) {
          log_Printf(LogERROR, "rad_cvt_string: %s\n", rad_strerror(r->cx.rad));
          rad_close(r->cx.rad);
          return;
        }

        log_Printf(LogPHASE, "        Route: %s\n", nuke);
        bundle = r->cx.auth->physical->dl->bundle;
        dest.ipaddr.s_addr = dest.mask.s_addr = INADDR_ANY;
        dest.width = 0;
        argc = command_Interpret(nuke, strlen(nuke), argv);
        if (argc < 0)
          log_Printf(LogWARN, "radius: %s: Syntax error\n",
                     argc == 1 ? argv[0] : "\"\"");
        else if (argc < 2)
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
            dest.mask = addr2mask(dest.ipaddr);
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
    log_Printf(LogERROR, "rad_get_attr: %s (failing!)\n",
               rad_strerror(r->cx.rad));
    auth_Failure(r->cx.auth);
    rad_close(r->cx.rad);
  } else {
    r->valid = 1;
    auth_Success(r->cx.auth);
    rad_close(r->cx.rad);
  }
}

/*
 * We've either timed out or select()ed on the read descriptor
 */
static void
radius_Continue(struct radius *r, int sel)
{
  struct timeval tv;
  int got;

  timer_Stop(&r->cx.timer);
  if ((got = rad_continue_send_request(r->cx.rad, sel, &r->cx.fd, &tv)) == 0) {
    log_Printf(LogPHASE, "Radius: Request re-sent\n");
    r->cx.timer.load = tv.tv_usec / TICKUNIT + tv.tv_sec * SECTICKS;
    timer_Start(&r->cx.timer);
    return;
  }

  radius_Process(r, got);
}

/*
 * Time to call rad_continue_send_request() - timed out.
 */
static void
radius_Timeout(void *v)
{
  radius_Continue((struct radius *)v, 0);
}

/*
 * Time to call rad_continue_send_request() - something to read.
 */
static void
radius_Read(struct fdescriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  radius_Continue(descriptor2radius(d), 1);
}

/*
 * Behave as a struct fdescriptor (descriptor.h)
 */
static int
radius_UpdateSet(struct fdescriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct radius *rad = descriptor2radius(d);

  if (r && rad->cx.fd != -1) {
    FD_SET(rad->cx.fd, r);
    if (*n < rad->cx.fd + 1)
      *n = rad->cx.fd + 1;
    log_Printf(LogTIMER, "Radius: fdset(r) %d\n", rad->cx.fd);
    return 1;
  }

  return 0;
}

/*
 * Behave as a struct fdescriptor (descriptor.h)
 */
static int
radius_IsSet(struct fdescriptor *d, const fd_set *fdset)
{
  struct radius *r = descriptor2radius(d);

  return r && r->cx.fd != -1 && FD_ISSET(r->cx.fd, fdset);
}

/*
 * Behave as a struct fdescriptor (descriptor.h)
 */
static int
radius_Write(struct fdescriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  /* We never want to write here ! */
  log_Printf(LogALERT, "radius_Write: Internal error: Bad call !\n");
  return 0;
}

/*
 * Initialise ourselves
 */
void
radius_Init(struct radius *r)
{
  r->valid = 0;
  r->cx.fd = -1;
  *r->cfg.file = '\0';;
  r->desc.type = RADIUS_DESCRIPTOR;
  r->desc.UpdateSet = radius_UpdateSet;
  r->desc.IsSet = radius_IsSet;
  r->desc.Read = radius_Read;
  r->desc.Write = radius_Write;
  memset(&r->cx.timer, '\0', sizeof r->cx.timer);
}

/*
 * Forget everything and go back to initialised state.
 */
void
radius_Destroy(struct radius *r)
{
  r->valid = 0;
  timer_Stop(&r->cx.timer);
  route_DeleteAll(&r->routes);
  if (r->cx.fd != -1) {
    r->cx.fd = -1;
    rad_close(r->cx.rad);
  }
}

/*
 * Start an authentication request to the RADIUS server.
 */
void
radius_Authenticate(struct radius *r, struct authinfo *authp, const char *name,
                    const char *key, const char *challenge)
{
  struct ttyent *ttyp;
  struct timeval tv;
  int got, slot;
  char hostname[MAXHOSTNAMELEN];
  struct hostent *hp;
  struct in_addr hostaddr;

  if (!*r->cfg.file)
    return;

  if (r->cx.fd != -1)
    /*
     * We assume that our name/key/challenge is the same as last time,
     * and just continue to wait for the RADIUS server(s).
     */
    return;

  radius_Destroy(r);

  if ((r->cx.rad = rad_open()) == NULL) {
    log_Printf(LogERROR, "rad_open: %s\n", strerror(errno));
    return;
  }

  if (rad_config(r->cx.rad, r->cfg.file) != 0) {
    log_Printf(LogERROR, "rad_config: %s\n", rad_strerror(r->cx.rad));
    rad_close(r->cx.rad);
    return;
  }

  if (rad_create_request(r->cx.rad, RAD_ACCESS_REQUEST) != 0) {
    log_Printf(LogERROR, "rad_create_request: %s\n", rad_strerror(r->cx.rad));
    rad_close(r->cx.rad);
    return;
  }

  if (rad_put_string(r->cx.rad, RAD_USER_NAME, name) != 0 ||
      rad_put_int(r->cx.rad, RAD_SERVICE_TYPE, RAD_FRAMED) != 0 ||
      rad_put_int(r->cx.rad, RAD_FRAMED_PROTOCOL, RAD_PPP) != 0) {
    log_Printf(LogERROR, "rad_put: %s\n", rad_strerror(r->cx.rad));
    rad_close(r->cx.rad);
    return;
  }

  if (challenge != NULL) {
    /* We're talking CHAP */
    if (rad_put_string(r->cx.rad, RAD_CHAP_PASSWORD, key) != 0 ||
        rad_put_string(r->cx.rad, RAD_CHAP_CHALLENGE, challenge) != 0) {
      log_Printf(LogERROR, "CHAP: rad_put_string: %s\n",
                 rad_strerror(r->cx.rad));
      rad_close(r->cx.rad);
      return;
    }
  } else if (rad_put_string(r->cx.rad, RAD_USER_PASSWORD, key) != 0) {
    /* We're talking PAP */
    log_Printf(LogERROR, "PAP: rad_put_string: %s\n", rad_strerror(r->cx.rad));
    rad_close(r->cx.rad);
    return;
  }

  if (gethostname(hostname, sizeof hostname) != 0)
    log_Printf(LogERROR, "rad_put: gethostname(): %s\n", strerror(errno));
  else {
    if ((hp = gethostbyname(hostname)) != NULL) {
      hostaddr.s_addr = *(u_long *)hp->h_addr;
      if (rad_put_addr(r->cx.rad, RAD_NAS_IP_ADDRESS, hostaddr) != 0) {
        log_Printf(LogERROR, "rad_put: rad_put_string: %s\n",
                   rad_strerror(r->cx.rad));
        rad_close(r->cx.rad);
        return;
      }
    }
    if (rad_put_string(r->cx.rad, RAD_NAS_IDENTIFIER, hostname) != 0) {
      log_Printf(LogERROR, "rad_put: rad_put_string: %s\n",
                 rad_strerror(r->cx.rad));
      rad_close(r->cx.rad);
      return;
    }
  }

  if (authp->physical->handler &&
      authp->physical->handler->type == TTY_DEVICE) {
    setttyent();
    for (slot = 1; (ttyp = getttyent()); ++slot)
      if (!strcmp(ttyp->ty_name, authp->physical->name.base)) {
        if(rad_put_int(r->cx.rad, RAD_NAS_PORT, slot) != 0) {
          log_Printf(LogERROR, "rad_put: rad_put_string: %s\n",
                      rad_strerror(r->cx.rad));
          rad_close(r->cx.rad);
          endttyent();
          return;
        }
        break;
      }
    endttyent();
  }


  if ((got = rad_init_send_request(r->cx.rad, &r->cx.fd, &tv)))
    radius_Process(r, got);
  else {
    log_Printf(LogPHASE, "Radius: Request sent\n");
    log_Printf(LogDEBUG, "Using radius_Timeout [%p]\n", radius_Timeout);
    r->cx.timer.load = tv.tv_usec / TICKUNIT + tv.tv_sec * SECTICKS;
    r->cx.timer.func = radius_Timeout;
    r->cx.timer.name = "radius";
    r->cx.timer.arg = r;
    r->cx.auth = authp;
    timer_Start(&r->cx.timer);
  }
}

/*
 * How do things look at the moment ?
 */
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

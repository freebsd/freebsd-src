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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netgraph.h>
#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netgraph/ng_ether.h>
#include <netgraph/ng_message.h>
#include <netgraph/ng_pppoe.h>
#include <netgraph/ng_socket.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/fcntl.h>
#if defined(__FreeBSD__) && !defined(NOKLDLOAD)
#include <sys/linker.h>
#endif
#include <sys/uio.h>
#include <termios.h>
#ifndef NONBLOCK_FIXED
#include <sys/time.h>
#endif
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
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
#include "main.h"
#include "mp.h"
#include "chat.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "slcompress.h"
#include "iplist.h"
#include "ipcp.h"
#include "filter.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "id.h"
#include "ether.h"


#define PPPOE_NODE_TYPE_LEN (sizeof NG_PPPOE_NODE_TYPE - 1) /* "PPPoE" */

struct etherdevice {
  struct device dev;			/* What struct physical knows about */
  int cs;				/* Control socket */
  int connected;			/* Are we connected yet ? */
  int timeout;				/* Seconds attempting to connect */
  char hook[sizeof TUN_NAME + 11];	/* Our socket node hook */
};

#define device2ether(d) \
  ((d)->type == ETHER_DEVICE ? (struct etherdevice *)d : NULL)

int
ether_DeviceSize(void)
{
  return sizeof(struct etherdevice);
}

static ssize_t
ether_Write(struct physical *p, const void *v, size_t n)
{
  struct etherdevice *dev = device2ether(p->handler);

  return NgSendData(p->fd, dev->hook, v, n) == -1 ? -1 : n;
}

static ssize_t
ether_Read(struct physical *p, void *v, size_t n)
{
  char hook[sizeof TUN_NAME + 11];

  return NgRecvData(p->fd, v, n, hook);
}

static int
ether_RemoveFromSet(struct physical *p, fd_set *r, fd_set *w, fd_set *e)
{
  struct etherdevice *dev = device2ether(p->handler);
  int result;

  if (r && dev->cs >= 0 && FD_ISSET(dev->cs, r)) {
    FD_CLR(dev->cs, r);
    log_Printf(LogTIMER, "%s: fdunset(ctrl) %d\n", p->link.name, dev->cs);
    result = 1;
  } else
    result = 0;

  /* Careful... physical_RemoveFromSet() called us ! */

  p->handler->removefromset = NULL;
  result += physical_RemoveFromSet(p, r, w, e);
  p->handler->removefromset = ether_RemoveFromSet;

  return result;
}

static void
ether_Free(struct physical *p)
{
  struct etherdevice *dev = device2ether(p->handler);

  physical_SetDescriptor(p);
  if (dev->cs != -1)
    close(dev->cs);
  free(dev);
}

static const char *
ether_OpenInfo(struct physical *p)
{
  struct etherdevice *dev = device2ether(p->handler);

  switch (dev->connected) {
    case CARRIER_PENDING:
      return "negotiating";
    case CARRIER_OK:
      return "established";
  }

  return "disconnected";
}

static void
ether_device2iov(struct device *d, struct iovec *iov, int *niov,
                 int maxiov, int *auxfd, int *nauxfd, pid_t newpid)
{
  struct etherdevice *dev = device2ether(d);
  int sz = physical_MaxDeviceSize();

  iov[*niov].iov_base = realloc(d, sz);
  if (iov[*niov].iov_base == NULL) {
    log_Printf(LogALERT, "Failed to allocate memory: %d\n", sz);
    AbortProgram(EX_OSERR);
  }
  iov[*niov].iov_len = sz;
  (*niov)++;

  if (dev->cs >= 0) {
    *auxfd = dev->cs;
    (*nauxfd)++;
  }
}

static void
ether_MessageIn(struct etherdevice *dev)
{
  char msgbuf[sizeof(struct ng_mesg) + sizeof(struct ngpppoe_sts)];
  struct ng_mesg *rep = (struct ng_mesg *)msgbuf;
  struct ngpppoe_sts *sts = (struct ngpppoe_sts *)(msgbuf + sizeof *rep);
  char unknown[14];
  const char *msg;
#ifndef NONBLOCK_FIXED
  struct timeval t;
  fd_set r;
#endif

  if (dev->cs < 0)
    return;

#ifndef NONBLOCK_FIXED
  FD_ZERO(&r);
  FD_SET(dev->cs, &r);
  t.tv_sec = t.tv_usec = 0;
  if (select(dev->cs + 1, &r, NULL, NULL, &t) <= 0)
    return;
#endif

  if (NgRecvMsg(dev->cs, rep, sizeof msgbuf, NULL) < 0)
    return;

  if (rep->header.version != NG_VERSION) {
    log_Printf(LogWARN, "%ld: Unexpected netgraph version, expected %ld\n",
               (long)rep->header.version, (long)NG_VERSION);
    return;
  }

  if (rep->header.typecookie != NGM_PPPOE_COOKIE) {
    log_Printf(LogWARN, "%ld: Unexpected netgraph cookie, expected %ld\n",
               (long)rep->header.typecookie, (long)NGM_PPPOE_COOKIE);
    return;
  }

  switch (rep->header.cmd) {
    case NGM_PPPOE_SET_FLAG:	msg = "SET_FLAG";	break;
    case NGM_PPPOE_CONNECT:	msg = "CONNECT";	break;
    case NGM_PPPOE_LISTEN:	msg = "LISTEN";		break;
    case NGM_PPPOE_OFFER:	msg = "OFFER";		break;
    case NGM_PPPOE_SUCCESS:	msg = "SUCCESS";	break;
    case NGM_PPPOE_FAIL:	msg = "FAIL";		break;
    case NGM_PPPOE_CLOSE:	msg = "CLOSE";		break;
    case NGM_PPPOE_GET_STATUS:	msg = "GET_STATUS";	break;
    default:
      snprintf(unknown, sizeof unknown, "<%d>", (int)rep->header.cmd);
      msg = unknown;
      break;
  }

  log_Printf(LogPHASE, "Received NGM_PPPOE_%s (hook \"%s\")\n", msg, sts->hook);

  switch (rep->header.cmd) {
    case NGM_PPPOE_SUCCESS:
      dev->connected = CARRIER_OK;
      break;
    case NGM_PPPOE_FAIL:
    case NGM_PPPOE_CLOSE:
      dev->connected = CARRIER_LOST;
      break;
  }
}

static int
ether_AwaitCarrier(struct physical *p)
{
  struct etherdevice *dev = device2ether(p->handler);

  if (!dev->timeout--)
    dev->connected = CARRIER_LOST;
  else if (dev->connected == CARRIER_PENDING)
    ether_MessageIn(dev);

  return dev->connected;
}

static const struct device baseetherdevice = {
  ETHER_DEVICE,
  "ether",
  ether_AwaitCarrier,
  ether_RemoveFromSet,
  NULL,
  NULL,
  NULL,
  NULL,
  ether_Free,
  ether_Read,
  ether_Write,
  ether_device2iov,
  NULL,
  ether_OpenInfo
};

struct device *
ether_iov2device(int type, struct physical *p, struct iovec *iov, int *niov,
                 int maxiov, int *auxfd, int *nauxfd)
{
  if (type == ETHER_DEVICE) {
    struct etherdevice *dev = (struct etherdevice *)iov[(*niov)++].iov_base;

    dev = realloc(dev, sizeof *dev);	/* Reduce to the correct size */
    if (dev == NULL) {
      log_Printf(LogALERT, "Failed to allocate memory: %d\n",
                 (int)(sizeof *dev));
      AbortProgram(EX_OSERR);
    }

    if (*nauxfd) {
      dev->cs = *auxfd;
      (*nauxfd)--;
    } else
      dev->cs = -1;

    /* Refresh function pointers etc */
    memcpy(&dev->dev, &baseetherdevice, sizeof dev->dev);

    physical_SetupStack(p, dev->dev.name, PHYSICAL_FORCE_SYNCNOACF);
    return &dev->dev;
  }

  return NULL;
}

static int
ether_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct physical *p = descriptor2physical(d);
  struct etherdevice *dev = device2ether(p->handler);
  int result;

  if (r && dev->cs >= 0) {
    FD_SET(dev->cs, r);
    log_Printf(LogTIMER, "%s(ctrl): fdset(r) %d\n", p->link.name, dev->cs);
    result = 1;
  } else
    result = 0;

  result += physical_doUpdateSet(d, r, w, e, n, 0);

  return result;
}

static int
ether_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  struct etherdevice *dev = device2ether(p->handler);
  int result;

  result = dev->cs >= 0 && FD_ISSET(dev->cs, fdset);
  result += physical_IsSet(d, fdset);

  return result;
}

static void
ether_DescriptorRead(struct descriptor *d, struct bundle *bundle,
                     const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  struct etherdevice *dev = device2ether(p->handler);

  if (dev->cs >= 0 && FD_ISSET(dev->cs, fdset)) {
    ether_MessageIn(dev);
    if (dev->connected == CARRIER_LOST) {
      log_Printf(LogPHASE, "%s: Device disconnected\n", p->link.name);
      datalink_Down(p->dl, CLOSE_NORMAL);
      return;
    }
  }

  if (physical_IsSet(d, fdset))
    physical_DescriptorRead(d, bundle, fdset);
}

static struct device *
ether_Abandon(struct etherdevice *dev, struct physical *p)
{
  /* Abandon our node construction */
  close(dev->cs);
  close(p->fd);
  p->fd = -2;	/* Nobody else need try.. */
  free(dev);

  return NULL;
}

struct device *
ether_Create(struct physical *p)
{
  u_char rbuf[2048];
  struct etherdevice *dev;
  struct ng_mesg *resp;
  const struct hooklist *hlist;
  const struct nodeinfo *ninfo;
  int f;

  dev = NULL;
  if (p->fd < 0 && !strncasecmp(p->name.full, NG_PPPOE_NODE_TYPE,
                                PPPOE_NODE_TYPE_LEN) &&
      p->name.full[PPPOE_NODE_TYPE_LEN] == ':') {
    const struct linkinfo *nlink;
    struct ngpppoe_init_data *data;
    struct ngm_mkpeer mkp;
    struct ngm_connect ngc;
    const char *iface, *provider;
    char *path, etherid[12];
    int ifacelen, providerlen, oldflag;
    char connectpath[sizeof dev->hook + 2];	/* .:<hook> */

#ifdef KLDSYM_LOOKUP
    /* First make sure we've got the right code loaded */
    char basesym[] = "ng_make_node", socksym[] = "ngdomain";
    struct kld_sym_lookup baselookup = { sizeof baselookup, basesym, 0, 0 };
    struct kld_sym_lookup socklookup = { sizeof socklookup, socksym, 0, 0 };
#endif

    p->fd--;				/* We own the device - change fd */

#ifdef KLDSYM_LOOKUP
    if (kldsym(0, KLDSYM_LOOKUP, &baselookup) == -1) {
      log_Printf(LogWARN, "Can't run without options NETGRAPH in the kernel\n");
      return NULL;
    }

    if (kldsym(0, KLDSYM_LOOKUP, &socklookup) == -1 &&
        ID0kldload("ng_socket") == -1) {
      log_Printf(LogWARN, "kldload: ng_socket: %s\n", strerror(errno));
      return NULL;
    }
#endif

    if ((dev = malloc(sizeof *dev)) == NULL)
      return NULL;

    iface = p->name.full + PPPOE_NODE_TYPE_LEN + 1;

    provider = strchr(iface, ':');
    if (provider) {
      ifacelen = provider - iface;
      provider++;
      providerlen = strlen(provider);
    } else {
      ifacelen = strlen(iface);
      provider = "";
      providerlen = 0;
    }

    /*
     * We're going to do this (where tunN is our tunnel device):
     *
     * .---------.
     * |  ether  |
     * | <iface> |                         dev->cs
     * `---------'                           |
     *  (orphan)                     p->fd   |
     *     |                           |     |
     *     |                           |     |
     * (ethernet)                      |     |
     * .---------.                  .-----------.
     * |  pppoe  |                  |  socket   |
     * | <iface> |(tunN)<---->(tunN)| <unnamed> |
     * `---------                   `-----------'
     *   (tunX)
     *     ^
     *     |
     *     `--->(tunX)
     */

    /* Create a socket node */
    if (NgMkSockNode(NULL, &dev->cs, &p->fd) == -1) {
      log_Printf(LogWARN, "Cannot create netgraph socket node: %s\n",
                 strerror(errno));
      free(dev);
      return NULL;
    }

    /*
     * Ask for a list of hooks attached to the "ether" node.  This node should
     * magically exist as a way of hooking stuff onto an ethernet device
     */
    path = (char *)alloca(ifacelen + 2);
    sprintf(path, "%.*s:", ifacelen, iface);
    if (NgSendMsg(dev->cs, path, NGM_GENERIC_COOKIE, NGM_LISTHOOKS,
                  NULL, 0) < 0) {
      log_Printf(LogWARN, "%s Cannot send a netgraph message: %s\n",
                 path, strerror(errno));
      return ether_Abandon(dev, p);
    }

    /* Get our list back */
    resp = (struct ng_mesg *)rbuf;
    if (NgRecvMsg(dev->cs, resp, sizeof rbuf, NULL) < 0) {
      log_Printf(LogWARN, "Cannot get netgraph response: %s\n",
                 strerror(errno));
      return ether_Abandon(dev, p);
    }

    hlist = (const struct hooklist *)resp->data;
    ninfo = &hlist->nodeinfo;

    /* Make sure we've got the right type of node */
    if (strncmp(ninfo->type, NG_ETHER_NODE_TYPE,
                sizeof NG_ETHER_NODE_TYPE - 1)) {
      log_Printf(LogWARN, "%s Unexpected node type ``%s'' (wanted ``"
                 NG_ETHER_NODE_TYPE "'')\n", path, ninfo->type);
      return ether_Abandon(dev, p);
    }

    log_Printf(LogDEBUG, "List of netgraph node ``%s'' (id %08x) hooks:\n",
               path, ninfo->id);

    /* look for a hook already attached.  */
    for (f = 0; f < ninfo->hooks; f++) {
      nlink = &hlist->link[f];

      log_Printf(LogDEBUG, "  Found %s -> %s\n", nlink->ourhook,
                 nlink->peerhook);

      if (!strcmp(nlink->ourhook, NG_ETHER_HOOK_ORPHAN) ||
          !strcmp(nlink->ourhook, NG_ETHER_HOOK_DIVERT)) {
        /*
         * Something is using the data coming out of this ``ether'' node.
         * If it's a PPPoE node, we use that node, otherwise we complain that
         * someone else is using the node.
         */
        if (!strcmp(nlink->nodeinfo.type, NG_PPPOE_NODE_TYPE))
          /* Use this PPPoE node ! */
          snprintf(ngc.path, sizeof ngc.path, "[%08x]:", nlink->nodeinfo.id);
        else {
          log_Printf(LogWARN, "%s Node type ``%s'' is currently active\n",
                     path, nlink->nodeinfo.type);
          return ether_Abandon(dev, p);
        }
        break;
      }
    }

    if (f == ninfo->hooks) {
      /*
       * Create a new ``PPPoE'' node connected to the ``ether'' node using
       * the magic ``orphan'' and ``ethernet'' hooks
       */
      snprintf(mkp.type, sizeof mkp.type, "%s", NG_PPPOE_NODE_TYPE);
      snprintf(mkp.ourhook, sizeof mkp.ourhook, "%s", NG_ETHER_HOOK_ORPHAN);
      snprintf(mkp.peerhook, sizeof mkp.peerhook, "%s", NG_PPPOE_HOOK_ETHERNET);
      snprintf(etherid, sizeof etherid, "[%08x]:", ninfo->id);

      log_Printf(LogDEBUG, "Creating PPPoE netgraph node %s%s -> %s\n",
                 etherid, mkp.ourhook, mkp.peerhook);

      if (NgSendMsg(dev->cs, etherid, NGM_GENERIC_COOKIE,
                    NGM_MKPEER, &mkp, sizeof mkp) < 0) {
        log_Printf(LogWARN, "%s Cannot create PPPoE netgraph node: %s\n",
                   etherid, strerror(errno));
        return ether_Abandon(dev, p);
      }

      snprintf(ngc.path, sizeof ngc.path, "%s%s", path, NG_ETHER_HOOK_ORPHAN);
    }

    snprintf(dev->hook, sizeof dev->hook, "%s%d",
             TUN_NAME, p->dl->bundle->unit);

    /*
     * Connect the PPPoE node to our socket node.
     * ngc.path has already been set up
     */
    snprintf(ngc.ourhook, sizeof ngc.ourhook, "%s", dev->hook);
    memcpy(ngc.peerhook, ngc.ourhook, sizeof ngc.peerhook);

    log_Printf(LogDEBUG, "Connecting netgraph socket .:%s -> %s:%s\n",
               ngc.ourhook, ngc.path, ngc.peerhook);
    if (NgSendMsg(dev->cs, ".:", NGM_GENERIC_COOKIE,
                  NGM_CONNECT, &ngc, sizeof ngc) < 0) {
      log_Printf(LogWARN, "Cannot connect PPPoE and socket netgraph "
                 "nodes: %s\n", strerror(errno));
      return ether_Abandon(dev, p);
    }

    /* And finally, request a connection to the given provider */

    data = (struct ngpppoe_init_data *)alloca(sizeof *data + providerlen + 1);

    snprintf(data->hook, sizeof data->hook, "%s", dev->hook);
    strcpy(data->data, provider);
    data->data_len = providerlen;

    snprintf(connectpath, sizeof connectpath, ".:%s", dev->hook);
    log_Printf(LogDEBUG, "Sending PPPOE_CONNECT to %s\n", connectpath);
    if (NgSendMsg(dev->cs, connectpath, NGM_PPPOE_COOKIE,
                  NGM_PPPOE_CONNECT, data, sizeof *data + providerlen) == -1) {
      log_Printf(LogWARN, "``%s'': Cannot start netgraph node: %s\n",
                 connectpath, strerror(errno));
      return ether_Abandon(dev, p);
    }

    /*
     * Now make our control socket non-blocking so that we can read()
     * without having to select()
     *
     * XXX: Does this work (#define NONBLOCK_FIXED) ?
     */
    oldflag = fcntl(dev->cs, F_GETFL, 0);
    if (oldflag < 0) {
      log_Printf(LogWARN, "%s: Open: Cannot get physical flags: %s\n",
                 p->link.name, strerror(errno));
      return ether_Abandon(dev, p);
    } else
      fcntl(dev->cs, F_SETFL, oldflag & ~O_NONBLOCK);

    dev->timeout = p->cfg.cd.delay;
    dev->connected = CARRIER_PENDING;
  } else {
    /* See if we're a netgraph socket */
    struct sockaddr_ng ngsock;
    struct sockaddr *sock = (struct sockaddr *)&ngsock;
    int sz;

    sz = sizeof ngsock;
    if (getsockname(p->fd, sock, &sz) != -1 && sock->sa_family == AF_NETGRAPH) {
      /*
       * It's a netgraph node... determine the hook name and set things up
       */

      if (NgSendMsg(dev->cs, ".", NGM_GENERIC_COOKIE, NGM_LISTHOOKS,
                    NULL, 0) < 0) {
        log_Printf(LogWARN, "Cannot send a netgraph message to stdin: %s\n",
                   strerror(errno));
        close(p->fd);
        p->fd = -1;
        return NULL;
      }

      /* Get our list back */
      resp = (struct ng_mesg *)rbuf;
      if (NgRecvMsg(dev->cs, resp, sizeof rbuf, NULL) < 0) {
        log_Printf(LogWARN, "Cannot get netgraph response: %s\n",
                   strerror(errno));
        close(p->fd);
        p->fd = -1;
        return NULL;
      }

      hlist = (const struct hooklist *)resp->data;
      ninfo = &hlist->nodeinfo;

      /*
       * Make sure we've got the right type of node...
       * Can it be anything else ?
       */
      if (strncmp(ninfo->type, NG_SOCKET_NODE_TYPE,
                  sizeof NG_SOCKET_NODE_TYPE - 1)) {
        log_Printf(LogWARN, "Unexpected netgraph node type ``%s'' (wanted ``"
                   NG_SOCKET_NODE_TYPE "'')\n", ninfo->type);
        close(p->fd);
        p->fd = -1;
        return NULL;
      }

      if (ninfo->hooks != 1) {
        log_Printf(LogWARN, "Can't handle netgraph node with %d hooks\n",
                   ninfo->hooks);
        close(p->fd);
        p->fd = -1;
        return NULL;
      }

      /* Looks good.... lets allocate a device structure */
      if ((dev = malloc(sizeof *dev)) == NULL) {
        log_Printf(LogWARN, "%s: Cannot allocate an ether device: %s\n",
                   p->link.name, strerror(errno));
        return NULL;
      }

      dev->cs = -1;
      dev->timeout = 0;
      dev->connected = CARRIER_OK;
      strncpy(dev->hook, hlist->link->ourhook, sizeof dev->hook - 1);
      dev->hook[sizeof dev->hook - 1] = '\0';

      log_Printf(LogDEBUG, "Using netgraph hook ``.:%s'' -> [%08x]:%s\n",
                 dev->hook, hlist->link->nodeinfo.id, hlist->link->peerhook);
    }
  }

  if (dev) {
    memcpy(&dev->dev, &baseetherdevice, sizeof dev->dev);

    /* Hook things up so that we monitor dev->cs */
    p->desc.UpdateSet = ether_UpdateSet;
    p->desc.IsSet = ether_IsSet;
    p->desc.Read = ether_DescriptorRead;

    physical_SetupStack(p, dev->dev.name, PHYSICAL_FORCE_SYNCNOACF);

    /* Moan about (and fix) invalid LCP configurations */
    if (p->link.lcp.cfg.mru > 1492) {
      log_Printf(LogWARN, "%s: Reducing MRU to 1492\n", p->link.name);
      p->link.lcp.cfg.mru = 1492;
    }
    if (p->dl->bundle->cfg.mtu > 1492) {
      log_Printf(LogWARN, "%s: Reducing MTU to 1492\n", p->link.name);
      p->dl->bundle->cfg.mtu = 1492;
    }

    return &dev->dev;
  }

  return NULL;
}

/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: bundle.c,v 1.5 1998/05/23 22:27:53 brian Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if_tun.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "route.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "bundle.h"
#include "async.h"
#include "physical.h"
#include "modem.h"
#include "loadalias.h"
#include "auth.h"
#include "lcpproto.h"
#include "chap.h"
#include "tun.h"
#include "prompt.h"
#include "chat.h"
#include "datalink.h"
#include "ip.h"

#define SCATTER_SEGMENTS 4	/* version, datalink, name, physical */
#define SOCKET_OVERHEAD	100	/* additional buffer space for large */
                                /* {recv,send}msg() calls            */

static int bundle_RemainingIdleTime(struct bundle *);
static int bundle_RemainingAutoLoadTime(struct bundle *);

static const char *PhaseNames[] = {
  "Dead", "Establish", "Authenticate", "Network", "Terminate"
};

const char *
bundle_PhaseName(struct bundle *bundle)
{
  return bundle->phase <= PHASE_TERMINATE ?
    PhaseNames[bundle->phase] : "unknown";
}

void
bundle_NewPhase(struct bundle *bundle, u_int new)
{
  if (new == bundle->phase)
    return;

  if (new <= PHASE_TERMINATE)
    log_Printf(LogPHASE, "bundle: %s\n", PhaseNames[new]);

  switch (new) {
  case PHASE_DEAD:
    log_DisplayPrompts();
    bundle->phase = new;
    break;

  case PHASE_ESTABLISH:
    bundle->phase = new;
    break;

  case PHASE_AUTHENTICATE:
    bundle->phase = new;
    log_DisplayPrompts();
    break;

  case PHASE_NETWORK:
    ipcp_Setup(&bundle->ncp.ipcp);
    fsm_Up(&bundle->ncp.ipcp.fsm);
    fsm_Open(&bundle->ncp.ipcp.fsm);
    bundle->phase = new;
    log_DisplayPrompts();
    break;

  case PHASE_TERMINATE:
    bundle->phase = new;
    mp_Down(&bundle->ncp.mp);
    log_DisplayPrompts();
    break;
  }
}

static int
bundle_CleanInterface(const struct bundle *bundle)
{
  int s;
  struct ifreq ifrq;
  struct ifaliasreq ifra;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "bundle_CleanInterface: socket(): %s\n",
              strerror(errno));
    return (-1);
  }
  strncpy(ifrq.ifr_name, bundle->ifp.Name, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  while (ID0ioctl(s, SIOCGIFADDR, &ifrq) == 0) {
    memset(&ifra.ifra_mask, '\0', sizeof ifra.ifra_mask);
    strncpy(ifra.ifra_name, bundle->ifp.Name, sizeof ifra.ifra_name - 1);
    ifra.ifra_name[sizeof ifra.ifra_name - 1] = '\0';
    ifra.ifra_addr = ifrq.ifr_addr;
    if (ID0ioctl(s, SIOCGIFDSTADDR, &ifrq) < 0) {
      if (ifra.ifra_addr.sa_family == AF_INET)
        log_Printf(LogERROR,
                  "bundle_CleanInterface: Can't get dst for %s on %s !\n",
                  inet_ntoa(((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr),
                  bundle->ifp.Name);
      close(s);
      return 0;
    }
    ifra.ifra_broadaddr = ifrq.ifr_dstaddr;
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      if (ifra.ifra_addr.sa_family == AF_INET)
        log_Printf(LogERROR,
                  "bundle_CleanInterface: Can't delete %s address on %s !\n",
                  inet_ntoa(((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr),
                  bundle->ifp.Name);
      close(s);
      return 0;
    }
  }
  close(s);

  return 1;
}

static void
bundle_LayerStart(void *v, struct fsm *fp)
{
  /* The given FSM is about to start up ! */
}


static void
bundle_Notify(struct bundle *bundle, char c)
{
  if (bundle->notify.fd != -1) {
    if (write(bundle->notify.fd, &c, 1) == 1)
      log_Printf(LogPHASE, "Parent notified of success.\n");
    else
      log_Printf(LogPHASE, "Failed to notify parent of success.\n");
    close(bundle->notify.fd);
    bundle->notify.fd = -1;
  }
}

static void
bundle_AutoLoadTimeout(void *v)
{
  struct bundle *bundle = (struct bundle *)v;

  if (bundle->autoload.comingup) {
    log_Printf(LogPHASE, "autoload: Another link is required\n");
    /* bundle_Open() stops the timer */
    bundle_Open(bundle, NULL, PHYS_DEMAND);
  } else {
    struct datalink *dl, *last;

    timer_Stop(&bundle->autoload.timer);
    for (last = NULL, dl = bundle->links; dl; dl = dl->next)
      if (dl->physical->type == PHYS_DEMAND && dl->state == DATALINK_OPEN)
        last = dl;

    if (last)
      datalink_Close(last, 1);
  }
}

static void
bundle_StartAutoLoadTimer(struct bundle *bundle, int up)
{
  struct datalink *dl;

  timer_Stop(&bundle->autoload.timer);

  if (bundle->CleaningUp || bundle->phase != PHASE_NETWORK) {
    dl = NULL;
    bundle->autoload.running = 0;
  } else if (up) {
    for (dl = bundle->links; dl; dl = dl->next)
      if (dl->state == DATALINK_CLOSED && dl->physical->type == PHYS_DEMAND) {
        if (bundle->cfg.autoload.max.timeout) {
          bundle->autoload.timer.func = bundle_AutoLoadTimeout;
          bundle->autoload.timer.name = "autoload up";
          bundle->autoload.timer.load =
            bundle->cfg.autoload.max.timeout * SECTICKS;
          bundle->autoload.timer.arg = bundle;
          timer_Start(&bundle->autoload.timer);
          bundle->autoload.done = time(NULL) + bundle->cfg.autoload.max.timeout;
        } else
          bundle_AutoLoadTimeout(bundle);
        break;
      }
    bundle->autoload.running = (dl || bundle->cfg.autoload.min.timeout) ? 1 : 0;
  } else {
    int nlinks;
    struct datalink *adl;

    for (nlinks = 0, adl = NULL, dl = bundle->links; dl; dl = dl->next)
      if (dl->state == DATALINK_OPEN) {
        if (dl->physical->type == PHYS_DEMAND)
          adl = dl;
        if (++nlinks > 1 && adl) {
          if (bundle->cfg.autoload.min.timeout) {
            bundle->autoload.timer.func = bundle_AutoLoadTimeout;
            bundle->autoload.timer.name = "autoload down";
            bundle->autoload.timer.load =
              bundle->cfg.autoload.min.timeout * SECTICKS;
            bundle->autoload.timer.arg = bundle;
            timer_Start(&bundle->autoload.timer);
            bundle->autoload.done =
              time(NULL) + bundle->cfg.autoload.min.timeout;
          }
          break;
        }
      }

    bundle->autoload.running = 1;
  }

  bundle->autoload.comingup = up ? 1 : 0;
}

static void
bundle_StopAutoLoadTimer(struct bundle *bundle)
{
  timer_Stop(&bundle->autoload.timer);
  bundle->autoload.done = 0;
}

static int
bundle_RemainingAutoLoadTime(struct bundle *bundle)
{
  if (bundle->autoload.done)
    return bundle->autoload.done - time(NULL);
  return -1;
}


static void
bundle_LayerUp(void *v, struct fsm *fp)
{
  /*
   * The given fsm is now up
   * If it's an LCP set our mtu (if we're multilink, add up the link
   * speeds and set the MRRU) and start our autoload timer.
   * If it's an NCP, tell our -background parent to go away.
   * If it's the first NCP, start the idle timer.
   */
  struct bundle *bundle = (struct bundle *)v;

  if (fp->proto == PROTO_LCP) {
    if (bundle->ncp.mp.active) {
      struct datalink *dl;

      bundle->ifp.Speed = 0;
      for (dl = bundle->links; dl; dl = dl->next)
        if (dl->state == DATALINK_OPEN)
          bundle->ifp.Speed += modem_Speed(dl->physical);
      tun_configure(bundle, bundle->ncp.mp.peer_mrru);
      bundle->autoload.running = 1;
    } else {
      bundle->ifp.Speed = modem_Speed(link2physical(fp->link));
      tun_configure(bundle, fsm2lcp(fp)->his_mru);
    }
  } else if (fp->proto == PROTO_IPCP) {
    bundle_StartIdleTimer(bundle);
    bundle_Notify(bundle, EX_NORMAL);
  }
}

static void
bundle_LayerDown(void *v, struct fsm *fp)
{
  /*
   * The given FSM has been told to come down.
   * If it's our last NCP, stop the idle timer.
   * If it's an LCP and we're in multilink mode, adjust our tun
   * speed and make sure our minimum sequence number is adjusted.
   */

  struct bundle *bundle = (struct bundle *)v;

  if (fp->proto == PROTO_IPCP)
    bundle_StopIdleTimer(bundle);
  else if (fp->proto == PROTO_LCP && bundle->ncp.mp.active) {
    struct datalink *dl;
    struct datalink *lost;

    bundle->ifp.Speed = 0;
    lost = NULL;
    for (dl = bundle->links; dl; dl = dl->next)
      if (fp == &dl->physical->link.lcp.fsm)
        lost = dl;
      else if (dl->state == DATALINK_OPEN)
        bundle->ifp.Speed += modem_Speed(dl->physical);

    if (bundle->ifp.Speed)
      /* Don't configure down to a speed of 0 */
      tun_configure(bundle, bundle->ncp.mp.link.lcp.his_mru);

    if (lost)
      mp_LinkLost(&bundle->ncp.mp, lost);
    else
      log_Printf(LogERROR, "Oops, lost an unrecognised datalink (%s) !\n",
                 fp->link->name);
  }
}

static void
bundle_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm is now down (fp cannot be NULL)
   *
   * If it's the last LCP, fsm_Down all NCPs
   * If it's the last NCP, fsm_Close all LCPs
   */

  struct bundle *bundle = (struct bundle *)v;
  struct datalink *dl;

  if (fp->proto == PROTO_IPCP) {
    if (bundle_Phase(bundle) != PHASE_DEAD)
      bundle_NewPhase(bundle, PHASE_TERMINATE);
    for (dl = bundle->links; dl; dl = dl->next)
      datalink_Close(dl, 0);
    fsm_Down(fp);
    fsm_Close(fp);
  } else if (fp->proto == PROTO_LCP) {
    int others_active;

    others_active = 0;
    for (dl = bundle->links; dl; dl = dl->next)
      if (fp != &dl->physical->link.lcp.fsm &&
          dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP)
        others_active++;

    if (!others_active) {
      fsm_Down(&bundle->ncp.ipcp.fsm);
      fsm_Close(&bundle->ncp.ipcp.fsm);		/* ST_INITIAL please */
    }
  }
}

int
bundle_LinkIsUp(const struct bundle *bundle)
{
  return bundle->ncp.ipcp.fsm.state == ST_OPENED;
}

void
bundle_Close(struct bundle *bundle, const char *name, int staydown)
{
  /*
   * Please close the given datalink.
   * If name == NULL or name is the last datalink, fsm_Close all NCPs
   * (except our MP)
   * If it isn't the last datalink, just Close that datalink.
   */

  struct datalink *dl, *this_dl;
  int others_active;

  if (bundle->phase == PHASE_TERMINATE || bundle->phase == PHASE_DEAD)
    return;

  others_active = 0;
  this_dl = NULL;

  for (dl = bundle->links; dl; dl = dl->next) {
    if (name && !strcasecmp(name, dl->name))
      this_dl = dl;
    if (name == NULL || this_dl == dl) {
      if (staydown)
        datalink_StayDown(dl);
    } else if (dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP)
      others_active++;
  }

  if (name && this_dl == NULL) {
    log_Printf(LogWARN, "%s: Invalid datalink name\n", name);
    return;
  }

  if (!others_active) {
    bundle_StopIdleTimer(bundle);
    bundle_StopAutoLoadTimer(bundle);
    if (bundle->ncp.ipcp.fsm.state > ST_CLOSED ||
        bundle->ncp.ipcp.fsm.state == ST_STARTING)
      fsm_Close(&bundle->ncp.ipcp.fsm);
    else {
      if (bundle->ncp.ipcp.fsm.state > ST_INITIAL) {
        fsm_Close(&bundle->ncp.ipcp.fsm);
        fsm_Down(&bundle->ncp.ipcp.fsm);
      }
      for (dl = bundle->links; dl; dl = dl->next)
        datalink_Close(dl, staydown);
    }
  } else if (this_dl && this_dl->state != DATALINK_CLOSED &&
             this_dl->state != DATALINK_HANGUP)
    datalink_Close(this_dl, staydown);
}

void
bundle_Down(struct bundle *bundle)
{
  struct datalink *dl;

  for (dl = bundle->links; dl; dl = dl->next)
    datalink_Down(dl, 1);
}

static int
bundle_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct bundle *bundle = descriptor2bundle(d);
  struct datalink *dl;
  int result, want, queued, nlinks;

  result = 0;
  for (dl = bundle->links; dl; dl = dl->next)
    result += descriptor_UpdateSet(&dl->desc, r, w, e, n);

  /* If there are aren't many packets queued, look for some more. */
  for (nlinks = 0, dl = bundle->links; dl; dl = dl->next)
    nlinks++;

  if (nlinks) {
    queued = r ? bundle_FillQueues(bundle) : ip_QueueLen();
    if (bundle->autoload.running) {
      if (queued < bundle->cfg.autoload.max.packets) {
        if (queued > bundle->cfg.autoload.min.packets)
          bundle_StopAutoLoadTimer(bundle);
        else if (bundle->autoload.timer.state != TIMER_RUNNING ||
                 bundle->autoload.comingup)
          bundle_StartAutoLoadTimer(bundle, 0);
      } else if (bundle->autoload.timer.state != TIMER_RUNNING ||
                 !bundle->autoload.comingup)
        bundle_StartAutoLoadTimer(bundle, 1);
    }

    if (r &&
        (bundle->phase == PHASE_NETWORK || bundle->phys_type & PHYS_DEMAND)) {
      /* enough surplus so that we can tell if we're getting swamped */
      want = bundle->cfg.autoload.max.packets + nlinks * 2;
      /* but at least 20 packets ! */
      if (want < 20)
        want = 20;
      if (queued < want) {
        /* Not enough - select() for more */
        FD_SET(bundle->dev.fd, r);
        if (*n < bundle->dev.fd + 1)
          *n = bundle->dev.fd + 1;
        log_Printf(LogTIMER, "tun: fdset(r) %d\n", bundle->dev.fd);
        result++;
      }
    }
  }

  /*
   * This *MUST* be called after the datalink UpdateSet()s as it
   * might be ``holding'' one of the datalinks (death-row) and
   * wants to be able to de-select() it from the descriptor set.
   */
  result += descriptor_UpdateSet(&bundle->ncp.mp.server.desc, r, w, e, n);

  return result;
}

static int
bundle_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct bundle *bundle = descriptor2bundle(d);
  struct datalink *dl;

  for (dl = bundle->links; dl; dl = dl->next)
    if (descriptor_IsSet(&dl->desc, fdset))
      return 1;

  if (descriptor_IsSet(&bundle->ncp.mp.server.desc, fdset))
    return 1;

  return FD_ISSET(bundle->dev.fd, fdset);
}

static void
bundle_DescriptorRead(struct descriptor *d, struct bundle *bundle,
                      const fd_set *fdset)
{
  struct datalink *dl;

  if (descriptor_IsSet(&bundle->ncp.mp.server.desc, fdset))
    descriptor_Read(&bundle->ncp.mp.server.desc, bundle, fdset);

  for (dl = bundle->links; dl; dl = dl->next)
    if (descriptor_IsSet(&dl->desc, fdset))
      descriptor_Read(&dl->desc, bundle, fdset);

  if (FD_ISSET(bundle->dev.fd, fdset)) {
    struct tun_data tun;
    int n, pri;

    /* something to read from tun */
    n = read(bundle->dev.fd, &tun, sizeof tun);
    if (n < 0) {
      log_Printf(LogERROR, "read from tun: %s\n", strerror(errno));
      return;
    }
    n -= sizeof tun - sizeof tun.data;
    if (n <= 0) {
      log_Printf(LogERROR, "read from tun: Only %d bytes read\n", n);
      return;
    }
    if (!tun_check_header(tun, AF_INET))
      return;

    if (((struct ip *)tun.data)->ip_dst.s_addr ==
        bundle->ncp.ipcp.my_ip.s_addr) {
      /* we've been asked to send something addressed *to* us :( */
      if (Enabled(bundle, OPT_LOOPBACK)) {
        pri = PacketCheck(bundle, tun.data, n, &bundle->filter.in);
        if (pri >= 0) {
          struct mbuf *bp;

#ifndef NOALIAS
          if (alias_IsEnabled()) {
            (*PacketAlias.In)(tun.data, sizeof tun.data);
            n = ntohs(((struct ip *)tun.data)->ip_len);
          }
#endif
          bp = mbuf_Alloc(n, MB_IPIN);
          memcpy(MBUF_CTOP(bp), tun.data, n);
          ip_Input(bundle, bp);
          log_Printf(LogDEBUG, "Looped back packet addressed to myself\n");
        }
        return;
      } else
        log_Printf(LogDEBUG, "Oops - forwarding packet addressed to myself\n");
    }

    /*
     * Process on-demand dialup. Output packets are queued within tunnel
     * device until IPCP is opened.
     */

    if (bundle_Phase(bundle) == PHASE_DEAD) {
      /*
       * Note, we must be in AUTO mode :-/ otherwise our interface should
       * *not* be UP and we can't receive data
       */
      if ((pri = PacketCheck(bundle, tun.data, n, &bundle->filter.dial)) >= 0)
        bundle_Open(bundle, NULL, PHYS_DEMAND);
      else
        /*
         * Drop the packet.  If we were to queue it, we'd just end up with
         * a pile of timed-out data in our output queue by the time we get
         * around to actually dialing.  We'd also prematurely reach the 
         * threshold at which we stop select()ing to read() the tun
         * device - breaking auto-dial.
         */
        return;
    }

    pri = PacketCheck(bundle, tun.data, n, &bundle->filter.out);
    if (pri >= 0) {
#ifndef NOALIAS
      if (alias_IsEnabled()) {
        (*PacketAlias.Out)(tun.data, sizeof tun.data);
        n = ntohs(((struct ip *)tun.data)->ip_len);
      }
#endif
      ip_Enqueue(pri, tun.data, n);
    }
  }
}

static void
bundle_DescriptorWrite(struct descriptor *d, struct bundle *bundle,
                       const fd_set *fdset)
{
  struct datalink *dl;

  /* This is not actually necessary as struct mpserver doesn't Write() */
  if (descriptor_IsSet(&bundle->ncp.mp.server.desc, fdset))
    descriptor_Write(&bundle->ncp.mp.server.desc, bundle, fdset);

  for (dl = bundle->links; dl; dl = dl->next)
    if (descriptor_IsSet(&dl->desc, fdset))
      descriptor_Write(&dl->desc, bundle, fdset);
}


struct bundle *
bundle_Create(const char *prefix, int type)
{
  int s, enoentcount, err;
  struct ifreq ifrq;
  static struct bundle bundle;		/* there can be only one */

  if (bundle.ifp.Name != NULL) {	/* Already allocated ! */
    log_Printf(LogERROR, "bundle_Create:  There's only one BUNDLE !\n");
    return NULL;
  }

  err = ENOENT;
  enoentcount = 0;
  for (bundle.unit = 0; ; bundle.unit++) {
    snprintf(bundle.dev.Name, sizeof bundle.dev.Name, "%s%d",
             prefix, bundle.unit);
    bundle.dev.fd = ID0open(bundle.dev.Name, O_RDWR);
    if (bundle.dev.fd >= 0)
      break;
    else if (errno == ENXIO) {
      err = errno;
      break;
    } else if (errno == ENOENT) {
      if (++enoentcount > 2)
	break;
    } else
      err = errno;
  }

  if (bundle.dev.fd < 0) {
    log_Printf(LogWARN, "No available tunnel devices found (%s).\n",
              strerror(err));
    return NULL;
  }

  log_SetTun(bundle.unit);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "bundle_Create: socket(): %s\n", strerror(errno));
    close(bundle.dev.fd);
    return NULL;
  }

  bundle.ifp.Name = strrchr(bundle.dev.Name, '/');
  if (bundle.ifp.Name == NULL)
    bundle.ifp.Name = bundle.dev.Name;
  else
    bundle.ifp.Name++;

  /*
   * Now, bring up the interface.
   */
  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, bundle.ifp.Name, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "OpenTunnel: ioctl(SIOCGIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    close(bundle.dev.fd);
    bundle.ifp.Name = NULL;
    return NULL;
  }
  ifrq.ifr_flags |= IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "OpenTunnel: ioctl(SIOCSIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    close(bundle.dev.fd);
    bundle.ifp.Name = NULL;
    return NULL;
  }

  close(s);

  if ((bundle.ifp.Index = GetIfIndex(bundle.ifp.Name)) < 0) {
    log_Printf(LogERROR, "OpenTunnel: Can't find interface index.\n");
    close(bundle.dev.fd);
    bundle.ifp.Name = NULL;
    return NULL;
  }
  log_Printf(LogPHASE, "Using interface: %s\n", bundle.ifp.Name);

  bundle.ifp.Speed = 0;

  bundle.routing_seq = 0;
  bundle.phase = PHASE_DEAD;
  bundle.CleaningUp = 0;

  bundle.fsm.LayerStart = bundle_LayerStart;
  bundle.fsm.LayerUp = bundle_LayerUp;
  bundle.fsm.LayerDown = bundle_LayerDown;
  bundle.fsm.LayerFinish = bundle_LayerFinish;
  bundle.fsm.object = &bundle;

  bundle.cfg.idle_timeout = NCP_IDLE_TIMEOUT;
  *bundle.cfg.auth.name = '\0';
  *bundle.cfg.auth.key = '\0';
  bundle.cfg.opt = OPT_SROUTES | OPT_IDCHECK | OPT_LOOPBACK |
                   OPT_THROUGHPUT | OPT_UTMP;
  *bundle.cfg.label = '\0';
  bundle.cfg.mtu = DEF_MTU;
  bundle.cfg.autoload.max.packets = 0;
  bundle.cfg.autoload.max.timeout = 0;
  bundle.cfg.autoload.min.packets = 0;
  bundle.cfg.autoload.min.timeout = 0;
  bundle.phys_type = type;

  bundle.links = datalink_Create("deflink", &bundle, type);
  if (bundle.links == NULL) {
    log_Printf(LogERROR, "Cannot create data link: %s\n", strerror(errno));
    close(bundle.dev.fd);
    bundle.ifp.Name = NULL;
    return NULL;
  }

  bundle.desc.type = BUNDLE_DESCRIPTOR;
  bundle.desc.UpdateSet = bundle_UpdateSet;
  bundle.desc.IsSet = bundle_IsSet;
  bundle.desc.Read = bundle_DescriptorRead;
  bundle.desc.Write = bundle_DescriptorWrite;

  mp_Init(&bundle.ncp.mp, &bundle);

  /* Send over the first physical link by default */
  ipcp_Init(&bundle.ncp.ipcp, &bundle, &bundle.links->physical->link,
            &bundle.fsm);

  memset(&bundle.filter, '\0', sizeof bundle.filter);
  bundle.filter.in.fragok = bundle.filter.in.logok = 1;
  bundle.filter.in.name = "IN";
  bundle.filter.out.fragok = bundle.filter.out.logok = 1;
  bundle.filter.out.name = "OUT";
  bundle.filter.dial.name = "DIAL";
  bundle.filter.dial.logok = 1;
  bundle.filter.alive.name = "ALIVE";
  bundle.filter.alive.logok = 1;
  memset(&bundle.idle.timer, '\0', sizeof bundle.idle.timer);
  bundle.idle.done = 0;
  bundle.notify.fd = -1;
  memset(&bundle.autoload.timer, '\0', sizeof bundle.autoload.timer);
  bundle.autoload.done = 0;
  bundle.autoload.running = 0;

  /* Clean out any leftover crud */
  bundle_CleanInterface(&bundle);

  return &bundle;
}

static void
bundle_DownInterface(struct bundle *bundle)
{
  struct ifreq ifrq;
  int s;

  route_IfDelete(bundle, 1);

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "bundle_DownInterface: socket: %s\n", strerror(errno));
    return;
  }

  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, bundle->ifp.Name, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "bundle_DownInterface: ioctl(SIOCGIFFLAGS): %s\n",
       strerror(errno));
    close(s);
    return;
  }
  ifrq.ifr_flags &= ~IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "bundle_DownInterface: ioctl(SIOCSIFFLAGS): %s\n",
       strerror(errno));
    close(s);
    return;
  }
  close(s);
}

void
bundle_Destroy(struct bundle *bundle)
{
  struct datalink *dl;

  /*
   * Clean up the interface.  We don't need to timer_Stop()s, mp_Down(),
   * ipcp_CleanInterface() and bundle_DownInterface() unless we're getting
   * out under exceptional conditions such as a descriptor exception.
   */
  timer_Stop(&bundle->idle.timer);
  timer_Stop(&bundle->autoload.timer);
  mp_Down(&bundle->ncp.mp);
  ipcp_CleanInterface(&bundle->ncp.ipcp);
  bundle_DownInterface(bundle);
  
  /* Again, these are all DATALINK_CLOSED unless we're abending */
  dl = bundle->links;
  while (dl)
    dl = datalink_Destroy(dl);

  /* In case we never made PHASE_NETWORK */
  bundle_Notify(bundle, EX_ERRDEAD);

  bundle->ifp.Name = NULL;
}

struct rtmsg {
  struct rt_msghdr m_rtm;
  char m_space[64];
};

int
bundle_SetRoute(struct bundle *bundle, int cmd, struct in_addr dst,
                struct in_addr gateway, struct in_addr mask, int bang)
{
  struct rtmsg rtmes;
  int s, nb, wb;
  char *cp;
  const char *cmdstr;
  struct sockaddr_in rtdata;
  int result = 1;

  if (bang)
    cmdstr = (cmd == RTM_ADD ? "Add!" : "Delete!");
  else
    cmdstr = (cmd == RTM_ADD ? "Add" : "Delete");
  s = ID0socket(PF_ROUTE, SOCK_RAW, 0);
  if (s < 0) {
    log_Printf(LogERROR, "bundle_SetRoute: socket(): %s\n", strerror(errno));
    return result;
  }
  memset(&rtmes, '\0', sizeof rtmes);
  rtmes.m_rtm.rtm_version = RTM_VERSION;
  rtmes.m_rtm.rtm_type = cmd;
  rtmes.m_rtm.rtm_addrs = RTA_DST;
  rtmes.m_rtm.rtm_seq = ++bundle->routing_seq;
  rtmes.m_rtm.rtm_pid = getpid();
  rtmes.m_rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;

  memset(&rtdata, '\0', sizeof rtdata);
  rtdata.sin_len = sizeof rtdata;
  rtdata.sin_family = AF_INET;
  rtdata.sin_port = 0;
  rtdata.sin_addr = dst;

  cp = rtmes.m_space;
  memcpy(cp, &rtdata, rtdata.sin_len);
  cp += rtdata.sin_len;
  if (cmd == RTM_ADD) {
    if (gateway.s_addr == INADDR_ANY) {
      /* Add a route through the interface */
      struct sockaddr_dl dl;
      const char *iname;
      int ilen;

      iname = Index2Nam(bundle->ifp.Index);
      ilen = strlen(iname);
      dl.sdl_len = sizeof dl - sizeof dl.sdl_data + ilen;
      dl.sdl_family = AF_LINK;
      dl.sdl_index = bundle->ifp.Index;
      dl.sdl_type = 0;
      dl.sdl_nlen = ilen;
      dl.sdl_alen = 0;
      dl.sdl_slen = 0;
      strncpy(dl.sdl_data, iname, sizeof dl.sdl_data);
      memcpy(cp, &dl, dl.sdl_len);
      cp += dl.sdl_len;
      rtmes.m_rtm.rtm_addrs |= RTA_GATEWAY;
    } else {
      rtdata.sin_addr = gateway;
      memcpy(cp, &rtdata, rtdata.sin_len);
      cp += rtdata.sin_len;
      rtmes.m_rtm.rtm_addrs |= RTA_GATEWAY;
    }
  }

  if (dst.s_addr == INADDR_ANY)
    mask.s_addr = INADDR_ANY;

  if (cmd == RTM_ADD || dst.s_addr == INADDR_ANY) {
    rtdata.sin_addr = mask;
    memcpy(cp, &rtdata, rtdata.sin_len);
    cp += rtdata.sin_len;
    rtmes.m_rtm.rtm_addrs |= RTA_NETMASK;
  }

  nb = cp - (char *) &rtmes;
  rtmes.m_rtm.rtm_msglen = nb;
  wb = ID0write(s, &rtmes, nb);
  if (wb < 0) {
    log_Printf(LogTCPIP, "bundle_SetRoute failure:\n");
    log_Printf(LogTCPIP, "bundle_SetRoute:  Cmd = %s\n", cmdstr);
    log_Printf(LogTCPIP, "bundle_SetRoute:  Dst = %s\n", inet_ntoa(dst));
    log_Printf(LogTCPIP, "bundle_SetRoute:  Gateway = %s\n", inet_ntoa(gateway));
    log_Printf(LogTCPIP, "bundle_SetRoute:  Mask = %s\n", inet_ntoa(mask));
failed:
    if (cmd == RTM_ADD && (rtmes.m_rtm.rtm_errno == EEXIST ||
                           (rtmes.m_rtm.rtm_errno == 0 && errno == EEXIST))) {
      if (!bang) {
        log_Printf(LogWARN, "Add route failed: %s already exists\n",
                  inet_ntoa(dst));
        result = 0;	/* Don't add to our dynamic list */
      } else {
        rtmes.m_rtm.rtm_type = cmd = RTM_CHANGE;
        if ((wb = ID0write(s, &rtmes, nb)) < 0)
          goto failed;
      }
    } else if (cmd == RTM_DELETE &&
             (rtmes.m_rtm.rtm_errno == ESRCH ||
              (rtmes.m_rtm.rtm_errno == 0 && errno == ESRCH))) {
      if (!bang)
        log_Printf(LogWARN, "Del route failed: %s: Non-existent\n",
                  inet_ntoa(dst));
    } else if (rtmes.m_rtm.rtm_errno == 0)
      log_Printf(LogWARN, "%s route failed: %s: errno: %s\n", cmdstr,
                inet_ntoa(dst), strerror(errno));
    else
      log_Printf(LogWARN, "%s route failed: %s: %s\n",
		cmdstr, inet_ntoa(dst), strerror(rtmes.m_rtm.rtm_errno));
  }
  log_Printf(LogDEBUG, "wrote %d: cmd = %s, dst = %x, gateway = %x\n",
            wb, cmdstr, (unsigned)dst.s_addr, (unsigned)gateway.s_addr);
  close(s);

  return result;
}

void
bundle_LinkClosed(struct bundle *bundle, struct datalink *dl)
{
  /*
   * Our datalink has closed.
   * CleanDatalinks() (called from DoLoop()) will remove closed
   * 1OFF and DIRECT links.
   * If it's the last data link, enter phase DEAD.
   *
   * NOTE: dl may not be in our list (bundle_SendDatalink()) !
   */

  struct datalink *odl;
  int other_links;

  other_links = 0;
  for (odl = bundle->links; odl; odl = odl->next)
    if (odl != dl && odl->state != DATALINK_CLOSED)
      other_links++;

  if (!other_links) {
    if (dl->physical->type != PHYS_DEMAND)	/* Not in -auto mode */
      bundle_DownInterface(bundle);
    if (bundle->ncp.ipcp.fsm.state > ST_CLOSED ||
        bundle->ncp.ipcp.fsm.state == ST_STARTING) {
      fsm_Down(&bundle->ncp.ipcp.fsm);
      fsm_Close(&bundle->ncp.ipcp.fsm);		/* ST_INITIAL please */
    }
    bundle_NewPhase(bundle, PHASE_DEAD);
    bundle_StopIdleTimer(bundle);
    bundle_StopAutoLoadTimer(bundle);
    bundle->autoload.running = 0;
  } else
    bundle->autoload.running = 1;
}

void
bundle_Open(struct bundle *bundle, const char *name, int mask)
{
  /*
   * Please open the given datalink, or all if name == NULL
   */
  struct datalink *dl;

  timer_Stop(&bundle->autoload.timer);
  for (dl = bundle->links; dl; dl = dl->next)
    if (name == NULL || !strcasecmp(dl->name, name)) {
      if (dl->state == DATALINK_CLOSED && (mask & dl->physical->type)) {
        datalink_Up(dl, 1, 1);
        if (mask == PHYS_DEMAND)
          /* Only one DEMAND link at a time (see the AutoLoad timer) */
          break;
      }
      if (name != NULL)
        break;
    }
}

struct datalink *
bundle2datalink(struct bundle *bundle, const char *name)
{
  struct datalink *dl;

  if (name != NULL) {
    for (dl = bundle->links; dl; dl = dl->next)
      if (!strcasecmp(dl->name, name))
        return dl;
  } else if (bundle->links && !bundle->links->next)
    return bundle->links;

  return NULL;
}

int
bundle_FillQueues(struct bundle *bundle)
{
  int total;

  if (bundle->ncp.mp.active)
    total = mp_FillQueues(bundle);
  else {
    struct datalink *dl;
    int add;

    for (total = 0, dl = bundle->links; dl; dl = dl->next)
      if (dl->state == DATALINK_OPEN) {
        add = link_QueueLen(&dl->physical->link);
        if (add == 0 && dl->physical->out == NULL)
          add = ip_FlushPacket(&dl->physical->link, bundle);
        total += add;
      }
  }

  return total + ip_QueueLen();
}

int
bundle_ShowLinks(struct cmdargs const *arg)
{
  struct datalink *dl;

  for (dl = arg->bundle->links; dl; dl = dl->next) {
    prompt_Printf(arg->prompt, "Name: %s [%s, %s]",
                  dl->name, mode2Nam(dl->physical->type), datalink_State(dl));
    if (dl->physical->link.throughput.rolling && dl->state == DATALINK_OPEN)
      prompt_Printf(arg->prompt, " weight %d, %d bytes/sec",
                    dl->mp.weight,
                    dl->physical->link.throughput.OctetsPerSecond);
    prompt_Printf(arg->prompt, "\n");
  }

  return 0;
}

static const char *
optval(struct bundle *bundle, int bit)
{
  return (bundle->cfg.opt & bit) ? "enabled" : "disabled";
}

int
bundle_ShowStatus(struct cmdargs const *arg)
{
  int remaining;

  prompt_Printf(arg->prompt, "Phase %s\n", bundle_PhaseName(arg->bundle));
  prompt_Printf(arg->prompt, " Device:        %s\n", arg->bundle->dev.Name);
  prompt_Printf(arg->prompt, " Interface:     %s @ %lubps\n",
                arg->bundle->ifp.Name, arg->bundle->ifp.Speed);

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  prompt_Printf(arg->prompt, " Label:         %s\n", arg->bundle->cfg.label);
  prompt_Printf(arg->prompt, " Auth name:     %s\n",
                arg->bundle->cfg.auth.name);
  prompt_Printf(arg->prompt, " Auto Load:     Up after %ds of >= %d packets\n",
                arg->bundle->cfg.autoload.max.timeout,
                arg->bundle->cfg.autoload.max.packets);
  prompt_Printf(arg->prompt, "                Down after %ds of <= %d"
                " packets\n", arg->bundle->cfg.autoload.min.timeout,
                arg->bundle->cfg.autoload.min.packets);
  if (arg->bundle->autoload.timer.state == TIMER_RUNNING)
    prompt_Printf(arg->prompt, "                %ds remaining 'till "
                  "a link comes %s\n",
                  bundle_RemainingAutoLoadTime(arg->bundle),
                  arg->bundle->autoload.comingup ? "up" : "down");
  else
    prompt_Printf(arg->prompt, "                %srunning with %d"
                  " packets queued\n", arg->bundle->autoload.running ?
                  "" : "not ", ip_QueueLen());

  prompt_Printf(arg->prompt, " Idle Timer:    ");
  if (arg->bundle->cfg.idle_timeout) {
    prompt_Printf(arg->prompt, "%ds", arg->bundle->cfg.idle_timeout);
    remaining = bundle_RemainingIdleTime(arg->bundle);
    if (remaining != -1)
      prompt_Printf(arg->prompt, " (%ds remaining)", remaining);
    prompt_Printf(arg->prompt, "\n");
  } else
    prompt_Printf(arg->prompt, "disabled\n");
  prompt_Printf(arg->prompt, " MTU:           ");
  if (arg->bundle->cfg.mtu)
    prompt_Printf(arg->prompt, "%d\n", arg->bundle->cfg.mtu);
  else
    prompt_Printf(arg->prompt, "unspecified\n");

  prompt_Printf(arg->prompt, " Sticky Routes: %s\n",
                optval(arg->bundle, OPT_SROUTES));
  prompt_Printf(arg->prompt, " ID check:      %s\n",
                optval(arg->bundle, OPT_IDCHECK));
  prompt_Printf(arg->prompt, " Loopback:      %s\n",
                optval(arg->bundle, OPT_LOOPBACK));
  prompt_Printf(arg->prompt, " PasswdAuth:    %s\n",
                optval(arg->bundle, OPT_PASSWDAUTH));
  prompt_Printf(arg->prompt, " Proxy:         %s\n",
                optval(arg->bundle, OPT_PROXY));
  prompt_Printf(arg->prompt, " Throughput:    %s\n",
                optval(arg->bundle, OPT_THROUGHPUT));
  prompt_Printf(arg->prompt, " Utmp Logging:  %s\n",
                optval(arg->bundle, OPT_UTMP));

  return 0;
}

static void 
bundle_IdleTimeout(void *v)
{
  struct bundle *bundle = (struct bundle *)v;

  log_Printf(LogPHASE, "Idle timer expired.\n");
  bundle_StopIdleTimer(bundle);
  bundle_Close(bundle, NULL, 1);
}

/*
 *  Start Idle timer. If timeout is reached, we call bundle_Close() to
 *  close LCP and link.
 */
void
bundle_StartIdleTimer(struct bundle *bundle)
{
  timer_Stop(&bundle->idle.timer);
  if ((bundle->phys_type & (PHYS_DEDICATED|PHYS_PERM)) != bundle->phys_type &&
      bundle->cfg.idle_timeout) {
    bundle->idle.timer.func = bundle_IdleTimeout;
    bundle->idle.timer.name = "idle";
    bundle->idle.timer.load = bundle->cfg.idle_timeout * SECTICKS;
    bundle->idle.timer.arg = bundle;
    timer_Start(&bundle->idle.timer);
    bundle->idle.done = time(NULL) + bundle->cfg.idle_timeout;
  }
}

void
bundle_SetIdleTimer(struct bundle *bundle, int value)
{
  bundle->cfg.idle_timeout = value;
  if (bundle_LinkIsUp(bundle))
    bundle_StartIdleTimer(bundle);
}

void
bundle_StopIdleTimer(struct bundle *bundle)
{
  timer_Stop(&bundle->idle.timer);
  bundle->idle.done = 0;
}

static int
bundle_RemainingIdleTime(struct bundle *bundle)
{
  if (bundle->idle.done)
    return bundle->idle.done - time(NULL);
  return -1;
}

int
bundle_IsDead(struct bundle *bundle)
{
  return !bundle->links || (bundle->phase == PHASE_DEAD && bundle->CleaningUp);
}

static void
bundle_LinkAdded(struct bundle *bundle, struct datalink *dl)
{
  bundle->phys_type |= dl->physical->type;
  if (dl->physical->type == PHYS_DEMAND &&
      bundle->autoload.timer.state == TIMER_STOPPED &&
      bundle->phase == PHASE_NETWORK)
    bundle->autoload.running = 1;
}

static void
bundle_LinksRemoved(struct bundle *bundle)
{
  struct datalink *dl;

  bundle->phys_type = 0;
  for (dl = bundle->links; dl; dl = dl->next)
    bundle_LinkAdded(bundle, dl);

  if ((bundle->phys_type & (PHYS_DEDICATED|PHYS_PERM)) == bundle->phys_type)
    timer_Stop(&bundle->idle.timer);
}

static struct datalink *
bundle_DatalinkLinkout(struct bundle *bundle, struct datalink *dl)
{
  struct datalink **dlp;

  for (dlp = &bundle->links; *dlp; dlp = &(*dlp)->next)
    if (*dlp == dl) {
      *dlp = dl->next;
      dl->next = NULL;
      bundle_LinksRemoved(bundle);
      return dl;
    }

  return NULL;
}

static void
bundle_DatalinkLinkin(struct bundle *bundle, struct datalink *dl)
{
  struct datalink **dlp = &bundle->links;

  while (*dlp)
    dlp = &(*dlp)->next;

  *dlp = dl;
  dl->next = NULL;

  bundle_LinkAdded(bundle, dl);
}

void
bundle_CleanDatalinks(struct bundle *bundle)
{
  struct datalink **dlp = &bundle->links;
  int found = 0;

  while (*dlp)
    if ((*dlp)->state == DATALINK_CLOSED &&
        (*dlp)->physical->type & (PHYS_DIRECT|PHYS_1OFF)) {
      *dlp = datalink_Destroy(*dlp);
      found++;
    } else
      dlp = &(*dlp)->next;

  if (found)
    bundle_LinksRemoved(bundle);
}

int
bundle_DatalinkClone(struct bundle *bundle, struct datalink *dl,
                     const char *name)
{
  if (bundle2datalink(bundle, name)) {
    log_Printf(LogWARN, "Clone: %s: name already exists\n", name);
    return 0;
  }

  bundle_DatalinkLinkin(bundle, datalink_Clone(dl, name));
  return 1;
}

void
bundle_DatalinkRemove(struct bundle *bundle, struct datalink *dl)
{
  dl = bundle_DatalinkLinkout(bundle, dl);
  if (dl)
    datalink_Destroy(dl);
}

void
bundle_SetLabel(struct bundle *bundle, const char *label)
{
  if (label)
    strncpy(bundle->cfg.label, label, sizeof bundle->cfg.label - 1);
  else
    *bundle->cfg.label = '\0';
}

const char *
bundle_GetLabel(struct bundle *bundle)
{
  return *bundle->cfg.label ? bundle->cfg.label : NULL;
}

void
bundle_ReceiveDatalink(struct bundle *bundle, int s, struct sockaddr_un *sun)
{
  char cmsgbuf[sizeof(struct cmsghdr) + sizeof(int)];
  struct cmsghdr *cmsg = (struct cmsghdr *)cmsgbuf;
  struct msghdr msg;
  struct iovec iov[SCATTER_SEGMENTS];
  struct datalink *dl;
  int niov, link_fd, expect, f;

  log_Printf(LogPHASE, "Receiving datalink\n");

  /* Create our scatter/gather array */
  niov = 1;
  iov[0].iov_len = strlen(Version) + 1;
  iov[0].iov_base = (char *)malloc(iov[0].iov_len);
  if (datalink2iov(NULL, iov, &niov, sizeof iov / sizeof *iov) == -1)
    return;

  for (f = expect = 0; f < niov; f++)
    expect += iov[f].iov_len;

  /* Set up our message */
  cmsg->cmsg_len = sizeof cmsgbuf;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;

  memset(&msg, '\0', sizeof msg);
  msg.msg_name = (caddr_t)sun;
  msg.msg_namelen = sizeof *sun;
  msg.msg_iov = iov;
  msg.msg_iovlen = niov;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof cmsgbuf;

  log_Printf(LogDEBUG, "Expecting %d scatter/gather bytes\n", expect);
  f = expect + 100;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, &f, sizeof f);
  if ((f = recvmsg(s, &msg, MSG_WAITALL)) != expect) {
    if (f == -1)
      log_Printf(LogERROR, "Failed recvmsg: %s\n", strerror(errno));
    else
      log_Printf(LogERROR, "Failed recvmsg: Got %d, not %d\n", f, expect);
    while (niov--)
      free(iov[niov].iov_base);
    return;
  }

  /* We've successfully received an open file descriptor through our socket */
  link_fd = *(int *)CMSG_DATA(cmsg);

  write(s, "!",1 );	/* ACK */

  if (strncmp(Version, iov[0].iov_base, iov[0].iov_len)) {
    log_Printf(LogWARN, "Cannot receive datalink, incorrect version"
               " (\"%.*s\", not \"%s\")\n", (int)iov[0].iov_len,
               iov[0].iov_base, Version);
    close(link_fd);
    while (niov--)
      free(iov[niov].iov_base);
    return;
  }

  niov = 1;
  dl = iov2datalink(bundle, iov, &niov, sizeof iov / sizeof *iov, link_fd);
  if (dl) {
    bundle_DatalinkLinkin(bundle, dl);
    datalink_AuthOk(dl);
  } else
    close(link_fd);

  free(iov[0].iov_base);
}

void
bundle_SendDatalink(struct datalink *dl, int s, struct sockaddr_un *sun)
{
  char cmsgbuf[sizeof(struct cmsghdr) + sizeof(int)], ack;
  struct cmsghdr *cmsg = (struct cmsghdr *)cmsgbuf;
  struct msghdr msg;
  struct iovec iov[SCATTER_SEGMENTS];
  int niov, link_fd, f, expect;

  log_Printf(LogPHASE, "Transmitting datalink %s\n", dl->name);

  bundle_LinkClosed(dl->bundle, dl);
  bundle_DatalinkLinkout(dl->bundle, dl);

  /* Build our scatter/gather array */
  iov[0].iov_len = strlen(Version) + 1;
  iov[0].iov_base = strdup(Version);
  niov = 1;

  link_fd = datalink2iov(dl, iov, &niov, sizeof iov / sizeof *iov);

  if (link_fd != -1) {
    cmsg->cmsg_len = sizeof cmsgbuf;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *(int *)CMSG_DATA(cmsg) = link_fd;

    memset(&msg, '\0', sizeof msg);
    msg.msg_name = (caddr_t)sun;
    msg.msg_namelen = sizeof *sun;
    msg.msg_iov = iov;
    msg.msg_iovlen = niov;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof cmsgbuf;

    for (f = expect = 0; f < niov; f++)
      expect += iov[f].iov_len;

    log_Printf(LogDEBUG, "Sending %d bytes in scatter/gather array\n", expect);

    f = expect + SOCKET_OVERHEAD;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &f, sizeof f);
    if (sendmsg(s, &msg, 0) == -1)
      log_Printf(LogERROR, "Failed sendmsg: %s\n", strerror(errno));
    /* We must get the ACK before closing the descriptor ! */
    read(s, &ack, 1);
    close(link_fd);
  }

  while (niov--)
    free(iov[niov].iov_base);
}

int
bundle_RenameDatalink(struct bundle *bundle, struct datalink *ndl,
                      const char *name)
{
  struct datalink *dl;

  if (!strcasecmp(ndl->name, name))
    return 1;

  for (dl = bundle->links; dl; dl = dl->next)
    if (!strcasecmp(dl->name, name))
      return 0;

  datalink_Rename(ndl, name);
  return 1;
}

int
bundle_SetMode(struct bundle *bundle, struct datalink *dl, int mode)
{
  int omode;

  omode = dl->physical->type;
  if (omode == mode)
    return 1;

  if (mode == PHYS_DEMAND && !(bundle->phys_type & PHYS_DEMAND))
    /* Changing to demand-dial mode */
    if (bundle->ncp.ipcp.peer_ip.s_addr == INADDR_ANY) {
      log_Printf(LogWARN, "You must `set ifaddr' before changing mode to %s\n",
                 mode2Nam(mode));
      return 0;
    }

  if (!datalink_SetMode(dl, mode))
    return 0;

  if (mode == PHYS_DEMAND && !(bundle->phys_type & PHYS_DEMAND))
    ipcp_InterfaceUp(&bundle->ncp.ipcp);

  /* Regenerate phys_type and adjust autoload & idle timers */
  bundle_LinksRemoved(bundle);

  if (omode == PHYS_DEMAND && !(bundle->phys_type & PHYS_DEMAND))
    /* Changing from demand-dial mode */
    ipcp_CleanInterface(&bundle->ncp.ipcp);

  return 1;
}

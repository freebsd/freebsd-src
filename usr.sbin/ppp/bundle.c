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
 *	$Id: bundle.c,v 1.1.2.45 1998/04/11 21:50:37 brian Exp $
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
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
#include "auth.h"
#include "lcpproto.h"
#include "chap.h"
#include "tun.h"
#include "prompt.h"
#include "chat.h"
#include "datalink.h"
#include "ip.h"

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
    LogPrintf(LogPHASE, "bundle: %s\n", PhaseNames[new]);

  switch (new) {
  case PHASE_DEAD:
    bundle->phase = new;
    break;

  case PHASE_ESTABLISH:
    bundle->phase = new;
    break;

  case PHASE_AUTHENTICATE:
    bundle->phase = new;
    bundle_DisplayPrompt(bundle);
    break;

  case PHASE_NETWORK:
    ipcp_Setup(&bundle->ncp.ipcp);
    FsmUp(&bundle->ncp.ipcp.fsm);
    FsmOpen(&bundle->ncp.ipcp.fsm);
    /* Fall through */

  case PHASE_TERMINATE:
    bundle->phase = new;
    bundle_DisplayPrompt(bundle);
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
    LogPrintf(LogERROR, "bundle_CleanInterface: socket(): %s\n",
              strerror(errno));
    return (-1);
  }
  strncpy(ifrq.ifr_name, bundle->ifname, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  while (ID0ioctl(s, SIOCGIFADDR, &ifrq) == 0) {
    memset(&ifra.ifra_mask, '\0', sizeof ifra.ifra_mask);
    strncpy(ifra.ifra_name, bundle->ifname, sizeof ifra.ifra_name - 1);
    ifra.ifra_name[sizeof ifra.ifra_name - 1] = '\0';
    ifra.ifra_addr = ifrq.ifr_addr;
    if (ID0ioctl(s, SIOCGIFDSTADDR, &ifrq) < 0) {
      if (ifra.ifra_addr.sa_family == AF_INET)
        LogPrintf(LogERROR,
                  "bundle_CleanInterface: Can't get dst for %s on %s !\n",
                  inet_ntoa(((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr),
                  bundle->ifname);
      return 0;
    }
    ifra.ifra_broadaddr = ifrq.ifr_dstaddr;
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      if (ifra.ifra_addr.sa_family == AF_INET)
        LogPrintf(LogERROR,
                  "bundle_CleanInterface: Can't delete %s address on %s !\n",
                  inet_ntoa(((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr),
                  bundle->ifname);
      return 0;
    }
  }

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
      LogPrintf(LogPHASE, "Parent notified of success.\n");
    else
      LogPrintf(LogPHASE, "Failed to notify parent of success.\n");
    close(bundle->notify.fd);
    bundle->notify.fd = -1;
  }
}

static void
bundle_vLayerUp(void *v, struct fsm *fp)
{
  bundle_LayerUp((struct bundle *)v, fp);
}

void
bundle_LayerUp(struct bundle *bundle, struct fsm *fp)
{
  /*
   * The given fsm is now up
   * If it's an LCP (including MP initialisation), set our mtu
   * (This routine is also called from mp_Init() with it's LCP)
   * If it's an NCP, tell our -background parent to go away.
   * If it's the first NCP, start the idle timer.
   */

  if (fp->proto == PROTO_LCP) {
    if (bundle->ncp.mp.active) {
      int speed;
      struct datalink *dl;

      for (dl = bundle->links, speed = 0; dl; dl = dl->next)
        speed += modem_Speed(dl->physical);
      if (speed)
        tun_configure(bundle, bundle->ncp.mp.link.lcp.his_mru, speed);
    } else
      tun_configure(bundle, fsm2lcp(fp)->his_mru,
                    modem_Speed(link2physical(fp->link)));
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
   * If it's our last NCP *OR* LCP, enter TERMINATE phase.
   * If it's an LCP and we're in multilink mode, adjust our tun speed.
   */

  struct bundle *bundle = (struct bundle *)v;

  if (fp->proto == PROTO_IPCP) {
    bundle_StopIdleTimer(bundle);
  } else if (fp->proto == PROTO_LCP) {
    int speed, others_active;
    struct datalink *dl;

    others_active = 0;
    for (dl = bundle->links, speed = 0; dl; dl = dl->next)
      if (fp != &dl->physical->link.lcp.fsm &&
          dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP) {
        speed += modem_Speed(dl->physical);
        others_active++;
      }
    if (bundle->ncp.mp.active && speed)
      tun_configure(bundle, bundle->ncp.mp.link.lcp.his_mru, speed);

    if (!others_active)
      bundle_NewPhase(bundle, PHASE_TERMINATE);
  }
}

static void
bundle_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm is now down (fp cannot be NULL)
   *
   * If it's the last LCP, FsmDown all NCPs
   * If it's the last NCP, FsmClose all LCPs
   */

  struct bundle *bundle = (struct bundle *)v;
  struct datalink *dl;

  if (fp->proto == PROTO_IPCP) {
    bundle_NewPhase(bundle, PHASE_TERMINATE);
    for (dl = bundle->links; dl; dl = dl->next)
      datalink_Close(dl, 0);
    FsmDown(fp);
    FsmClose(fp);
  } else if (fp->proto == PROTO_LCP) {
    int others_active;

    others_active = 0;
    for (dl = bundle->links; dl; dl = dl->next)
      if (fp != &dl->physical->link.lcp.fsm &&
          dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP)
        others_active++;

    if (!others_active) {
      FsmDown(&bundle->ncp.ipcp.fsm);
      FsmClose(&bundle->ncp.ipcp.fsm);		/* ST_INITIAL please */
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
   * If name == NULL or name is the last datalink, enter TERMINATE phase
   * and FsmClose all NCPs (except our MP)
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
    LogPrintf(LogWARN, "%s: Invalid datalink name\n", name);
    return;
  }

  if (!others_active) {
    if (bundle->ncp.ipcp.fsm.state > ST_CLOSED ||
        bundle->ncp.ipcp.fsm.state == ST_STARTING)
      FsmClose(&bundle->ncp.ipcp.fsm);
    else {
      if (bundle->ncp.ipcp.fsm.state > ST_INITIAL) {
        FsmClose(&bundle->ncp.ipcp.fsm);
        FsmDown(&bundle->ncp.ipcp.fsm);
      }
      for (dl = bundle->links; dl; dl = dl->next)
        datalink_Close(dl, staydown);
    }
  } else if (this_dl && this_dl->state != DATALINK_CLOSED &&
             this_dl->state != DATALINK_HANGUP)
    datalink_Close(this_dl, staydown);
}

static int
bundle_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct bundle *bundle = descriptor2bundle(d);
  struct datalink *dl;
  struct descriptor *desc;
  int result;

  result = 0;
  for (dl = bundle->links; dl; dl = dl->next)
    result += descriptor_UpdateSet(&dl->desc, r, w, e, n);

  for (desc = bundle->desc.next; desc; desc = desc->next)
    result += descriptor_UpdateSet(desc, r, w, e, n);

  return result;
}

static int
bundle_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct bundle *bundle = descriptor2bundle(d);
  struct datalink *dl;
  struct descriptor *desc;

  for (dl = bundle->links; dl; dl = dl->next)
    if (descriptor_IsSet(&dl->desc, fdset))
      return 1;

  for (desc = bundle->desc.next; desc; desc = desc->next)
    if (descriptor_IsSet(desc, fdset))
      return 1;

  return 0;
}

static void
bundle_DescriptorRead(struct descriptor *d, struct bundle *bundle,
                      const fd_set *fdset)
{
  struct datalink *dl;
  struct descriptor *desc;

  for (dl = bundle->links; dl; dl = dl->next)
    if (descriptor_IsSet(&dl->desc, fdset))
      descriptor_Read(&dl->desc, bundle, fdset);

  for (desc = bundle->desc.next; desc; desc = desc->next)
    if (descriptor_IsSet(desc, fdset))
      descriptor_Read(desc, bundle, fdset);
}

static void
bundle_DescriptorWrite(struct descriptor *d, struct bundle *bundle,
                       const fd_set *fdset)
{
  struct datalink *dl;
  struct descriptor *desc;

  for (dl = bundle->links; dl; dl = dl->next)
    if (descriptor_IsSet(&dl->desc, fdset))
      descriptor_Write(&dl->desc, bundle, fdset);

  for (desc = bundle->desc.next; desc; desc = desc->next)
    if (descriptor_IsSet(desc, fdset))
      descriptor_Write(desc, bundle, fdset);
}


#define MAX_TUN 256
/*
 * MAX_TUN is set at 256 because that is the largest minor number
 * we can use (certainly with mknod(1) anyway).  The search for a
 * device aborts when it reaches the first `Device not configured'
 * (ENXIO) or the third `No such file or directory' (ENOENT) error.
 */
struct bundle *
bundle_Create(const char *prefix, struct prompt *prompt, int type)
{
  int s, enoentcount, err;
  struct ifreq ifrq;
  static struct bundle bundle;		/* there can be only one */

  if (bundle.ifname != NULL) {	/* Already allocated ! */
    LogPrintf(LogERROR, "bundle_Create:  There's only one BUNDLE !\n");
    return NULL;
  }

  err = ENOENT;
  enoentcount = 0;
  for (bundle.unit = 0; bundle.unit <= MAX_TUN; bundle.unit++) {
    snprintf(bundle.dev, sizeof bundle.dev, "%s%d", prefix, bundle.unit);
    bundle.tun_fd = ID0open(bundle.dev, O_RDWR);
    if (bundle.tun_fd >= 0)
      break;
    if (errno == ENXIO) {
      bundle.unit = MAX_TUN;
      err = errno;
    } else if (errno == ENOENT) {
      if (++enoentcount > 2)
	bundle.unit = MAX_TUN;
    } else
      err = errno;
  }

  if (bundle.unit > MAX_TUN) {
    prompt_Printf(prompt, "No tunnel device is available (%s).\n",
                  strerror(err));
    return NULL;
  }

  LogSetTun(bundle.unit);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "bundle_Create: socket(): %s\n", strerror(errno));
    close(bundle.tun_fd);
    return NULL;
  }

  bundle.ifname = strrchr(bundle.dev, '/');
  if (bundle.ifname == NULL)
    bundle.ifname = bundle.dev;
  else
    bundle.ifname++;

  /*
   * Now, bring up the interface.
   */
  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, bundle.ifname, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OpenTunnel: ioctl(SIOCGIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    close(bundle.tun_fd);
    bundle.ifname = NULL;
    return NULL;
  }
  ifrq.ifr_flags |= IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OpenTunnel: ioctl(SIOCSIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    close(bundle.tun_fd);
    bundle.ifname = NULL;
    return NULL;
  }

  close(s);

  if ((bundle.ifIndex = GetIfIndex(bundle.ifname)) < 0) {
    LogPrintf(LogERROR, "OpenTunnel: Can't find ifindex.\n");
    close(bundle.tun_fd);
    bundle.ifname = NULL;
    return NULL;
  }

  prompt_Printf(prompt, "Using interface: %s\n", bundle.ifname);
  LogPrintf(LogPHASE, "Using interface: %s\n", bundle.ifname);

  bundle.routing_seq = 0;
  bundle.phase = PHASE_DEAD;
  bundle.CleaningUp = 0;

  bundle.fsm.LayerStart = bundle_LayerStart;
  bundle.fsm.LayerUp = bundle_vLayerUp;
  bundle.fsm.LayerDown = bundle_LayerDown;
  bundle.fsm.LayerFinish = bundle_LayerFinish;
  bundle.fsm.object = &bundle;

  bundle.cfg.idle_timeout = NCP_IDLE_TIMEOUT;
  bundle.phys_type = type;

  bundle.links = datalink_Create("default", &bundle, &bundle.fsm, type);
  if (bundle.links == NULL) {
    LogPrintf(LogERROR, "Cannot create data link: %s\n", strerror(errno));
    close(bundle.tun_fd);
    bundle.ifname = NULL;
    return NULL;
  }

  bundle.desc.type = BUNDLE_DESCRIPTOR;
  bundle.desc.next = NULL;
  bundle.desc.UpdateSet = bundle_UpdateSet;
  bundle.desc.IsSet = bundle_IsSet;
  bundle.desc.Read = bundle_DescriptorRead;
  bundle.desc.Write = bundle_DescriptorWrite;

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

  /* Clean out any leftover crud */
  bundle_CleanInterface(&bundle);

  if (prompt) {
    /* Retrospectively introduce ourselves to the prompt */
    prompt->bundle = &bundle;
    bundle_RegisterDescriptor(&bundle, &prompt->desc);
  }

  return &bundle;
}

static void
bundle_DownInterface(struct bundle *bundle)
{
  struct ifreq ifrq;
  int s;

  DeleteIfRoutes(bundle, 1);

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "bundle_DownInterface: socket: %s\n", strerror(errno));
    return;
  }

  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, bundle->ifname, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "bundle_DownInterface: ioctl(SIOCGIFFLAGS): %s\n",
       strerror(errno));
    close(s);
    return;
  }
  ifrq.ifr_flags &= ~IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "bundle_DownInterface: ioctl(SIOCSIFFLAGS): %s\n",
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
  struct descriptor *desc, *ndesc;

  if (bundle->phys_type & PHYS_DEMAND) {
    IpcpCleanInterface(&bundle->ncp.ipcp);
    bundle_DownInterface(bundle);
  }
  
  dl = bundle->links;
  while (dl)
    dl = datalink_Destroy(dl);

  bundle_Notify(bundle, EX_ERRDEAD);

  desc = bundle->desc.next;
  while (desc) {
    ndesc = desc->next;
    if (desc->type == PROMPT_DESCRIPTOR)
      prompt_Destroy((struct prompt *)desc, 1);
    else
      LogPrintf(LogERROR, "bundle_Destroy: Don't know how to delete descriptor"
                " type %d\n", desc->type);
    desc = ndesc;
  }
  bundle->desc.next = NULL;
  bundle->ifname = NULL;
}

struct rtmsg {
  struct rt_msghdr m_rtm;
  char m_space[64];
};

void
bundle_SetRoute(struct bundle *bundle, int cmd, struct in_addr dst,
                struct in_addr gateway, struct in_addr mask, int bang)
{
  struct rtmsg rtmes;
  int s, nb, wb;
  char *cp;
  const char *cmdstr;
  struct sockaddr_in rtdata;

  if (bang)
    cmdstr = (cmd == RTM_ADD ? "Add!" : "Delete!");
  else
    cmdstr = (cmd == RTM_ADD ? "Add" : "Delete");
  s = ID0socket(PF_ROUTE, SOCK_RAW, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "bundle_SetRoute: socket(): %s\n", strerror(errno));
    return;
  }
  memset(&rtmes, '\0', sizeof rtmes);
  rtmes.m_rtm.rtm_version = RTM_VERSION;
  rtmes.m_rtm.rtm_type = cmd;
  rtmes.m_rtm.rtm_addrs = RTA_DST;
  rtmes.m_rtm.rtm_seq = ++bundle->routing_seq;
  rtmes.m_rtm.rtm_pid = getpid();
  rtmes.m_rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;

  memset(&rtdata, '\0', sizeof rtdata);
  rtdata.sin_len = 16;
  rtdata.sin_family = AF_INET;
  rtdata.sin_port = 0;
  rtdata.sin_addr = dst;

  cp = rtmes.m_space;
  memcpy(cp, &rtdata, 16);
  cp += 16;
  if (cmd == RTM_ADD)
    if (gateway.s_addr == INADDR_ANY) {
      /* Add a route through the interface */
      struct sockaddr_dl dl;
      const char *iname;
      int ilen;

      iname = Index2Nam(bundle->ifIndex);
      ilen = strlen(iname);
      dl.sdl_len = sizeof dl - sizeof dl.sdl_data + ilen;
      dl.sdl_family = AF_LINK;
      dl.sdl_index = bundle->ifIndex;
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
      memcpy(cp, &rtdata, 16);
      cp += 16;
      rtmes.m_rtm.rtm_addrs |= RTA_GATEWAY;
    }

  if (dst.s_addr == INADDR_ANY)
    mask.s_addr = INADDR_ANY;

  if (cmd == RTM_ADD || dst.s_addr == INADDR_ANY) {
    rtdata.sin_addr = mask;
    memcpy(cp, &rtdata, 16);
    cp += 16;
    rtmes.m_rtm.rtm_addrs |= RTA_NETMASK;
  }

  nb = cp - (char *) &rtmes;
  rtmes.m_rtm.rtm_msglen = nb;
  wb = ID0write(s, &rtmes, nb);
  if (wb < 0) {
    LogPrintf(LogTCPIP, "bundle_SetRoute failure:\n");
    LogPrintf(LogTCPIP, "bundle_SetRoute:  Cmd = %s\n", cmd);
    LogPrintf(LogTCPIP, "bundle_SetRoute:  Dst = %s\n", inet_ntoa(dst));
    LogPrintf(LogTCPIP, "bundle_SetRoute:  Gateway = %s\n", inet_ntoa(gateway));
    LogPrintf(LogTCPIP, "bundle_SetRoute:  Mask = %s\n", inet_ntoa(mask));
failed:
    if (cmd == RTM_ADD && (rtmes.m_rtm.rtm_errno == EEXIST ||
                           (rtmes.m_rtm.rtm_errno == 0 && errno == EEXIST)))
      if (!bang)
        LogPrintf(LogWARN, "Add route failed: %s already exists\n",
                  inet_ntoa(dst));
      else {
        rtmes.m_rtm.rtm_type = cmd = RTM_CHANGE;
        if ((wb = ID0write(s, &rtmes, nb)) < 0)
          goto failed;
      }
    else if (cmd == RTM_DELETE &&
             (rtmes.m_rtm.rtm_errno == ESRCH ||
              (rtmes.m_rtm.rtm_errno == 0 && errno == ESRCH))) {
      if (!bang)
        LogPrintf(LogWARN, "Del route failed: %s: Non-existent\n",
                  inet_ntoa(dst));
    } else if (rtmes.m_rtm.rtm_errno == 0)
      LogPrintf(LogWARN, "%s route failed: %s: errno: %s\n", cmdstr,
                inet_ntoa(dst), strerror(errno));
    else
      LogPrintf(LogWARN, "%s route failed: %s: %s\n",
		cmdstr, inet_ntoa(dst), strerror(rtmes.m_rtm.rtm_errno));
  }
  LogPrintf(LogDEBUG, "wrote %d: cmd = %s, dst = %x, gateway = %x\n",
            wb, cmdstr, dst.s_addr, gateway.s_addr);
  close(s);
}

void
bundle_LinkLost(struct bundle *bundle, struct physical *p, int staydown)
{
  /*
   * Locate the appropriate datalink, and Down it.
   *
   * The LayerFinish() called from the datalinks LCP will 
   * potentially Down our NCPs (if it's the last link).
   *
   * The LinkClosed() called when the datalink is finally in
   * the CLOSED state MAY cause the entire datalink to be deleted
   * and MAY cause a program exit.
   */

  if (p->type == PHYS_STDIN || bundle->CleaningUp)
    staydown = 1;
  datalink_Down(p->dl, staydown);
}

void
bundle_LinkClosed(struct bundle *bundle, struct datalink *dl)
{
  /*
   * Our datalink has closed.
   * UpdateSet() will remove 1OFF and STDIN links.
   * If it's the last data link, enter phase DEAD.
   */

  struct datalink *odl;
  int other_links;

  other_links = 0;
  for (odl = bundle->links; odl; odl = odl->next)
    if (odl != dl && odl->state != DATALINK_CLOSED)
      other_links++;

  if (!other_links) {
    if (dl->physical->type != PHYS_DEMAND)
      bundle_DownInterface(bundle);
    bundle_NewPhase(bundle, PHASE_DEAD);
    bundle_DisplayPrompt(bundle);
  }
}

void
bundle_Open(struct bundle *bundle, const char *name, int mask)
{
  /*
   * Please open the given datalink, or all if name == NULL
   */
  struct datalink *dl;

  for (dl = bundle->links; dl; dl = dl->next)
    if (name == NULL || !strcasecmp(dl->name, name)) {
      if (mask & dl->physical->type)
        datalink_Up(dl, 1, 1);
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

  if (bundle->ncp.mp.active) {
    total = mp_FillQueues(bundle);
  } else {
    total = link_QueueLen(&bundle->links->physical->link);
    if (total == 0 && bundle->links->physical->out == NULL)
      total = IpFlushPacket(&bundle->links->physical->link, bundle);
  }

  return total + ip_QueueLen();
}

int
bundle_ShowLinks(struct cmdargs const *arg)
{
  if (arg->cx)
    datalink_Show(arg->cx, arg->prompt);
  else {
    struct datalink *dl;

    for (dl = arg->bundle->links; dl; dl = dl->next)
      datalink_Show(dl, arg->prompt);
  }

  return 0;
}

int
bundle_ShowStatus(struct cmdargs const *arg)
{
  int remaining;

  prompt_Printf(arg->prompt, "Phase %s\n", bundle_PhaseName(arg->bundle));
  prompt_Printf(arg->prompt, " Interface: %s\n", arg->bundle->dev);

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  prompt_Printf(arg->prompt, " Auth name: %s\n", arg->bundle->cfg.auth.name);
  prompt_Printf(arg->prompt, " Idle Timer: ");
  if (arg->bundle->cfg.idle_timeout) {
    prompt_Printf(arg->prompt, "%ds", arg->bundle->cfg.idle_timeout);
    remaining = bundle_RemainingIdleTime(arg->bundle);
    if (remaining != -1)
      prompt_Printf(arg->prompt, " (%ds remaining)", remaining);
    prompt_Printf(arg->prompt, "\n");
  } else
    prompt_Printf(arg->prompt, "disabled\n");

  return 0;
}

static void 
bundle_IdleTimeout(void *v)
{
  struct bundle *bundle = (struct bundle *)v;

  bundle->idle.done = 0;
  LogPrintf(LogPHASE, "IPCP Idle timer expired.\n");
  bundle_Close(bundle, NULL, 1);
}

/*
 *  Start Idle timer. If timeout is reached, we call bundle_Close() to
 *  close LCP and link.
 */
void
bundle_StartIdleTimer(struct bundle *bundle)
{
  if (!(bundle->phys_type & (PHYS_DEDICATED|PHYS_PERM)) &&
      bundle->cfg.idle_timeout) {
    StopTimer(&bundle->idle.timer);
    bundle->idle.timer.func = bundle_IdleTimeout;
    bundle->idle.timer.name = "idle";
    bundle->idle.timer.load = bundle->cfg.idle_timeout * SECTICKS;
    bundle->idle.timer.state = TIMER_STOPPED;
    bundle->idle.timer.arg = bundle;
    StartTimer(&bundle->idle.timer);
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
  StopTimer(&bundle->idle.timer);
  bundle->idle.done = 0;
}

int
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

void
bundle_RegisterDescriptor(struct bundle *bundle, struct descriptor *d)
{
  d->next = bundle->desc.next;
  bundle->desc.next = d;
}

void
bundle_UnRegisterDescriptor(struct bundle *bundle, struct descriptor *d)
{
  struct descriptor **desc;

  for (desc = &bundle->desc.next; *desc; desc = &(*desc)->next)
    if (*desc == d) {
      *desc = d->next;
      break;
    }
}

void
bundle_DelPromptDescriptors(struct bundle *bundle, struct server *s)
{
  struct descriptor **desc;
  struct prompt *p;

  desc = &bundle->desc.next;
  while (*desc) {
    if ((*desc)->type == PROMPT_DESCRIPTOR) {
      p = (struct prompt *)*desc;
      if (p->owner == s) {
        prompt_Destroy(p, 1);
        desc = &bundle->desc.next;
        continue;
      }
    }
    desc = &(*desc)->next;
  }
}

void
bundle_DisplayPrompt(struct bundle *bundle)
{
  struct descriptor **desc;

  for (desc = &bundle->desc.next; *desc; desc = &(*desc)->next)
    if ((*desc)->type == PROMPT_DESCRIPTOR)
      prompt_Required((struct prompt *)*desc);
}

void
bundle_WriteTermPrompt(struct bundle *bundle, struct datalink *dl,
                       const char *data, int len)
{
  struct descriptor *desc;
  struct prompt *p;

  for (desc = bundle->desc.next; desc; desc = desc->next)
    if (desc->type == PROMPT_DESCRIPTOR) {
      p = (struct prompt *)desc;
      if (prompt_IsTermMode(p, dl))
        prompt_Printf(p, "%.*s", len, data);
    }
}

void
bundle_SetTtyCommandMode(struct bundle *bundle, struct datalink *dl)
{
  struct descriptor *desc;
  struct prompt *p;

  for (desc = bundle->desc.next; desc; desc = desc->next)
    if (desc->type == PROMPT_DESCRIPTOR) {
      p = (struct prompt *)desc;
      if (prompt_IsTermMode(p, dl))
        prompt_TtyCommandMode(p);
    }
}

static void
bundle_GenPhysType(struct bundle *bundle)
{
  struct datalink *dl;

  bundle->phys_type = 0;
  for (dl = bundle->links; dl; dl = dl->next)
    bundle->phys_type |= dl->physical->type;
}

void
bundle_DatalinkClone(struct bundle *bundle, struct datalink *dl,
                     const char *name)
{
  struct datalink *ndl = datalink_Clone(dl, name);

  ndl->next = dl->next;
  dl->next = ndl;
  bundle_GenPhysType(bundle);
}

void
bundle_DatalinkRemove(struct bundle *bundle, struct datalink *dl)
{
  struct datalink **dlp;

  if (dl->state == DATALINK_CLOSED)
    for (dlp = &bundle->links; *dlp; dlp = &(*dlp)->next)
      if (*dlp == dl) {
        *dlp = datalink_Destroy(dl);
        break;
      }
  bundle_GenPhysType(bundle);
}

void
bundle_CleanDatalinks(struct bundle *bundle)
{
  struct datalink **dlp = &bundle->links;

  while (*dlp)
    if ((*dlp)->state == DATALINK_CLOSED &&
        (*dlp)->physical->type & (PHYS_STDIN|PHYS_1OFF))
      *dlp = datalink_Destroy(*dlp);
    else
      dlp = &(*dlp)->next;
  bundle_GenPhysType(bundle);
}

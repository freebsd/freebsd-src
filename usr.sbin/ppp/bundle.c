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
 *	$Id: bundle.c,v 1.1.2.17 1998/02/27 01:22:16 brian Exp $
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if_dl.h>

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
#include "hdlc.h"
#include "throughput.h"
#include "ipcp.h"
#include "link.h"
#include "bundle.h"
#include "loadalias.h"
#include "vars.h"
#include "arp.h"
#include "systems.h"
#include "route.h"
#include "lcp.h"
#include "ccp.h"
#include "async.h"
#include "descriptor.h"
#include "physical.h"
#include "modem.h"
#include "main.h"
#include "auth.h"
#include "lcpproto.h"
#include "pap.h"
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
bundle_NewPhase(struct bundle *bundle, struct physical *physical, u_int new)
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
    prompt_Display(&prompt, bundle);
    break;

  case PHASE_NETWORK:
    tun_configure(bundle, LcpInfo.his_mru, modem_Speed(physical));
    ipcp_Setup(&IpcpInfo);
    FsmUp(&IpcpInfo.fsm);
    FsmOpen(&IpcpInfo.fsm);
    /* Fall through */

  case PHASE_TERMINATE:
    bundle->phase = new;
    prompt_Display(&prompt, bundle);
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
  struct bundle *bundle = (struct bundle *)v;

  if (fp == &LcpInfo.fsm)
    bundle_NewPhase(bundle, link2physical(fp->link), PHASE_ESTABLISH);
}

static void
bundle_LayerUp(void *v, struct fsm *fp)
{
  /*
   * The given fsm is now up
   * If it's a datalink, enter network phase
   * If it's an NCP, tell our background mode parent to go away.
   */

  struct bundle *bundle = (struct bundle *)v;

  if (fp->proto == PROTO_LCP)
    bundle_NewPhase(bundle, link2physical(fp->link), PHASE_NETWORK);

  if (fp == &IpcpInfo.fsm)
    if (mode & MODE_BACKGROUND && BGFiledes[1] != -1) {
      char c = EX_NORMAL;

      if (write(BGFiledes[1], &c, 1) == 1)
	LogPrintf(LogPHASE, "Parent notified of success.\n");
      else
	LogPrintf(LogPHASE, "Failed to notify parent of success.\n");
      close(BGFiledes[1]);
      BGFiledes[1] = -1;
    }
}

static void
bundle_LayerDown(void *v, struct fsm *fp)
{
  /*
   * The given FSM has been told to come down.
   * We don't do anything here, as the FSM will eventually
   * come up or down and will call LayerUp or LayerFinish.
   */
}

static void
bundle_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm is now down (fp cannot be NULL)
   *
   * If it's the last LCP, FsmDown all NCPs
   * If it's the last NCP, FsmClose all LCPs and enter TERMINATE phase.
   */

  struct bundle *bundle = (struct bundle *)v;

  if (fp == &LcpInfo.fsm) {
    FsmDown(&IpcpInfo.fsm);		/* You've lost your underlings */
    FsmClose(&IpcpInfo.fsm);		/* ST_INITIAL please */
  } else if (fp == &IpcpInfo.fsm) {
    struct datalink *dl;

    bundle_NewPhase(bundle, NULL, PHASE_TERMINATE);

    for (dl = bundle->links; dl; dl = dl->next)
      datalink_Close(dl, 1);
  }
}

int
bundle_LinkIsUp(const struct bundle *bundle)
{
  return IpcpInfo.fsm.state == ST_OPENED;
}

void
bundle_Close(struct bundle *bundle, const char *name, int staydown)
{
  /*
   * Please close the given datalink.
   *
   * If name == NULL or name is the last datalink, enter TERMINATE phase.
   *
   * If name == NULL, FsmClose all NCPs.
   *
   * If name is the last datalink, FsmClose all NCPs.
   *
   * If isn't the last datalink, just Close that datalink.
   */

  struct datalink *dl;

  if (IpcpInfo.fsm.state > ST_CLOSED || IpcpInfo.fsm.state == ST_STARTING) {
    bundle_NewPhase(bundle, NULL, PHASE_TERMINATE);
    FsmClose(&IpcpInfo.fsm);
    if (staydown)
      for (dl = bundle->links; dl; dl = dl->next)
        datalink_StayDown(dl);
  } else
    for (dl = bundle->links; dl; dl = dl->next)
      datalink_Close(dl, staydown);
}

/*
 *  Open tunnel device and returns its descriptor
 */

#define MAX_TUN 256
/*
 * MAX_TUN is set at 256 because that is the largest minor number
 * we can use (certainly with mknod(1) anyway.  The search for a
 * device aborts when it reaches the first `Device not configured'
 * (ENXIO) or the third `No such file or directory' (ENOENT) error.
 */
struct bundle *
bundle_Create(const char *prefix)
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
    prompt_Printf(&prompt, "No tunnel device is available (%s).\n",
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

  prompt_Printf(&prompt, "Using interface: %s\n", bundle.ifname);
  LogPrintf(LogPHASE, "Using interface: %s\n", bundle.ifname);

  bundle.routing_seq = 0;
  bundle.phase = 0;

  bundle.fsm.LayerStart = bundle_LayerStart;
  bundle.fsm.LayerUp = bundle_LayerUp;
  bundle.fsm.LayerDown = bundle_LayerDown;
  bundle.fsm.LayerFinish = bundle_LayerFinish;
  bundle.fsm.object = &bundle;

  bundle.links = datalink_Create("Modem", &bundle, &bundle.fsm);
  if (bundle.links == NULL) {
    LogPrintf(LogERROR, "Cannot create data link: %s\n", strerror(errno));
    close(bundle.tun_fd);
    bundle.ifname = NULL;
    return NULL;
  }

  ipcp_Init(&IpcpInfo, &bundle, &bundle.links->physical->link, &bundle.fsm);

  /* Clean out any leftover crud */
  bundle_CleanInterface(&bundle);

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

  if (mode & MODE_AUTO) {
    IpcpCleanInterface(&IpcpInfo.fsm);
    bundle_DownInterface(bundle);
  }
  
  dl = bundle->links;
  while (dl)
    dl = datalink_Destroy(dl);

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
bundle_LinkLost(struct bundle *bundle, struct link *link, int staydown)
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

  if ((mode & MODE_DIRECT) || CleaningUp)
    staydown = 1;
  datalink_Down(bundle->links, staydown);
}

void
bundle_LinkClosed(struct bundle *bundle, struct datalink *dl)
{
  /*
   * Our datalink has closed.
   * If it's DIRECT or BACKGROUND, delete it.
   * If it's the last data link,
   */

  if (mode & (MODE_BACKGROUND|MODE_DIRECT))
     CleaningUp = 1;

  if (!(mode & MODE_AUTO))
    bundle_DownInterface(bundle);

  if (mode & MODE_DDIAL)
    datalink_Up(dl, 1, 1);
  else
    bundle_NewPhase(bundle, NULL, PHASE_DEAD);

  if (mode & MODE_INTER)
    prompt_Display(&prompt, bundle);
    
}

void
bundle_Open(struct bundle *bundle, const char *name)
{
  /*
   * Please open the given datalink, or all if name == NULL
   */
  struct datalink *dl;
  int runscripts;

  runscripts = (mode & (MODE_DIRECT|MODE_DEDICATED)) ? 0 : 1;
  for (dl = bundle->links; dl; dl = dl->next)
    if (name == NULL || !strcasecmp(dl->name, name)) {
      datalink_Up(dl, runscripts, 1);
      if (name != NULL)
        break;
    }
  bundle_NewPhase(bundle, NULL, PHASE_ESTABLISH);
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

struct physical *
bundle2physical(struct bundle *bundle, const char *name)
{
  struct datalink *dl = bundle2datalink(bundle, name);
  return dl ? dl->physical : NULL;
}

struct ccp *
bundle2ccp(struct bundle *bundle, const char *name)
{
  struct datalink *dl = bundle2datalink(bundle, name);
  if (dl)
    return &dl->ccp;
  return NULL;
}

struct authinfo *
bundle2pap(struct bundle *bundle, const char *name)
{
  struct datalink *dl = bundle2datalink(bundle, name);
  if (dl)
    return &dl->pap;
  return NULL;
}

struct chap *
bundle2chap(struct bundle *bundle, const char *name)
{
  struct datalink *dl = bundle2datalink(bundle, name);
  if (dl)
    return &dl->chap;
  return NULL;
}

struct link *
bundle2link(struct bundle *bundle, const char *name)
{
  struct physical *physical = bundle2physical(bundle, name);
  return physical ? &physical->link : NULL;
}

int
bundle_UpdateSet(struct bundle *bundle, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct datalink *dl;
  int result;

  result = 0;
  for (dl = bundle->links; dl; dl = dl->next)
    result += descriptor_UpdateSet(&dl->desc, r, w, e, n);

  return result;
}

int
bundle_FillQueues(struct bundle *bundle)
{
  struct datalink *dl;
  int packets, total;

  total = 0;
  for (dl = bundle->links; dl; dl = dl->next) {
    packets = link_QueueLen(&dl->physical->link);
    if (packets == 0) {
      IpStartOutput(&dl->physical->link, bundle);
      packets = link_QueueLen(&dl->physical->link);
    }
    total += packets;
  }
  total += ip_QueueLen();

  return total;
}

int
bundle_ShowLinks(struct cmdargs const *arg)
{
  if (arg->cx)
    datalink_Show(arg->cx);
  else {
    struct datalink *dl;

    for (dl = arg->bundle->links; dl; dl = dl->next)
      datalink_Show(dl);
  }

  return 0;
}

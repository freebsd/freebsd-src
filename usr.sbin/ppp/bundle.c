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
 *	$Id: bundle.c,v 1.1.2.3 1998/02/06 02:23:28 brian Exp $
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
#include "throughput.h"
#include "ipcp.h"
#include "bundle.h"
#include "loadalias.h"
#include "vars.h"
#include "arp.h"
#include "systems.h"
#include "route.h"
#include "lcp.h"
#include "ccp.h"
#include "modem.h"

static int
bundle_SetIpDevice(struct bundle *bundle, struct in_addr myaddr,
                   struct in_addr hisaddr, struct in_addr netmask, int silent)
{
  struct sockaddr_in *sock_in;
  int s;
  u_long mask, addr;
  struct ifaliasreq ifra;

  /* If given addresses are alreay set, then ignore this request */
  if (bundle->if_mine.s_addr == myaddr.s_addr &&
      bundle->if_peer.s_addr == hisaddr.s_addr)
    return 0;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "SetIpDevice: socket(): %s\n", strerror(errno));
    return (-1);
  }

  memset(&ifra, '\0', sizeof ifra);
  strncpy(ifra.ifra_name, bundle->ifname, sizeof ifra.ifra_name - 1);
  ifra.ifra_name[sizeof ifra.ifra_name - 1] = '\0';

  /* If different address has been set, then delete it first */
  if (bundle->if_mine.s_addr != INADDR_ANY ||
      bundle->if_peer.s_addr != INADDR_ANY)
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCDIFADDR): %s\n",
		strerror(errno));
      close(s);
      return (-1);
    }

  /* Set interface address */
  sock_in = (struct sockaddr_in *)&ifra.ifra_addr;
  sock_in->sin_family = AF_INET;
  sock_in->sin_addr = myaddr;
  sock_in->sin_len = sizeof *sock_in;

  /* Set destination address */
  sock_in = (struct sockaddr_in *)&ifra.ifra_broadaddr;
  sock_in->sin_family = AF_INET;
  sock_in->sin_addr = hisaddr;
  sock_in->sin_len = sizeof *sock_in;

  addr = ntohl(myaddr.s_addr);
  if (IN_CLASSA(addr))
    mask = IN_CLASSA_NET;
  else if (IN_CLASSB(addr))
    mask = IN_CLASSB_NET;
  else
    mask = IN_CLASSC_NET;

  /* if subnet mask is given, use it instead of class mask */
  if (netmask.s_addr != INADDR_ANY && (ntohl(netmask.s_addr) & mask) == mask)
    mask = ntohl(netmask.s_addr);

  sock_in = (struct sockaddr_in *)&ifra.ifra_mask;
  sock_in->sin_family = AF_INET;
  sock_in->sin_addr.s_addr = htonl(mask);
  sock_in->sin_len = sizeof *sock_in;

  if (ID0ioctl(s, SIOCAIFADDR, &ifra) < 0) {
    if (!silent)
      LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCAIFADDR): %s\n",
		strerror(errno));
    close(s);
    return (-1);
  }

  bundle->if_peer.s_addr = hisaddr.s_addr;
  bundle->if_mine.s_addr = myaddr.s_addr;

  if (Enabled(ConfProxy))
    sifproxyarp(bundle, s);

  close(s);
  return (0);
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

int
bundle_TrySetIPaddress(struct bundle *bundle, struct in_addr myaddr,
                       struct in_addr hisaddr)
{
  return bundle_SetIpDevice(bundle, myaddr, hisaddr, ifnetmask, 1);
}

int
bundle_SetIPaddress(struct bundle *bundle, struct in_addr myaddr,
                    struct in_addr hisaddr)
{
  return bundle_SetIpDevice(bundle, myaddr, hisaddr, ifnetmask, 0);
}

void
bundle_Linkup(struct bundle *bundle)
{
  if (bundle->linkup == 0) {
    char *s;

    reconnectState = RECON_UNKNOWN;
    if (mode & MODE_BACKGROUND && BGFiledes[1] != -1) {
      char c = EX_NORMAL;

      if (write(BGFiledes[1], &c, 1) == 1)
	LogPrintf(LogPHASE, "Parent notified of success.\n");
      else
	LogPrintf(LogPHASE, "Failed to notify parent of success.\n");
      close(BGFiledes[1]);
      BGFiledes[1] = -1;
    }

    s = inet_ntoa(bundle->if_peer);
    if (LogIsKept(LogLINK))
      LogPrintf(LogLINK, "OsLinkup: %s\n", s);
    else
      LogPrintf(LogLCP, "OsLinkup: %s\n", s);

    /*
     * XXX this stuff should really live in the FSM.  Our config should
     * associate executable sections in files with events.
     */
    if (SelectSystem(bundle, inet_ntoa(bundle->if_mine), LINKUPFILE) < 0) {
      if (GetLabel()) {
	if (SelectSystem(bundle, GetLabel(), LINKUPFILE) < 0)
	  SelectSystem(bundle, "MYADDR", LINKUPFILE);
      } else
	SelectSystem(bundle, "MYADDR", LINKUPFILE);
    }
    bundle->linkup = 1;
  }
}

int
bundle_LinkIsUp(const struct bundle *bundle)
{
  return bundle->linkup;
}

void
bundle_Linkdown(struct bundle *bundle)
{
  char *s = NULL;
  int Level;

  if (bundle->linkup) {
    s = inet_ntoa(bundle->if_peer);
    Level = LogIsKept(LogLINK) ? LogLINK : LogIPCP;
    LogPrintf(Level, "OsLinkdown: %s\n", s);
  }

  FsmClose(&IpcpInfo.fsm);
  FsmClose(&CcpInfo.fsm);

  if (bundle->linkup) {
    /*
     * XXX this stuff should really live in the FSM.  Our config should
     * associate executable sections in files with events.
     */
    bundle->linkup = 0;
    if (SelectSystem(bundle, s, LINKDOWNFILE) < 0)
      if (GetLabel()) {
	if (SelectSystem(bundle, GetLabel(), LINKDOWNFILE) < 0)
	  SelectSystem(bundle, "MYADDR", LINKDOWNFILE);
      } else
	SelectSystem(bundle, "MYADDR", LINKDOWNFILE);
  }
}

int
bundle_InterfaceDown(struct bundle *bundle)
{
  struct ifreq ifrq;
  struct ifaliasreq ifra;
  int s;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: socket: %s\n", strerror(errno));
    return -1;
  }

  if (Enabled(ConfProxy))
    cifproxyarp(bundle, s);

  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, bundle->ifname, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: ioctl(SIOCGIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    return -1;
  }
  ifrq.ifr_flags &= ~IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: ioctl(SIOCSIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    return -1;
  }

  if (bundle->if_mine.s_addr != INADDR_ANY ||
      bundle->if_peer.s_addr != INADDR_ANY) {
    memset(&ifra, '\0', sizeof ifra);
    strncpy(ifra.ifra_name, bundle->ifname, sizeof ifra.ifra_name - 1);
    ifra.ifra_name[sizeof ifra.ifra_name - 1] = '\0';
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "OsInterfaceDown: ioctl(SIOCDIFADDR): %s\n",
		strerror(errno));
      close(s);
      return -1;
    }
    bundle->if_mine.s_addr = bundle->if_peer.s_addr = INADDR_ANY;
  }

  close(s);
  return 0;
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
    if (VarTerm)
      fprintf(VarTerm, "No tunnel device is available (%s).\n", strerror(err));
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

  if (VarTerm)
    fprintf(VarTerm, "Using interface: %s\n", bundle.ifname);
  LogPrintf(LogPHASE, "Using interface: %s\n", bundle.ifname);

  bundle.linkup = 0;
  bundle.if_mine.s_addr = bundle.if_peer.s_addr = INADDR_ANY;
  bundle.routing_seq = 0;

  /* Clean out any leftover crud */
  bundle_CleanInterface(&bundle);

  bundle.physical = modem_Create("default");
  if (bundle.physical == NULL) {
    LogPrintf(LogERROR, "Cannot create modem device: %s\n", strerror(errno));
    return NULL;
  }

  return &bundle;
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
    LogPrintf(LogERROR, "OsSetRoute: socket(): %s\n", strerror(errno));
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
    LogPrintf(LogTCPIP, "OsSetRoute failure:\n");
    LogPrintf(LogTCPIP, "OsSetRoute:  Cmd = %s\n", cmd);
    LogPrintf(LogTCPIP, "OsSetRoute:  Dst = %s\n", inet_ntoa(dst));
    LogPrintf(LogTCPIP, "OsSetRoute:  Gateway = %s\n", inet_ntoa(gateway));
    LogPrintf(LogTCPIP, "OsSetRoute:  Mask = %s\n", inet_ntoa(mask));
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
bundle_Down(struct bundle *bundle, struct link *link)
{
  /* If link is NULL slam everything down, otherwise `link' is dead */

  LogPrintf(LogPHASE, "Disconnected!\n");
  FsmDown(&LcpInfo.fsm);
  FsmDown(&IpcpInfo.fsm);
  FsmDown(&CcpInfo.fsm);
}

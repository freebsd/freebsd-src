/*
 *	      PPP OS Layer Interface Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: os.c,v 1.39 1998/01/08 23:47:55 brian Exp $
 *
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <net/route.h>
#include <arpa/inet.h>

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
#include "ipcp.h"
#include "os.h"
#include "loadalias.h"
#include "vars.h"
#include "arp.h"
#include "systems.h"
#include "route.h"
#include "lcp.h"
#include "ccp.h"
#include "modem.h"

char *IfDevName;

static struct ifaliasreq ifra;
static struct ifreq ifrq;
static struct in_addr oldmine, oldhis;
static int linkup;

enum set_method { SET_UP, SET_DOWN, SET_TRY };

static int
SetIpDevice(struct in_addr myaddr,
	    struct in_addr hisaddr,
	    struct in_addr netmask,
	    enum set_method how)
{
  struct sockaddr_in *sock_in;
  int s;
  u_long mask, addr;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "SetIpDevice: socket(): %s\n", strerror(errno));
    return (-1);
  }
  if (how == SET_DOWN) {
    if (Enabled(ConfProxy))
      cifproxyarp(s, oldhis.s_addr);
    if (oldmine.s_addr == 0 && oldhis.s_addr == 0) {
      close(s);
      return (0);
    }
    memset(&ifra.ifra_addr, '\0', sizeof ifra.ifra_addr);
    memset(&ifra.ifra_broadaddr, '\0', sizeof ifra.ifra_broadaddr);
    memset(&ifra.ifra_mask, '\0', sizeof ifra.ifra_mask);
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCDIFADDR): %s\n",
		strerror(errno));
      close(s);
      return (-1);
    }
    oldmine.s_addr = oldhis.s_addr = 0;
  } else {
    /* If given addresses are alreay set, then ignore this request */
    if (oldmine.s_addr == myaddr.s_addr && oldhis.s_addr == hisaddr.s_addr) {
      close(s);
      return (0);
    }

    /*
     * If different address has been set, then delete it first.
     */
    if (oldmine.s_addr || oldhis.s_addr) {
      memset(&ifra.ifra_addr, '\0', sizeof ifra.ifra_addr);
      memset(&ifra.ifra_broadaddr, '\0', sizeof ifra.ifra_broadaddr);
      memset(&ifra.ifra_mask, '\0', sizeof ifra.ifra_mask);
      if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
        LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCDIFADDR): %s\n",
		  strerror(errno));
        close(s);
        return (-1);
      }
    }

    /* Set interface address */
    sock_in = (struct sockaddr_in *) & (ifra.ifra_addr);
    sock_in->sin_family = AF_INET;
    sock_in->sin_addr = myaddr;
    sock_in->sin_len = sizeof *sock_in;

    /* Set destination address */
    sock_in = (struct sockaddr_in *) & (ifra.ifra_broadaddr);
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

    /*
     * if subnet mask is given, use it instead of class mask.
     */
    if (netmask.s_addr && (ntohl(netmask.s_addr) & mask) == mask)
      mask = ntohl(netmask.s_addr);

    sock_in = (struct sockaddr_in *) & (ifra.ifra_mask);
    sock_in->sin_family = AF_INET;
    sock_in->sin_addr.s_addr = htonl(mask);
    sock_in->sin_len = sizeof *sock_in;

    if (ID0ioctl(s, SIOCAIFADDR, &ifra) < 0) {
      if (how != SET_TRY)
        LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCAIFADDR): %s\n",
		  strerror(errno));
      close(s);
      return (-1);
    }

    oldhis.s_addr = hisaddr.s_addr;
    oldmine.s_addr = myaddr.s_addr;
    if (Enabled(ConfProxy))
      sifproxyarp(s, hisaddr.s_addr);
  }
  close(s);
  return (0);
}

int
CleanInterface(const char *name)
{
  int s;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "SetIpDevice: socket(): %s\n", strerror(errno));
    return (-1);
  }
  strncpy(ifrq.ifr_name, name, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  while (ID0ioctl(s, SIOCGIFADDR, &ifrq) == 0) {
    memset(&ifra.ifra_mask, '\0', sizeof ifra.ifra_mask);
    ifra.ifra_addr = ifrq.ifr_addr;
    if (ID0ioctl(s, SIOCGIFDSTADDR, &ifrq) < 0) {
      if (ifra.ifra_addr.sa_family == AF_INET)
        LogPrintf(LogERROR, "tun_configure: Can't get dst for %s on %s !\n",
                  inet_ntoa(((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr),
                  name);
      return 0;
    }
    ifra.ifra_broadaddr = ifrq.ifr_dstaddr;
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      if (ifra.ifra_addr.sa_family == AF_INET)
        LogPrintf(LogERROR, "tun_configure: Can't delete %s address on %s !\n",
                  inet_ntoa(((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr),
                  name);
      return 0;
    }
  }

  return 1;
}

int
OsTrySetIpaddress(struct in_addr myaddr, struct in_addr hisaddr)
{
  return (SetIpDevice(myaddr, hisaddr, ifnetmask, SET_TRY));
}

int
OsSetIpaddress(struct in_addr myaddr, struct in_addr hisaddr)
{
  return (SetIpDevice(myaddr, hisaddr, ifnetmask, SET_UP));
}

static struct in_addr peer_addr;
struct in_addr defaddr;

void
OsLinkup()
{
  char *s;

  if (linkup == 0) {
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
    peer_addr = IpcpInfo.his_ipaddr;
    s = (char *) inet_ntoa(peer_addr);
    if (LogIsKept(LogLINK))
      LogPrintf(LogLINK, "OsLinkup: %s\n", s);
    else
      LogPrintf(LogLCP, "OsLinkup: %s\n", s);

    if (SelectSystem(inet_ntoa(IpcpInfo.want_ipaddr), LINKUPFILE) < 0) {
      if (GetLabel()) {
	if (SelectSystem(GetLabel(), LINKUPFILE) < 0)
	  SelectSystem("MYADDR", LINKUPFILE);
      } else
	SelectSystem("MYADDR", LINKUPFILE);
    }
    linkup = 1;
  }
}

int
OsLinkIsUp()
{
  return linkup;
}

void
OsLinkdown()
{
  char *s;
  int Level;

  if (linkup) {
    s = (char *) inet_ntoa(peer_addr);
    Level = LogIsKept(LogLINK) ? LogLINK : LogIPCP;
    LogPrintf(Level, "OsLinkdown: %s\n", s);

    FsmDown(&IpcpFsm);	/* IPCP must come down */
    FsmDown(&CcpFsm);	/* CCP must come down */

    linkup = 0;
    if (SelectSystem(s, LINKDOWNFILE) < 0) {
      if (GetLabel()) {
	if (SelectSystem(GetLabel(), LINKDOWNFILE) < 0)
	  SelectSystem("MYADDR", LINKDOWNFILE);
      } else
	SelectSystem("MYADDR", LINKDOWNFILE);
    }
  }
}

int
OsInterfaceDown(int final)
{
  struct in_addr zeroaddr;
  int s;

  OsLinkdown();
  if (!final && (mode & MODE_DAEMON))	/* We still want interface alive */
    return (0);
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: socket: %s\n", strerror(errno));
    return (-1);
  }
  ifrq.ifr_flags &= ~IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: ioctl(SIOCSIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    return (-1);
  }
  zeroaddr.s_addr = 0;
  SetIpDevice(zeroaddr, zeroaddr, zeroaddr, SET_DOWN);

  close(s);
  return (0);
}

/*
 *  Open tunnel device and returns its descriptor
 */

#define MAX_TUN 256
/* MAX_TUN is set at an arbitrarily large value  *
 * as the loop aborts when it reaches the first  *
 * 'Device not configured' (ENXIO), or the third *
 * 'No such file or directory' (ENOENT) error.   */
int
OpenTunnel(int *ptun)
{
  int s;
  char ifname[IFNAMSIZ];
  static char devname[14];	/* sufficient room for "/dev/tun65535" */
  unsigned unit, enoentcount = 0;
  int err;

  err = ENOENT;
  for (unit = 0; unit <= MAX_TUN; unit++) {
    snprintf(devname, sizeof devname, "/dev/tun%d", unit);
    tun_out = ID0open(devname, O_RDWR);
    if (tun_out >= 0)
      break;
    if (errno == ENXIO) {
      unit = MAX_TUN;
      err = errno;
    } else if (errno == ENOENT) {
      enoentcount++;
      if (enoentcount > 2)
	unit = MAX_TUN;
    } else
      err = errno;
  }
  if (unit > MAX_TUN) {
    if (VarTerm)
      fprintf(VarTerm, "No tunnel device is available (%s).\n", strerror(err));
    return -1;
  }
  *ptun = unit;

  LogSetTun(unit);

  /*
   * At first, name the interface.
   */
  strncpy(ifname, devname + 5, IFNAMSIZ - 1);

  memset(&ifra, '\0', sizeof ifra);
  memset(&ifrq, '\0', sizeof ifrq);

  strncpy(ifrq.ifr_name, ifname, IFNAMSIZ - 1);
  strncpy(ifra.ifra_name, ifname, IFNAMSIZ - 1);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "OpenTunnel: socket(): %s\n", strerror(errno));
    return (-1);
  }

  /*
   * Now, bring up the interface.
   */
  if (ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OpenTunnel: ioctl(SIOCGIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    return (-1);
  }
  ifrq.ifr_flags |= IFF_UP;
  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OpenTunnel: ioctl(SIOCSIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    return (-1);
  }
  tun_in = tun_out;
  IfDevName = devname + 5;
  if (GetIfIndex(IfDevName) < 0) {
    LogPrintf(LogERROR, "OpenTunnel: Can't find ifindex.\n");
    close(s);
    return (-1);
  }
  if (VarTerm)
    fprintf(VarTerm, "Using interface: %s\n", IfDevName);
  LogPrintf(LogPHASE, "Using interface: %s\n", IfDevName);
  close(s);
  return (0);
}

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
 * $Id: os.c,v 1.29 1997/10/29 01:19:47 brian Exp $
 *
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_var.h>
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

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "ipcp.h"
#include "os.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "arp.h"
#include "systems.h"
#include "route.h"
#include "ccp.h"
#include "modem.h"

char *IfDevName;

static struct ifaliasreq ifra;
static struct ifreq ifrq;
static struct in_addr oldmine, oldhis;
static int linkup;

static int
SetIpDevice(struct in_addr myaddr,
	    struct in_addr hisaddr,
	    struct in_addr netmask,
	    int updown)
{
  struct sockaddr_in *sock_in;
  int s;
  int changeaddr = 0;
  u_long mask, addr;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "SetIpDevice: socket(): %s\n", strerror(errno));
    return (-1);
  }
  if (updown == 0) {
    if (Enabled(ConfProxy))
      cifproxyarp(s, oldhis.s_addr);
    if (oldmine.s_addr == 0 && oldhis.s_addr == 0) {
      close(s);
      return (0);
    }
    memset(&ifra.ifra_addr, '\0', sizeof(ifra.ifra_addr));
    memset(&ifra.ifra_broadaddr, '\0', sizeof(ifra.ifra_addr));
    memset(&ifra.ifra_mask, '\0', sizeof(ifra.ifra_addr));
    if (ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCDIFADDR): %s\n",
		strerror(errno));
      close(s);
      return (-1);
    }
    oldmine.s_addr = oldhis.s_addr = 0;
  } else {

    /*
     * If given addresses are alreay set, then ignore this request.
     */
    if (oldmine.s_addr == myaddr.s_addr && oldhis.s_addr == hisaddr.s_addr) {
      close(s);
      return (0);
    }

    /*
     * If different address has been set, then delete it first.
     */
    if (oldmine.s_addr || oldhis.s_addr) {
      changeaddr = 1;
    }

    /*
     * Set interface address
     */
    sock_in = (struct sockaddr_in *) & (ifra.ifra_addr);
    sock_in->sin_family = AF_INET;
    sock_in->sin_addr = oldmine = myaddr;
    sock_in->sin_len = sizeof(*sock_in);

    /*
     * Set destination address
     */
    sock_in = (struct sockaddr_in *) & (ifra.ifra_broadaddr);
    sock_in->sin_family = AF_INET;
    sock_in->sin_addr = oldhis = hisaddr;
    sock_in->sin_len = sizeof(*sock_in);

    /*
     * */
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
    sock_in->sin_len = sizeof(*sock_in);

    if (changeaddr) {

      /*
       * Interface already exists. Just change the address.
       */
      memcpy(&ifrq.ifr_addr, &ifra.ifra_addr, sizeof(struct sockaddr));
      if (ioctl(s, SIOCSIFADDR, &ifra) < 0)
	LogPrintf(LogERROR, "SetIpDevice: ioctl(SIFADDR): %s\n",
		  strerror(errno));
      memcpy(&ifrq.ifr_dstaddr, &ifra.ifra_broadaddr, sizeof(struct sockaddr));
      if (ioctl(s, SIOCSIFDSTADDR, &ifrq) < 0)
	LogPrintf(LogERROR, "SetIpDevice: ioctl(SIFDSTADDR): %s\n",
		  strerror(errno));
#ifdef notdef
      memcpy(&ifrq.ifr_broadaddr, &ifra.ifra_mask, sizeof(struct sockaddr));
      if (ioctl(s, SIOCSIFBRDADDR, &ifrq) < 0)
	LogPrintf(LogERROR, "SetIpDevice: ioctl(SIFBRDADDR): %s\n",
		  strerror(errno));
#endif
    } else if (ioctl(s, SIOCAIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "SetIpDevice: ioctl(SIOCAIFADDR): %s\n",
		strerror(errno));
      close(s);
      return (-1);
    }
    if (Enabled(ConfProxy))
      sifproxyarp(s, hisaddr.s_addr);
  }
  close(s);
  return (0);
}

int
OsSetIpaddress(struct in_addr myaddr,
	       struct in_addr hisaddr,
	       struct in_addr netmask)
{
  return (SetIpDevice(myaddr, hisaddr, netmask, 1));
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
      if (dstsystem) {
	if (SelectSystem(dstsystem, LINKUPFILE) < 0)
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

    if (!(mode & MODE_AUTO))
      DeleteIfRoutes(0);
    linkup = 0;
    if (SelectSystem(s, LINKDOWNFILE) < 0) {
      if (dstsystem) {
	if (SelectSystem(dstsystem, LINKDOWNFILE) < 0)
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
  if (!final && (mode & MODE_AUTO))	/* We still want interface alive */
    return (0);
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: socket: %s\n", strerror(errno));
    return (-1);
  }
  ifrq.ifr_flags &= ~IFF_UP;
  if (ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "OsInterfaceDown: ioctl(SIOCSIFFLAGS): %s\n",
	      strerror(errno));
    close(s);
    return (-1);
  }
  zeroaddr.s_addr = 0;
  SetIpDevice(zeroaddr, zeroaddr, zeroaddr, 0);

  close(s);
  return (0);
}

void
OsSetInterfaceParams(int type, int mtu, int speed)
{
  struct tuninfo info;

  info.type = type;
  info.mtu = mtu;
  if (VarPrefMTU != 0 && VarPrefMTU < mtu)
    info.mtu = VarPrefMTU;
  info.baudrate = speed;
  if (ioctl(tun_out, TUNSIFINFO, &info) < 0)
    LogPrintf(LogERROR, "OsSetInterfaceParams: ioctl(TUNSIFINFO): %s\n",
	      strerror(errno));
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
    snprintf(devname, sizeof(devname), "/dev/tun%d", unit);
    tun_out = open(devname, O_RDWR);
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

  memset(&ifra, '\0', sizeof(ifra));
  memset(&ifrq, '\0', sizeof(ifrq));

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
  if (ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
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

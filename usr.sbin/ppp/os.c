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
 * $Id: os.c,v 1.23 1997/06/13 02:07:30 brian Exp $
 *
 */
#include "fsm.h"
#include <sys/param.h>
#include <sys/socket.h>
#if BSD >= 199206 || _BSDI_VERSION >= 199312
#include <sys/select.h>
#endif
#include <sys/ioctl.h>
#include <sys/time.h>

#include <fcntl.h>
#include <errno.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_tun.h>
#include <net/route.h>
#include <arpa/inet.h>

#include "ipcp.h"
#include "os.h"
#include "loadalias.h"
#include "vars.h"
#include "arp.h"
#include "systems.h"
#include "route.h"

static struct ifaliasreq ifra;
static struct ifreq ifrq;
static struct in_addr oldmine, oldhis;
static int linkup;

#ifdef bsdi
extern char *inet_ntoa();

#endif
extern void HangupModem();

char *IfDevName;

static int
SetIpDevice(struct in_addr myaddr,
	    struct in_addr hisaddr,
	    struct in_addr netmask,
	    int updown)
{
  struct sockaddr_in *sin;
  int s;
  int changeaddr = 0;
  u_long mask, addr;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "socket: %s", strerror(errno));
    return (-1);
  }
  if (updown == 0) {
    if (Enabled(ConfProxy))
      cifproxyarp(s, oldhis.s_addr);
    if (oldmine.s_addr == 0 && oldhis.s_addr == 0) {
      close(s);
      return (0);
    }
    bzero(&ifra.ifra_addr, sizeof(ifra.ifra_addr));
    bzero(&ifra.ifra_broadaddr, sizeof(ifra.ifra_addr));
    bzero(&ifra.ifra_mask, sizeof(ifra.ifra_addr));
    if (ioctl(s, SIOCDIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "ioctl(SIOCDIFADDR): %s", strerror(errno));
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
    sin = (struct sockaddr_in *) & (ifra.ifra_addr);
    sin->sin_family = AF_INET;
    sin->sin_addr = oldmine = myaddr;
    sin->sin_len = sizeof(*sin);

    /*
     * Set destination address
     */
    sin = (struct sockaddr_in *) & (ifra.ifra_broadaddr);
    sin->sin_family = AF_INET;
    sin->sin_addr = oldhis = hisaddr;
    sin->sin_len = sizeof(*sin);

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

    sin = (struct sockaddr_in *) & (ifra.ifra_mask);
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(mask);
    sin->sin_len = sizeof(*sin);

    if (changeaddr) {

      /*
       * Interface already exists. Just change the address.
       */
      bcopy(&ifra.ifra_addr, &ifrq.ifr_addr, sizeof(struct sockaddr));
      if (ioctl(s, SIOCSIFADDR, &ifra) < 0)
	LogPrintf(LogERROR, "ioctl(SIFADDR): %s", strerror(errno));
      bcopy(&ifra.ifra_broadaddr, &ifrq.ifr_dstaddr, sizeof(struct sockaddr));
      if (ioctl(s, SIOCSIFDSTADDR, &ifrq) < 0)
	LogPrintf(LogERROR, "ioctl(SIFDSTADDR): %s", strerror(errno));
#ifdef notdef
      bcopy(&ifra.ifra_mask, &ifrq.ifr_broadaddr, sizeof(struct sockaddr));
      if (ioctl(s, SIOCSIFBRDADDR, &ifrq) < 0)
	LogPrintf(LogERROR, "ioctl(SIFBRDADDR): %s", strerror(errno));
#endif
    } else if (ioctl(s, SIOCAIFADDR, &ifra) < 0) {
      LogPrintf(LogERROR, "ioctl(SIOCAIFADDR): %s", strerror(errno));
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

void
OsLinkdown()
{
  char *s;

  if (linkup) {
    s = (char *) inet_ntoa(peer_addr);
    if (LogIsKept(LogLINK))
      LogPrintf(LogLINK, "OsLinkdown: %s\n", s);
    else
      LogPrintf(LogLCP, "OsLinkdown: %s\n", s);

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
    LogPrintf(LogERROR, "socket: %s", strerror(errno));
    return (-1);
  }
  ifrq.ifr_flags &= ~IFF_UP;
  if (ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "ioctl(SIOCSIFFLAGS): %s", strerror(errno));
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
    LogPrintf(LogERROR, "ioctl(TUNSIFINFO): %s", strerror(errno));
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

  bzero((char *) &ifra, sizeof(ifra));
  bzero((char *) &ifrq, sizeof(ifrq));

  strncpy(ifrq.ifr_name, ifname, IFNAMSIZ - 1);
  strncpy(ifra.ifra_name, ifname, IFNAMSIZ - 1);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "socket: %s", strerror(errno));
    return (-1);
  }

  /*
   * Now, bring up the interface.
   */
  if (ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "ioctl(SIOCGIFFLAGS): %s", strerror(errno));
    close(s);
    return (-1);
  }
  ifrq.ifr_flags |= IFF_UP;
  if (ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    LogPrintf(LogERROR, "ioctl(SIOCSIFFLAGS): %s", strerror(errno));
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

void
OsCloseLink(int flag)
{
  HangupModem(flag);
}

void
OsAddInOctets(int cnt)
{
}

void
OsAddOutOctets(int cnt)
{
}

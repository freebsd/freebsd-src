/*
 *	      PPP Routing related Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1994, Internet Initiative Japan, Inc. All rights reserverd.
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
 * $Id: route.c,v 1.15 1997/06/13 03:59:36 brian Exp $
 *
 */
#include <sys/types.h>
#include <machine/endian.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <net/route.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"
#include "loadalias.h"
#include "vars.h"

static int IfIndex;

struct rtmsg {
  struct rt_msghdr m_rtm;
  char m_space[64];
};

static int seqno;

void
OsSetRoute(cmd, dst, gateway, mask)
int cmd;
struct in_addr dst;
struct in_addr gateway;
struct in_addr mask;
{
  struct rtmsg rtmes;
  int s, nb, wb;
  char *cp;
  u_long *lp;
  struct sockaddr_in rtdata;

  s = socket(PF_ROUTE, SOCK_RAW, 0);
  if (s < 0)
    LogPrintf(LogERROR, "socket: %s", strerror(errno));

  bzero(&rtmes, sizeof(rtmes));
  rtmes.m_rtm.rtm_version = RTM_VERSION;
  rtmes.m_rtm.rtm_type = cmd;
  rtmes.m_rtm.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
  rtmes.m_rtm.rtm_seq = ++seqno;
  rtmes.m_rtm.rtm_pid = getpid();
  rtmes.m_rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;

  bzero(&rtdata, sizeof(rtdata));
  rtdata.sin_len = 16;
  rtdata.sin_family = AF_INET;
  rtdata.sin_port = 0;
  rtdata.sin_addr = dst;

  cp = rtmes.m_space;
  bcopy(&rtdata, cp, 16);
  cp += 16;
  if (gateway.s_addr) {
    rtdata.sin_addr = gateway;
    bcopy(&rtdata, cp, 16);
    cp += 16;
  }

  if (dst.s_addr == INADDR_ANY)
    mask.s_addr = INADDR_ANY;

  lp = (u_long *)cp;

  if (mask.s_addr) {
    *lp++ = 8;
    cp += sizeof(int);
    *lp = mask.s_addr;
  } else
    *lp = 0;
  cp += sizeof(u_long);

  nb = cp - (char *)&rtmes;
  rtmes.m_rtm.rtm_msglen = nb;
  wb = write(s, &rtmes, nb);
  if (wb < 0) {
    LogPrintf(LogTCPIP, "OsSetRoute: Dst = %s\n", inet_ntoa(dst));
    LogPrintf(LogTCPIP, "OsSetRoute:  Gateway = %s\n", inet_ntoa(gateway));
    LogPrintf(LogTCPIP, "OsSetRoute:  Mask = %s\n", inet_ntoa(mask));
    switch(rtmes.m_rtm.rtm_errno) {
      case EEXIST:
        LogPrintf(LogTCPIP, "Add route failed: Already exists\n");
        break;
      case ESRCH:
        LogPrintf(LogTCPIP, "Del route failed: Non-existent\n");
        break;
      case ENOBUFS:
      default:
        LogPrintf(LogTCPIP, "Add/Del route failed: %s\n",
                  strerror(rtmes.m_rtm.rtm_errno));
        break;
    }
  }

  LogPrintf(LogDEBUG, "wrote %d: dst = %x, gateway = %x\n", nb,
            dst.s_addr, gateway.s_addr);
  close(s);
}

static void
p_sockaddr(sa, width)
struct sockaddr *sa;
int width;
{
  if (VarTerm) {
    register char *cp;
    register struct sockaddr_in *sin = (struct sockaddr_in *)sa;

    cp = (sin->sin_addr.s_addr == 0) ? "default" :
	   inet_ntoa(sin->sin_addr);
    fprintf(VarTerm, "%-*.*s ", width, width, cp);
  }
}

struct bits {
  short b_mask;
  char  b_val;
} bits[] = {
  { RTF_UP,	  'U' },
  { RTF_GATEWAY,  'G' },
  { RTF_HOST,	  'H' },
  { RTF_DYNAMIC,  'D' },
  { RTF_MODIFIED, 'M' },
  { RTF_CLONING,  'C' },
  { RTF_XRESOLVE, 'X' },
  { RTF_LLINFO,   'L' },
  { RTF_REJECT,   'R' },
  { 0 }
};

static void
p_flags(f, format)
register int f;
char *format;
{
  if (VarTerm) {
    char name[33], *flags;
    register struct bits *p = bits;

    for (flags = name; p->b_mask; p++)
      if (p->b_mask & f)
        *flags++ = p->b_val;
    *flags = '\0';
    fprintf(VarTerm, format, name);
  }
}

int
ShowRoute()
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa;
  char *sp, *ep, *cp;
  u_char *wp;
  int *lp;
  int needed, nb;
  u_long mask;
  int mib[6];

  if (!VarTerm)
    return 1;

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "sysctl: estimate: %s", strerror(errno));
    return(1);
  }

  if (needed < 0)
    return(1);
  sp = malloc(needed);
  if (sp == NULL)
    return(1);
  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "sysctl: getroute: %s", strerror(errno));
    free(sp);
    return(1);
  }

  ep = sp + needed;

  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)cp;
    sa = (struct sockaddr *)(rtm + 1);
    mask = 0xffffffff;
    if (rtm->rtm_addrs == RTA_DST)
      p_sockaddr(sa, 36);
    else {
      wp = (u_char *)cp + rtm->rtm_msglen;
      p_sockaddr(sa, 16);
      if (sa->sa_len == 0)
	sa->sa_len = sizeof(long);
      sa = (struct sockaddr *)(sa->sa_len + (char *)sa);
      p_sockaddr(sa, 18);
      lp = (int *)(sa->sa_len + (char *)sa);
      if ((char *)lp < (char *)wp && *lp) {
	LogPrintf(LogDEBUG, " flag = %x, rest = %d", rtm->rtm_flags, *lp);
	wp = (u_char *)(lp + 1);
	mask = 0;
	for (nb = *(char *)lp; nb > 4; nb--) {
	  mask <<= 8;
	  mask |= *wp++;
	}
	for (nb = 8 - *(char *)lp; nb > 0; nb--)
	  mask <<= 8;
      }
    }
    fprintf(VarTerm, "%08lx  ", mask);
    p_flags(rtm->rtm_flags & (RTF_UP|RTF_GATEWAY|RTF_HOST), "%-6.6s ");
    fprintf(VarTerm, "(%d)\n", rtm->rtm_index);
  }
  free(sp);
  return 0;
}

/*
 *  Delete routes associated with our interface
 */
void
DeleteIfRoutes(all)
int all;
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa;
  struct in_addr dstnet, gateway, maddr;
  int needed;
  char *sp, *cp, *ep;
  u_long mask;
  int *lp, nb;
  u_char *wp;
  int mib[6];

  LogPrintf(LogDEBUG, "DeleteIfRoutes (%d)\n", IfIndex);

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "sysctl: estimate: %s", strerror(errno));
    return;
  }

  if (needed < 0)
    return;

  sp = malloc(needed);
  if (sp == NULL)
    return;

  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "sysctl: getroute: %s", strerror(errno));
    free(sp);
    return;
  }
  ep = sp + needed;

  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)cp;
    sa = (struct sockaddr *)(rtm + 1);
    LogPrintf(LogDEBUG, "DeleteIfRoutes: addrs: %x, index: %d, flags: %x,"
			" dstnet: %s\n",
			rtm->rtm_addrs, rtm->rtm_index, rtm->rtm_flags,
			inet_ntoa(((struct sockaddr_in *)sa)->sin_addr));
    if (rtm->rtm_addrs != RTA_DST &&
       (rtm->rtm_index == IfIndex) &&
       (all || (rtm->rtm_flags & RTF_GATEWAY))) {
      LogPrintf(LogDEBUG, "DeleteIfRoutes: Remove it\n");
      dstnet = ((struct sockaddr_in *)sa)->sin_addr;
      wp = (u_char *)cp + rtm->rtm_msglen;
      if (sa->sa_len == 0)
	sa->sa_len = sizeof(long);
      sa = (struct sockaddr *)(sa->sa_len + (char *)sa);
      gateway = ((struct sockaddr_in *)sa)->sin_addr;
      lp = (int *)(sa->sa_len + (char *)sa);
      mask = 0;
      if ((char *)lp < (char *)wp && *lp) {
	LogPrintf(LogDEBUG, "DeleteIfRoutes: flag = %x, rest = %d",
		  rtm->rtm_flags, *lp);
	wp = (u_char *)(lp + 1);
	for (nb = *lp; nb > 4; nb--) {
	  mask <<= 8;
	  mask |= *wp++;
	}
	for (nb = 8 - *lp; nb > 0; nb--)
	  mask <<= 8;
      }
      LogPrintf(LogDEBUG, "DeleteIfRoutes: Dst: %s\n", inet_ntoa(dstnet));
      LogPrintf(LogDEBUG, "DeleteIfRoutes: Gw: %s\n", inet_ntoa(gateway));
      LogPrintf(LogDEBUG, "DeleteIfRoutes: Index: %d\n", rtm->rtm_index);
      if (dstnet.s_addr == INADDR_ANY)
        mask = INADDR_ANY;
      maddr.s_addr = htonl(mask);
      OsSetRoute(RTM_DELETE, dstnet, gateway, maddr);
    }
  }
  free(sp);
}

 /*
  * 960603 - Modified to use dynamic buffer allocator as in ifconfig
  */

int
GetIfIndex(name)
char *name;
{
  char *buffer;
  struct ifreq *ifrp;
  int s, len, elen, index;
  struct ifconf ifconfs;
  /* struct ifreq reqbuf[256]; -- obsoleted :) */
  int oldbufsize, bufsize = sizeof(struct ifreq);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "socket: %s", strerror(errno));
    return(-1);
  }

  buffer = malloc(bufsize);   /* allocate first buffer */
  ifconfs.ifc_len = bufsize;  /* Initial setting */
  /*
   * Iterate through here until we don't get many more data 
   */

  do {
      oldbufsize = ifconfs.ifc_len;
      bufsize += 1+sizeof(struct ifreq);
      buffer = realloc((void *)buffer, bufsize);      /* Make it bigger */
      LogPrintf(LogDEBUG, "GetIfIndex: Growing buffer to %d\n", bufsize);
      ifconfs.ifc_len = bufsize;
      ifconfs.ifc_buf = buffer;
      if (ioctl(s, SIOCGIFCONF, &ifconfs) < 0) {
          LogPrintf(LogERROR, "ioctl(SIOCGIFCONF): %s", strerror(errno));
          free(buffer);
          return(-1);
      }
  } while (ifconfs.ifc_len > oldbufsize);

  ifrp = ifconfs.ifc_req;

  index = 1;
  for (len = ifconfs.ifc_len; len > 0; len -= sizeof(struct ifreq)) {
    elen = ifrp->ifr_addr.sa_len - sizeof(struct sockaddr);
    if (ifrp->ifr_addr.sa_family == AF_LINK) {
      LogPrintf(LogDEBUG, "GetIfIndex: %d: %-*.*s, %d, %d\n",
		index, IFNAMSIZ, IFNAMSIZ, ifrp->ifr_name,
		ifrp->ifr_addr.sa_family, elen);
      if (strcmp(ifrp->ifr_name, name) == 0) {
        IfIndex = index;
	free(buffer);
        return(index);
      }
      index++;
    }

    len -= elen;
    ifrp = (struct ifreq *)((char *)ifrp + elen);
    ifrp++;
  }

  close(s);
  free(buffer);
  return(-1);
}

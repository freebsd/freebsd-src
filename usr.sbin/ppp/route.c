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
 * $Id: route.c,v 1.26 1997/11/15 02:15:56 brian Exp $
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_dl.h>

#include <errno.h>
#include <machine/endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "defs.h"
#include "vars.h"
#include "id.h"
#include "route.h"

static int IfIndex;

struct rtmsg {
  struct rt_msghdr m_rtm;
  char m_space[64];
};

static int seqno;

void
OsSetRoute(int cmd,
	   struct in_addr dst,
	   struct in_addr gateway,
	   struct in_addr mask)
{
  struct rtmsg rtmes;
  int s, nb, wb;
  char *cp;
  const char *cmdstr;
  u_long *lp;
  struct sockaddr_in rtdata;

  cmdstr = (cmd == RTM_ADD ? "Add" : "Delete");
  s = ID0socket(PF_ROUTE, SOCK_RAW, 0);
  if (s < 0) {
    LogPrintf(LogERROR, "OsSetRoute: socket(): %s\n", strerror(errno));
    return;
  }
  memset(&rtmes, '\0', sizeof(rtmes));
  rtmes.m_rtm.rtm_version = RTM_VERSION;
  rtmes.m_rtm.rtm_type = cmd;
  rtmes.m_rtm.rtm_addrs = RTA_DST | RTA_NETMASK;
  rtmes.m_rtm.rtm_seq = ++seqno;
  rtmes.m_rtm.rtm_pid = getpid();
  rtmes.m_rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;

  memset(&rtdata, '\0', sizeof(rtdata));
  rtdata.sin_len = 16;
  rtdata.sin_family = AF_INET;
  rtdata.sin_port = 0;
  rtdata.sin_addr = dst;

  cp = rtmes.m_space;
  memcpy(cp, &rtdata, 16);
  cp += 16;
  if (gateway.s_addr) {
    rtdata.sin_addr = gateway;
    memcpy(cp, &rtdata, 16);
    cp += 16;
    rtmes.m_rtm.rtm_addrs |= RTA_GATEWAY;
  }
  if (dst.s_addr == INADDR_ANY)
    mask.s_addr = INADDR_ANY;

  lp = (u_long *) cp;

  if (mask.s_addr) {
    *lp++ = 8;
    cp += sizeof(int);
    *lp = mask.s_addr;
  } else
    *lp = 0;
  cp += sizeof(u_long);

  nb = cp - (char *) &rtmes;
  rtmes.m_rtm.rtm_msglen = nb;
  wb = write(s, &rtmes, nb);
  if (wb < 0) {
    LogPrintf(LogTCPIP, "OsSetRoute: Dst = %s\n", inet_ntoa(dst));
    LogPrintf(LogTCPIP, "OsSetRoute:  Gateway = %s\n", inet_ntoa(gateway));
    LogPrintf(LogTCPIP, "OsSetRoute:  Mask = %s\n", inet_ntoa(mask));
    switch (rtmes.m_rtm.rtm_errno) {
    case EEXIST:
      LogPrintf(LogTCPIP, "Add route failed: Already exists\n");
      break;
    case ESRCH:
      LogPrintf(LogTCPIP, "Del route failed: Non-existent\n");
      break;
    case 0:
      LogPrintf(LogTCPIP, "%s route failed: %s\n", cmdstr, strerror(errno));
      break;
    case ENOBUFS:
    default:
      LogPrintf(LogTCPIP, "%s route failed: %s\n",
		cmdstr, strerror(rtmes.m_rtm.rtm_errno));
      break;
    }
  }
  LogPrintf(LogDEBUG, "wrote %d: cmd = %s, dst = %x, gateway = %x\n",
            wb, cmdstr, dst.s_addr, gateway.s_addr);
  close(s);
}

static void
p_sockaddr(struct sockaddr *phost, struct sockaddr *pmask, int width)
{
  char buf[29];
  const char *cp;
  struct sockaddr_in *ihost = (struct sockaddr_in *)phost;
  struct sockaddr_in *mask = (struct sockaddr_in *)pmask;
  struct sockaddr_dl *dl = (struct sockaddr_dl *)phost;

  switch (phost->sa_family) {
  case AF_INET:
    if (!phost)
      cp = "";
    else if (ihost->sin_addr.s_addr == INADDR_ANY)
      cp = "default";
    else if (!mask) 
      cp = inet_ntoa(ihost->sin_addr);
    else {
      u_int msk = ntohl(mask->sin_addr.s_addr);
      u_int tst;
      int bits;
      int len;
      struct sockaddr_in net;

      for (tst = 1, bits=32; tst; tst <<= 1, bits--)
        if (msk & tst)
          break;

      for (tst <<=1; tst; tst <<= 1)
        if (!(msk & tst))
          break;

      net.sin_addr.s_addr = ihost->sin_addr.s_addr & mask->sin_addr.s_addr;
      sprintf(buf, "%s", inet_ntoa(net.sin_addr));
      for (len = strlen(buf); len > 3; buf[len-=2] = '\0')
        if (strcmp(buf+len-2, ".0"))
          break;

      if (tst)    /* non-contiguous :-( */
        sprintf(buf+strlen(buf),"&0x%08x", msk);
      else
        sprintf(buf+strlen(buf), "/%d", bits);
      cp = buf;
    }
    break;

  case AF_LINK:
    if (!dl)
      cp = "";
    else if (dl->sdl_nlen == 0 && dl->sdl_alen == 0 && dl->sdl_slen == 0) {
      sprintf(buf, "link#%d", dl->sdl_index);
      cp = buf;
    } else if (dl->sdl_type == IFT_ETHER && dl->sdl_alen &&
               dl->sdl_alen < sizeof(buf)/3) {
      int f;
      u_char *MAC;

      MAC = (u_char *)dl->sdl_data + dl->sdl_nlen;
      for (f = 0; f < dl->sdl_alen; f++)
        sprintf(buf+f*3, "%02x:", MAC[f]);
      buf[f*3-1] = '\0';
      cp = buf;
    } else
      cp = "???";
    break;

  default:
    cp = "???";
    break;
  }

  fprintf(VarTerm, "%-*s ", width-1, cp);
}

struct bits {
  u_long b_mask;
  char b_val;
} bits[] = {

  { RTF_UP, 'U' },
  { RTF_GATEWAY, 'G' },
  { RTF_HOST, 'H' },
  { RTF_REJECT, 'R' },
  { RTF_DYNAMIC, 'D' },
  { RTF_MODIFIED, 'M' },
  { RTF_DONE, 'd' },
  { RTF_CLONING, 'C' },
  { RTF_XRESOLVE, 'X' },
  { RTF_LLINFO, 'L' },
  { RTF_STATIC, 'S' },
  { RTF_PROTO1, '1' },
  { RTF_PROTO2, '2' },
  { RTF_BLACKHOLE, 'B' },
#ifdef __FreeBSD__
  { RTF_WASCLONED, 'W' },
  { RTF_PRCLONING, 'c' },
  { RTF_PROTO3, '3' },
  { RTF_BROADCAST, 'b' },
#endif
  { 0, '\0' }
};

static void
p_flags(u_long f, const char *format)
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

static const char *
Index2Nam(int idx)
{
  static char ifs[50][6];
  static int nifs;

  if (!nifs) {
    int mib[6], needed, len;
    char *buf, *ptr, *end;
    struct if_msghdr *n;
    struct sockaddr_dl *dl;
    struct if_msghdr *ifm;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
      LogPrintf(LogERROR, "Index2Nam: sysctl: estimate: %s\n", strerror(errno));
      return "???";
    }
    if ((buf = malloc(needed)) == NULL)
      return "???";
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
      free(buf);
      return "???";
    }
    end = buf + needed;

    ptr = buf;
    while (ptr < end) {
      ifm = (struct if_msghdr *)ptr;

      if (ifm->ifm_type != RTM_IFINFO) {
        free(buf);
        return "???";
      }
      dl = (struct sockaddr_dl *)(ifm + 1);
      ptr += ifm->ifm_msglen;
      while (ptr < end) {
        n = (struct if_msghdr *)ptr;
        if (n->ifm_type != RTM_NEWADDR)
          break;
        ptr += n->ifm_msglen;
      }
      if ((len = dl->sdl_nlen) > sizeof(ifs[0])-1)
        len = sizeof(ifs[0])-1;
      strncpy(ifs[nifs], dl->sdl_data, len);
      ifs[nifs++][len] = '\0';
      if (nifs == sizeof(ifs)/sizeof(ifs[0]))
        break;
    }
    free(buf);
  }

#ifdef __FreeBSD__
  idx--;	/* We start at 1, not 0 */
#endif
  if (idx < 0 || idx >= nifs)
    return "???";
  return ifs[idx];
}

int
ShowRoute(struct cmdargs const *arg)
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa_dst, *sa_gw, *sa_mask;
  char *sp, *ep, *cp, *wp;
  int needed;
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
    LogPrintf(LogERROR, "ShowRoute: sysctl: estimate: %s\n", strerror(errno));
    return (1);
  }
  if (needed < 0)
    return (1);
  sp = malloc(needed);
  if (sp == NULL)
    return (1);
  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "ShowRoute: sysctl: getroute: %s\n", strerror(errno));
    free(sp);
    return (1);
  }
  ep = sp + needed;

  fprintf(VarTerm, "%-20s%-20sFlags  Netif\n", "Destination", "Gateway");
  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *) cp;
    wp = (char *)(rtm+1);

    if (rtm->rtm_addrs & RTA_DST) {
      sa_dst = (struct sockaddr *)wp;
      wp += sa_dst->sa_len;
    } else
      sa_dst = NULL;

    if (rtm->rtm_addrs & RTA_GATEWAY) {
      sa_gw = (struct sockaddr *)wp;
      wp += sa_gw->sa_len;
    } else
      sa_gw = NULL;

    if (rtm->rtm_addrs & RTA_NETMASK) {
      sa_mask = (struct sockaddr *)wp;
      wp += sa_mask->sa_len;
    } else
      sa_mask = NULL;

    p_sockaddr(sa_dst, sa_mask, 20);
    p_sockaddr(sa_gw, NULL, 20);

    p_flags(rtm->rtm_flags, "%-6.6s ");
    fprintf(VarTerm, "%s\n", Index2Nam(rtm->rtm_index));
  }
  free(sp);
  return 0;
}

/*
 *  Delete routes associated with our interface
 */
void
DeleteIfRoutes(int all)
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa;
  struct in_addr sa_dst, sa_gw, sa_mask;
  int needed;
  char *sp, *cp, *ep;
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
    LogPrintf(LogERROR, "DeleteIfRoutes: sysctl: estimate: %s\n",
	      strerror(errno));
    return;
  }
  if (needed < 0)
    return;

  sp = malloc(needed);
  if (sp == NULL)
    return;

  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "DeleteIfRoutes: sysctl: getroute: %s\n",
	      strerror(errno));
    free(sp);
    return;
  }
  ep = sp + needed;

  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *) cp;
    sa = (struct sockaddr *) (rtm + 1);
    LogPrintf(LogDEBUG, "DeleteIfRoutes: addrs: %x, Netif: %d (%s), flags: %x,"
	      " dst: %s ?\n", rtm->rtm_addrs, rtm->rtm_index,
              Index2Nam(rtm->rtm_index), rtm->rtm_flags,
	      inet_ntoa(((struct sockaddr_in *) sa)->sin_addr));
    if (rtm->rtm_addrs & RTA_DST && rtm->rtm_addrs & RTA_GATEWAY &&
	rtm->rtm_index == IfIndex &&
	(all || (rtm->rtm_flags & RTF_GATEWAY))) {
      sa_dst.s_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
      wp = (u_char *) cp + rtm->rtm_msglen;
      sa = (struct sockaddr *)((char *)sa + sa->sa_len);
      if (sa->sa_family == AF_INET) {
        LogPrintf(LogDEBUG, "DeleteIfRoutes: Remove it\n");
        sa_gw.s_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
        sa = (struct sockaddr *)((char *)sa + sa->sa_len);
        if (rtm->rtm_addrs & RTA_NETMASK)
          sa_mask.s_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
        else
          sa_mask.s_addr = 0xffffffff;
        if (sa_dst.s_addr == INADDR_ANY)
	  sa_mask.s_addr = INADDR_ANY;
        LogPrintf(LogDEBUG, "DeleteIfRoutes: Dst: %s\n", inet_ntoa(sa_dst));
        LogPrintf(LogDEBUG, "DeleteIfRoutes: Gw: %s\n", inet_ntoa(sa_gw));
        LogPrintf(LogDEBUG, "DeleteIfRoutes: Index: %d\n", rtm->rtm_index);
        OsSetRoute(RTM_DELETE, sa_dst, sa_gw, sa_mask);
      } else
        LogPrintf(LogDEBUG, "DeleteIfRoutes: Can't remove an AF_LINK !\n");
    }
  }
  free(sp);
}

int
GetIfIndex(char *name)
{
  int idx;
  const char *got;

#ifdef __FreeBSD__
  idx = 1;	/* We start at 1, not 0 */
#else
  idx = 0;
#endif

  while (strcmp(got = Index2Nam(idx), "???"))
    if (!strcmp(got, name))
      return IfIndex = idx;
    else
      idx++;
  return -1;
}

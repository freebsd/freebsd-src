/*
 * sys-bsd.c - System-dependent procedures for setting up
 * PPP interfaces on bsd-4.4-ish systems (including 386BSD, NetBSD, etc.)
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: arp.c,v 1.28 1998/05/21 21:43:51 brian Exp $
 *
 */

/*
 * TODO:
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "defs.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "bundle.h"
#include "arp.h"

/*
 * SET_SA_FAMILY - set the sa_family field of a struct sockaddr,
 * if it exists.
 */
#define SET_SA_FAMILY(addr, family)		\
    memset((char *) &(addr), '\0', sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);


#if RTM_VERSION >= 3

/*
 * arp_SetProxy - Make a proxy ARP entry for the peer.
 */
static struct {
  struct rt_msghdr hdr;
  struct sockaddr_inarp dst;
  struct sockaddr_dl hwa;
  char extra[128];
} arpmsg;

static int arpmsg_valid;

int
arp_SetProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  int routes;

  /*
   * Get the hardware address of an interface on the same subnet as our local
   * address.
   */
  memset(&arpmsg, 0, sizeof arpmsg);
  if (!get_ether_addr(s, addr, &arpmsg.hwa)) {
    log_Printf(LogWARN, "Cannot determine ethernet address for proxy ARP\n");
    return 0;
  }
  routes = ID0socket(PF_ROUTE, SOCK_RAW, AF_INET);
  if (routes < 0) {
    log_Printf(LogERROR, "arp_SetProxy: opening routing socket: %s\n",
	      strerror(errno));
    return 0;
  }
  arpmsg.hdr.rtm_type = RTM_ADD;
  arpmsg.hdr.rtm_flags = RTF_ANNOUNCE | RTF_HOST | RTF_STATIC;
  arpmsg.hdr.rtm_version = RTM_VERSION;
  arpmsg.hdr.rtm_seq = ++bundle->routing_seq;
  arpmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
  arpmsg.hdr.rtm_inits = RTV_EXPIRE;
  arpmsg.dst.sin_len = sizeof(struct sockaddr_inarp);
  arpmsg.dst.sin_family = AF_INET;
  arpmsg.dst.sin_addr.s_addr = addr.s_addr;
  arpmsg.dst.sin_other = SIN_PROXY;

  arpmsg.hdr.rtm_msglen = (char *) &arpmsg.hwa - (char *) &arpmsg
    + arpmsg.hwa.sdl_len;
  if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
    log_Printf(LogERROR, "Add proxy arp entry: %s\n", strerror(errno));
    close(routes);
    return 0;
  }
  close(routes);
  arpmsg_valid = 1;
  return 1;
}

/*
 * arp_ClearProxy - Delete the proxy ARP entry for the peer.
 */
int
arp_ClearProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  int routes;

  if (!arpmsg_valid)
    return 0;
  arpmsg_valid = 0;

  arpmsg.hdr.rtm_type = RTM_DELETE;
  arpmsg.hdr.rtm_seq = ++bundle->routing_seq;

  routes = ID0socket(PF_ROUTE, SOCK_RAW, AF_INET);
  if (routes < 0) {
    log_Printf(LogERROR, "arp_SetProxy: opening routing socket: %s\n",
	      strerror(errno));
    return 0;
  }
  if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
    log_Printf(LogERROR, "Delete proxy arp entry: %s\n", strerror(errno));
    close(routes);
    return 0;
  }
  close(routes);
  return 1;
}

#else				/* RTM_VERSION */

/*
 * arp_SetProxy - Make a proxy ARP entry for the peer.
 */
int
arp_SetProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  struct arpreq arpreq;
  struct {
    struct sockaddr_dl sdl;
    char space[128];
  }      dls;

  memset(&arpreq, '\0', sizeof arpreq);

  /*
   * Get the hardware address of an interface on the same subnet as our local
   * address.
   */
  if (!get_ether_addr(s, addr, &dls.sdl)) {
    log_Printf(LOG_PHASE_BIT, "Cannot determine ethernet address for proxy ARP\n");
    return 0;
  }
  arpreq.arp_ha.sa_len = sizeof(struct sockaddr);
  arpreq.arp_ha.sa_family = AF_UNSPEC;
  memcpy(arpreq.arp_ha.sa_data, LLADDR(&dls.sdl), dls.sdl.sdl_alen);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *)&arpreq.arp_pa)->sin_addr.s_addr = addr.s_addr;
  arpreq.arp_flags = ATF_PERM | ATF_PUBL;
  if (ID0ioctl(s, SIOCSARP, (caddr_t) & arpreq) < 0) {
    log_Printf(LogERROR, "arp_SetProxy: ioctl(SIOCSARP): %s\n",
               strerror(errno));
    return 0;
  }
  return 1;
}

/*
 * arp_ClearProxy - Delete the proxy ARP entry for the peer.
 */
int
arp_ClearProxy(struct bundle *bundle, struct in_addr addr, int s)
{
  struct arpreq arpreq;

  memset(&arpreq, '\0', sizeof arpreq);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *)&arpreq.arp_pa)->sin_addr.s_addr = addr.s_addr;
  if (ID0ioctl(s, SIOCDARP, (caddr_t) & arpreq) < 0) {
    log_Printf(LogERROR, "arp_ClearProxy: ioctl(SIOCDARP): %s\n",
               strerror(errno));
    return 0;
  }
  return 1;
}

#endif				/* RTM_VERSION */


/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */

int
get_ether_addr(int s, struct in_addr ipaddr, struct sockaddr_dl *hwaddr)
{
  int mib[6], sa_len, skip, b;
  size_t needed;
  char *buf, *ptr, *end;
  struct if_msghdr *ifm;
  struct ifa_msghdr *ifam;
  struct sockaddr *sa;
  struct sockaddr_dl *dl;
  struct sockaddr_in *ifa, *mask;

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_IFLIST;
  mib[5] = 0;

  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "get_ether_addr: sysctl: estimate: %s\n",
              strerror(errno));
    return 0;
  }

  if ((buf = malloc(needed)) == NULL)
    return 0;

  if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
    free(buf);
    return 0;
  }
  end = buf + needed;

  ptr = buf;
  while (ptr < end) {
    ifm = (struct if_msghdr *)ptr;		/* On if_msghdr */
    if (ifm->ifm_type != RTM_IFINFO)
      break;
    dl = (struct sockaddr_dl *)(ifm + 1);	/* Single _dl at end */
    skip = (ifm->ifm_flags & (IFF_UP | IFF_BROADCAST | IFF_POINTOPOINT |
            IFF_NOARP | IFF_LOOPBACK)) != (IFF_UP | IFF_BROADCAST);
    ptr += ifm->ifm_msglen;			/* First ifa_msghdr */
    while (ptr < end) {
      ifam = (struct ifa_msghdr *)ptr;	/* Next ifa_msghdr (alias) */
      if (ifam->ifam_type != RTM_NEWADDR)	/* finished ? */
        break;
      sa = (struct sockaddr *)(ifam+1);	/* pile of sa's at end */
      ptr += ifam->ifam_msglen;
      if (skip || (ifam->ifam_addrs & (RTA_NETMASK|RTA_IFA)) !=
          (RTA_NETMASK|RTA_IFA))
        continue;
      /* Found a candidate.  Do the addresses match ? */
      if (log_IsKept(LogDEBUG) &&
          ptr == (char *)ifm + ifm->ifm_msglen + ifam->ifam_msglen)
        log_Printf(LogDEBUG, "%.*s interface is a candidate for proxy\n",
                  dl->sdl_nlen, dl->sdl_data);
      b = 1;
      ifa = mask = NULL;
      while (b < (RTA_NETMASK|RTA_IFA) && sa < (struct sockaddr *)ptr) {
        switch (b) {
        case RTA_IFA:
          ifa = (struct sockaddr_in *)sa;
          break;
        case RTA_NETMASK:
          /*
           * Careful here !  this sockaddr doesn't have sa_family set to 
           * AF_INET, and is only 8 bytes big !  I have no idea why !
           */
          mask = (struct sockaddr_in *)sa;
          break;
        }
        if (ifam->ifam_addrs & b) {
#define ALN sizeof(ifa->sin_addr.s_addr)
          sa_len = sa->sa_len > 0 ? ((sa->sa_len-1)|(ALN-1))+1 : ALN;
          sa = (struct sockaddr *)((char *)sa + sa_len);
        }
        b <<= 1;
      }
      if (log_IsKept(LogDEBUG)) {
        char a[16];
        strncpy(a, inet_ntoa(mask->sin_addr), sizeof a - 1);
        a[sizeof a - 1] = '\0';
        log_Printf(LogDEBUG, "Check addr %s, mask %s\n",
                  inet_ntoa(ifa->sin_addr), a);
      }
      if (ifa->sin_family == AF_INET &&
          (ifa->sin_addr.s_addr & mask->sin_addr.s_addr) ==
          (ipaddr.s_addr & mask->sin_addr.s_addr)) {
        log_Printf(LogPHASE, "Found interface %.*s for %s\n",
                  dl->sdl_alen, dl->sdl_data, inet_ntoa(ipaddr));
        memcpy(hwaddr, dl, dl->sdl_len);
        free(buf);
        return 1;
      }
    }
  }
  free(buf);

  return 0;
}

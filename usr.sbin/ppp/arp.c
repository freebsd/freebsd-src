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
 * $Id: arp.c,v 1.21 1998/01/11 04:02:57 brian Exp $
 *
 */

/*
 * TODO:
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <net/if_types.h>
#include <netinet/if_ether.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "route.h"
#include "arp.h"

#ifdef DEBUG
/*
 * To test the proxy arp stuff, put the following in your Makefile:
 * 
 * arp-test: arp.c
 * 	cp ${.CURDIR}/arp.c arp-test.c
 * 	echo 'const char *' >>arp-test.c
 * 	awk '/^Index2Nam/,/^}/' ${.CURDIR}/route.c >>arp-test.c
 * 	cc -I${.CURDIR} -DDEBUG arp-test.c -o arp-test
 *
 * and type ``make arp-test''.
 *
 */
#define LogIsKept(x) 0
#define LogPrintf fprintf
#undef LogDEBUG
#define LogDEBUG stderr
#undef LogERROR
#define LogERROR stderr
#undef LogPHASE
#define LogPHASE stdout
#define ID0socket socket
#define ID0ioctl ioctl
#endif

static int rtm_seq;

static int get_ether_addr(int, struct in_addr, struct sockaddr_dl *);

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
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
static struct {
  struct rt_msghdr hdr;
  struct sockaddr_inarp dst;
  struct sockaddr_dl hwa;
  char extra[128];
} arpmsg;

static int arpmsg_valid;

int
sifproxyarp(int unit, struct in_addr hisaddr)
{
  int routes;

  /*
   * Get the hardware address of an interface on the same subnet as our local
   * address.
   */
  memset(&arpmsg, 0, sizeof arpmsg);
  if (!get_ether_addr(unit, hisaddr, &arpmsg.hwa)) {
    LogPrintf(LogERROR, "Cannot determine ethernet address for proxy ARP\n");
    return 0;
  }
  routes = ID0socket(PF_ROUTE, SOCK_RAW, AF_INET);
  if (routes < 0) {
    LogPrintf(LogERROR, "sifproxyarp: opening routing socket: %s\n",
	      strerror(errno));
    return 0;
  }
  arpmsg.hdr.rtm_type = RTM_ADD;
  arpmsg.hdr.rtm_flags = RTF_ANNOUNCE | RTF_HOST | RTF_STATIC;
  arpmsg.hdr.rtm_version = RTM_VERSION;
  arpmsg.hdr.rtm_seq = ++rtm_seq;
  arpmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
  arpmsg.hdr.rtm_inits = RTV_EXPIRE;
  arpmsg.dst.sin_len = sizeof(struct sockaddr_inarp);
  arpmsg.dst.sin_family = AF_INET;
  arpmsg.dst.sin_addr.s_addr = hisaddr.s_addr;
  arpmsg.dst.sin_other = SIN_PROXY;

  arpmsg.hdr.rtm_msglen = (char *) &arpmsg.hwa - (char *) &arpmsg
    + arpmsg.hwa.sdl_len;
  if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
    LogPrintf(LogERROR, "Add proxy arp entry: %s\n", strerror(errno));
    close(routes);
    return 0;
  }
  close(routes);
  arpmsg_valid = 1;
  return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(int unit, struct in_addr hisaddr)
{
  int routes;

  if (!arpmsg_valid)
    return 0;
  arpmsg_valid = 0;

  arpmsg.hdr.rtm_type = RTM_DELETE;
  arpmsg.hdr.rtm_seq = ++rtm_seq;

  routes = ID0socket(PF_ROUTE, SOCK_RAW, AF_INET);
  if (routes < 0) {
    LogPrintf(LogERROR, "sifproxyarp: opening routing socket: %s\n",
	      strerror(errno));
    return 0;
  }
  if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
    LogPrintf(LogERROR, "Delete proxy arp entry: %s\n", strerror(errno));
    close(routes);
    return 0;
  }
  close(routes);
  return 1;
}

#else				/* RTM_VERSION */

/*
 * sifproxyarp - Make a proxy ARP entry for the peer.
 */
int
sifproxyarp(int unit, struct in_addr hisaddr)
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
  if (!get_ether_addr(unit, hisaddr, &dls.sdl)) {
    LogPrintf(LOG_PHASE_BIT, "Cannot determine ethernet address for proxy ARP\n");
    return 0;
  }
  arpreq.arp_ha.sa_len = sizeof(struct sockaddr);
  arpreq.arp_ha.sa_family = AF_UNSPEC;
  memcpy(arpreq.arp_ha.sa_data, LLADDR(&dls.sdl), dls.sdl.sdl_alen);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *) & arpreq.arp_pa)->sin_addr.s_addr = hisaddr.s_addr;
  arpreq.arp_flags = ATF_PERM | ATF_PUBL;
  if (ID0ioctl(unit, SIOCSARP, (caddr_t) & arpreq) < 0) {
    LogPrintf(LogERROR, "sifproxyarp: ioctl(SIOCSARP): %s\n", strerror(errno));
    return 0;
  }
  return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(int unit, struct in_addr hisaddr)
{
  struct arpreq arpreq;

  memset(&arpreq, '\0', sizeof arpreq);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *) & arpreq.arp_pa)->sin_addr.s_addr = hisaddr.s_addr;
  if (ID0ioctl(unit, SIOCDARP, (caddr_t) & arpreq) < 0) {
    LogPrintf(LogERROR, "cifproxyarp: ioctl(SIOCDARP): %s\n", strerror(errno));
    return 0;
  }
  return 1;
}

#endif				/* RTM_VERSION */


/*
 * get_ether_addr - get the hardware address of an interface on the
 * the same subnet as ipaddr.
 */
#define MAX_IFS		32

static int
get_ether_addr(int s, struct in_addr ipaddr, struct sockaddr_dl *hwaddr)
{
  int idx;
  const char *got;
  char *sp, *ep, *cp, *wp;
  struct ifreq ifrq;
  struct in_addr addr, mask;
  struct rt_msghdr *rtm;
  struct sockaddr *sa_dst, *sa_gw;
  struct sockaddr_dl *dl;
  size_t needed;
  int mib[6];

  idx = 1;
  while (strcmp(got = Index2Nam(idx), "???")) {
    strncpy(ifrq.ifr_name, got, sizeof ifrq.ifr_name - 1);
    ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
    if (ID0ioctl(s, SIOCGIFADDR, &ifrq) == 0 &&
        ifrq.ifr_addr.sa_family == AF_INET) {
      addr = ((struct sockaddr_in *)&ifrq.ifr_addr)->sin_addr;
      if (ID0ioctl(s, SIOCGIFNETMASK, &ifrq) == 0) {
        mask = ((struct sockaddr_in *)&ifrq.ifr_broadaddr)->sin_addr;
        if ((ipaddr.s_addr & mask.s_addr) == (addr.s_addr & mask.s_addr))
          break;
      }
    }
    idx++;
  }

  if (!strcmp(got, "???"))
    return 0;

  LogPrintf(LogPHASE, "Found interface %s for proxy arp\n", got);

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "get_ether_addr: sysctl: estimate: %s\n",
              strerror(errno));
    return 0;
  }
  if (needed < 0)
    return 0;
  if ((sp = malloc(needed)) == NULL)
    return 0;
  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    LogPrintf(LogERROR, "ShowRoute: sysctl: getroute: %s\n", strerror(errno));
    free(sp);
    return (1);
  }
  ep = sp + needed;

  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *) cp;
    if (rtm->rtm_index == idx) {
      wp = (char *)(rtm+1);

      if (rtm->rtm_addrs & RTA_DST) {
        sa_dst = (struct sockaddr *)wp;
        wp += sa_dst->sa_len;
      } else
        sa_dst = NULL;

      if (rtm->rtm_addrs & RTA_GATEWAY) {
        sa_gw = (struct sockaddr *)wp;
        if (sa_gw->sa_family == AF_LINK) {
          dl = (struct sockaddr_dl *)wp;
          if (dl->sdl_alen && dl->sdl_type == IFT_ETHER) {
            memcpy(hwaddr, dl, dl->sdl_len);
            free(sp);
            return 1;
          }
        }
      }
    }
  }
  free(sp);
  return 0;
}

#ifdef DEBUG
int
main(int argc, char **argv)
{
  struct in_addr ipaddr;
  int s, f;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  for (f = 1; f < argc; f++) {
    if (inet_aton(argv[f], &ipaddr))
      sifproxyarp(s, ipaddr);
  }
  close(s);
}
#endif

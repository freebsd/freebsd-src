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
 * $Id: arp.c,v 1.5.2.4 1997/08/31 23:01:56 brian Exp $
 *
 */

/*
 * TODO:
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef __bsdi__
#include <kvm.h>
#endif
#include <net/if_types.h>
#include <netinet/in_var.h>
#if RTM_VERSION >= 3
#include <netinet/if_ether.h>
#endif
#include "log.h"

static int rtm_seq;

static int get_ether_addr(int, u_long, struct sockaddr_dl *);

#define BCOPY(s, d, l)		memcpy(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)
/*
 * SET_SA_FAMILY - set the sa_family field of a struct sockaddr,
 * if it exists.
 */
#define SET_SA_FAMILY(addr, family)		\
    BZERO((char *) &(addr), sizeof(addr));	\
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
}      arpmsg;

static int arpmsg_valid;

int
sifproxyarp(int unit, u_long hisaddr)
{
  int routes;

  /*
   * Get the hardware address of an interface on the same subnet as our local
   * address.
   */
  memset(&arpmsg, 0, sizeof(arpmsg));
  if (!get_ether_addr(unit, hisaddr, &arpmsg.hwa)) {
    LogPrintf(LogERROR, "Cannot determine ethernet address for proxy ARP\n");
    return 0;
  }
  if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
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
  arpmsg.dst.sin_addr.s_addr = hisaddr;
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
cifproxyarp(int unit, u_long hisaddr)
{
  int routes;

  if (!arpmsg_valid)
    return 0;
  arpmsg_valid = 0;

  arpmsg.hdr.rtm_type = RTM_DELETE;
  arpmsg.hdr.rtm_seq = ++rtm_seq;

  if ((routes = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
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
sifproxyarp(int unit, u_long hisaddr)
{
  struct arpreq arpreq;
  struct {
    struct sockaddr_dl sdl;
    char space[128];
  }      dls;

  BZERO(&arpreq, sizeof(arpreq));

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
  BCOPY(LLADDR(&dls.sdl), arpreq.arp_ha.sa_data, dls.sdl.sdl_alen);
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *) & arpreq.arp_pa)->sin_addr.s_addr = hisaddr;
  arpreq.arp_flags = ATF_PERM | ATF_PUBL;
  if (ioctl(unit, SIOCSARP, (caddr_t) & arpreq) < 0) {
    LogPrintf(LogERROR, "sifproxyarp: ioctl(SIOCSARP): %s\n", strerror(errno));
    return 0;
  }
  return 1;
}

/*
 * cifproxyarp - Delete the proxy ARP entry for the peer.
 */
int
cifproxyarp(int unit, u_long hisaddr)
{
  struct arpreq arpreq;

  BZERO(&arpreq, sizeof(arpreq));
  SET_SA_FAMILY(arpreq.arp_pa, AF_INET);
  ((struct sockaddr_in *) & arpreq.arp_pa)->sin_addr.s_addr = hisaddr;
  if (ioctl(unit, SIOCDARP, (caddr_t) & arpreq) < 0) {
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

int
get_ether_addr(int s, u_long ipaddr, struct sockaddr_dl * hwaddr)
{
  struct ifreq *ifr, *ifend, *ifp;
  u_long ina, mask;
  struct sockaddr_dl *dla;
  struct ifreq ifreq;
  struct ifconf ifc;
  struct ifreq ifs[MAX_IFS];

  ifc.ifc_len = sizeof(ifs);
  ifc.ifc_req = ifs;
  if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
    LogPrintf(LogERROR, "get_ether_addr: ioctl(SIOCGIFCONF): %s\n",
	      strerror(errno));
    return 0;
  }

  /*
   * Scan through looking for an interface with an Internet address on the
   * same subnet as `ipaddr'.
   */
  ifend = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
  for (ifr = ifc.ifc_req; ifr < ifend;) {
    if (ifr->ifr_addr.sa_family == AF_INET) {
      ina = ((struct sockaddr_in *) & ifr->ifr_addr)->sin_addr.s_addr;
      strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
      ifreq.ifr_name[sizeof(ifreq.ifr_name) - 1] = '\0';

      /*
       * Check that the interface is up, and not point-to-point or loopback.
       */
      if (ioctl(s, SIOCGIFFLAGS, &ifreq) < 0)
	continue;
      if ((ifreq.ifr_flags &
      (IFF_UP | IFF_BROADCAST | IFF_POINTOPOINT | IFF_LOOPBACK | IFF_NOARP))
	  != (IFF_UP | IFF_BROADCAST))
	goto nextif;

      /*
       * Get its netmask and check that it's on the right subnet.
       */
      if (ioctl(s, SIOCGIFNETMASK, &ifreq) < 0)
	continue;
      mask = ((struct sockaddr_in *) & ifreq.ifr_addr)->sin_addr.s_addr;
      if ((ipaddr & mask) != (ina & mask))
	goto nextif;

      break;
    }
nextif:
    ifr = (struct ifreq *) ((char *) &ifr->ifr_addr + ifr->ifr_addr.sa_len);
  }

  if (ifr >= ifend)
    return 0;
  LogPrintf(LogPHASE, "Found interface %s for proxy arp\n", ifr->ifr_name);

  /*
   * Now scan through again looking for a link-level address for this
   * interface.
   */
  ifp = ifr;
  for (ifr = ifc.ifc_req; ifr < ifend;) {
    if (strcmp(ifp->ifr_name, ifr->ifr_name) == 0
	&& ifr->ifr_addr.sa_family == AF_LINK) {

      /*
       * Found the link-level address - copy it out
       */
      dla = (struct sockaddr_dl *) & ifr->ifr_addr;
#ifdef __bsdi__
      if (dla->sdl_alen == 0)
	kmemgetether(ifr->ifr_name, dla);
#endif
      BCOPY(dla, hwaddr, dla->sdl_len);
      return 1;
    }
    ifr = (struct ifreq *) ((char *) &ifr->ifr_addr + ifr->ifr_addr.sa_len);
  }

  return 0;
}

#ifdef __bsdi__
#include <nlist.h>

struct nlist nl[] = {
#define N_IFNET		0
  {"_ifnet"},
  "",
};


kvm_t *kvmd;

/*
 * Read kernel memory, return 0 on success.
 */
int
kread(u_long addr, char *buf, int size)
{
  if (kvm_read(kvmd, addr, buf, size) != size) {
    /* XXX this duplicates kvm_read's error printout */
    LogPrintf(LogERROR, "kvm_read %s\n", kvm_geterr(kvmd));
    return -1;
  }
  return 0;
}

void
kmemgetether(char *ifname, struct sockaddr_dl * dlo)
{
  struct ifnet ifnet;
  int n;
  u_long addr, ifaddraddr, ifnetfound, ifaddrfound;
  char name[16 + 32];
  struct sockaddr *sa;
  char *cp;
  struct sockaddr_dl *sdl;
  union {
    struct ifaddr ifa;
    struct in_ifaddr in;
  }     ifaddr;
  struct arpcom ac;

  kvmd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL);
  if (kvmd) {
    n = kvm_nlist(kvmd, nl);
    if (n >= 0) {
      addr = nl[N_IFNET].n_value;
      kread(addr, (char *) &addr, sizeof(addr));
      ifaddraddr = ifnetfound = 0;
      while (addr || ifaddraddr) {
	ifnetfound = addr;
	if (ifaddraddr == 0) {
	  if (kread(addr, (char *) &ifnet, sizeof(ifnet)) ||
	      kread((u_long) ifnet.if_name, name, 16))
	    return;
	  name[15] = 0;
	  addr = (u_long) ifnet.if_next;
	  cp = (char *) index(name, '\0');
	  cp += sprintf(cp, "%d", ifnet.if_unit);
	  *cp = '\0';
	  ifaddraddr = (u_long) ifnet.if_addrlist;
	}
	ifaddrfound = ifaddraddr;
	if (ifaddraddr) {
	  if (kread(ifaddraddr, (char *) &ifaddr, sizeof ifaddr)) {
	    ifaddraddr = 0;
	    continue;
	  }
#define CP(x) ((char *)(x))
	  cp = (CP(ifaddr.ifa.ifa_addr) - CP(ifaddraddr)) + CP(&ifaddr);
	  sa = (struct sockaddr *) cp;
	  if (sa->sa_family == AF_LINK && strcmp(ifname, name) == 0) {
	    sdl = (struct sockaddr_dl *) sa;
	    cp = (char *) LLADDR(sdl);
	    n = sdl->sdl_alen;
	    if (ifnet.if_type == IFT_ETHER) {
	      if (n == 0) {
		kread(ifnetfound, (char *) &ac, sizeof(ac));
		cp = (char *) LLADDR(sdl);
		bcopy((char *) ac.ac_enaddr, cp, 6);
		sdl->sdl_alen = 6;
	      }
	      bcopy(sdl, dlo, sizeof(*sdl));
	      return;
	    }
	  }
	  ifaddraddr = (u_long) ifaddr.ifa.ifa_next;
	}
      }
    }
  }
}

#endif

#ifdef DEBUG
main()
{
  u_long ipaddr;
  int s;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  ipaddr = inet_addr("192.168.1.32");
  sifproxyarp(s, ipaddr);
  close(s);
}

#endif

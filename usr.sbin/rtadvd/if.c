/*	$FreeBSD$	*/
/*	$KAME: if.c,v 1.17 2001/01/21 15:27:30 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_types.h>
#ifdef __FreeBSD__
# include <net/ethernet.h>
#endif
#include <ifaddrs.h>
#ifdef __NetBSD__
#include <net/if_ether.h>
#endif
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#ifdef __bsdi__
# include <netinet/if_ether.h>
#endif
#ifdef __OpenBSD__
#include <netinet/if_ether.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "rtadvd.h"
#include "if.h"

#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((caddr_t)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(u_long)) :\
			  			 sizeof(u_long)))

struct if_msghdr **iflist;
int iflist_init_ok;
size_t ifblock_size;
char *ifblock;

static void get_iflist __P((char **buf, size_t *size));
static void parse_iflist __P((struct if_msghdr ***ifmlist_p, char *buf,
		       size_t bufsize));

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int i;
	
	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			NEXT_SA(sa);
		}
		else
			rti_info[i] = NULL;
	}
}

struct sockaddr_dl *
if_nametosdl(char *name)
{
	int mib[6] = {CTL_NET, AF_ROUTE, 0, 0, NET_RT_IFLIST, 0};
	char *buf, *next, *lim;
	size_t len;
	struct if_msghdr *ifm;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	struct sockaddr_dl *sdl = NULL, *ret_sdl;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
		return(NULL);
	if ((buf = malloc(len)) == NULL)
		return(NULL);
	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		free(buf);
		return(NULL);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type == RTM_IFINFO) {
			sa = (struct sockaddr *)(ifm + 1);
			get_rtaddrs(ifm->ifm_addrs, sa, rti_info);
			if ((sa = rti_info[RTAX_IFP]) != NULL) {
				if (sa->sa_family == AF_LINK) {
					sdl = (struct sockaddr_dl *)sa;
					if (strlen(name) != sdl->sdl_nlen)
						continue; /* not same len */
					if (strncmp(&sdl->sdl_data[0],
						    name,
						    sdl->sdl_nlen) == 0) {
						break;
					}
				}
			}
		}
	}
	if (next == lim) {
		/* search failed */
		free(buf);
		return(NULL);
	}

	if ((ret_sdl = malloc(sdl->sdl_len)) == NULL)
		return(NULL);
	memcpy((caddr_t)ret_sdl, (caddr_t)sdl, sdl->sdl_len);
	return(ret_sdl);
}

int
if_getmtu(char *name)
{
	struct ifaddrs *ifap, *ifa;
	struct if_data *ifd;
	u_long mtu = 0;

	if (getifaddrs(&ifap) < 0)
		return(0);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, name) == 0) {
			ifd = ifa->ifa_data;
			if (ifd)
				mtu = ifd->ifi_mtu;
			break;
		}
	}
	freeifaddrs(ifap);

#ifdef SIOCGIFMTU		/* XXX: this ifdef may not be necessary */
	if (mtu == 0) {
		struct ifreq ifr;
		int s;

		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			return(0);

		ifr.ifr_addr.sa_family = AF_INET6;
		strncpy(ifr.ifr_name, name,
			sizeof(ifr.ifr_name));
		if (ioctl(s, SIOCGIFMTU, (caddr_t)&ifr) < 0) {
			close(s);
			return(0);
		}
		close(s);

		mtu = ifr.ifr_mtu;
	}
#endif

	return(mtu);
}

/* give interface index and its old flags, then new flags returned */
int
if_getflags(int ifindex, int oifflags)
{
	struct ifreq ifr;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %s", __func__,
		       strerror(errno));
		return (oifflags & ~IFF_UP);
	}

	if_indextoname(ifindex, ifr.ifr_name);
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "<%s> ioctl:SIOCGIFFLAGS: failed for %s",
		       __func__, ifr.ifr_name);
		close(s);
		return (oifflags & ~IFF_UP);
	}
	close(s);
	return (ifr.ifr_flags);
}

#define ROUNDUP8(a) (1 + (((a) - 1) | 7))
int
lladdropt_length(struct sockaddr_dl *sdl)
{
	switch (sdl->sdl_type) {
	case IFT_ETHER:
		return(ROUNDUP8(ETHER_ADDR_LEN + 2));
	default:
		return(0);
	}
}

void
lladdropt_fill(struct sockaddr_dl *sdl, struct nd_opt_hdr *ndopt)
{
	char *addr;

	ndopt->nd_opt_type = ND_OPT_SOURCE_LINKADDR; /* fixed */

	switch (sdl->sdl_type) {
	case IFT_ETHER:
		ndopt->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
		addr = (char *)(ndopt + 1);
		memcpy(addr, LLADDR(sdl), ETHER_ADDR_LEN);
		break;
	default:
		syslog(LOG_ERR, "<%s> unsupported link type(%d)",
		    __func__, sdl->sdl_type);
		exit(1);
	}

	return;
}

int
rtbuf_len()
{
	size_t len;

	int mib[6] = {CTL_NET, AF_ROUTE, 0, AF_INET6, NET_RT_DUMP, 0};

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
		return(-1);

	return(len);
}

#define FILTER_MATCH(type, filter) ((0x1 << type) & filter)
#define SIN6(s) ((struct sockaddr_in6 *)(s))
#define SDL(s) ((struct sockaddr_dl *)(s))
char *
get_next_msg(char *buf, char *lim, int ifindex, size_t *lenp, int filter)
{
	struct rt_msghdr *rtm;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa, *dst, *gw, *ifa, *rti_info[RTAX_MAX];

	*lenp = 0;
	for (rtm = (struct rt_msghdr *)buf;
	     rtm < (struct rt_msghdr *)lim;
	     rtm = (struct rt_msghdr *)(((char *)rtm) + rtm->rtm_msglen)) {
		/* just for safety */
		if (!rtm->rtm_msglen) {
			syslog(LOG_WARNING, "<%s> rtm_msglen is 0 "
				"(buf=%p lim=%p rtm=%p)", __func__,
				buf, lim, rtm);
			break;
		}
		if (FILTER_MATCH(rtm->rtm_type, filter) == 0) {
			continue;
		}

		switch (rtm->rtm_type) {
		case RTM_GET:
		case RTM_ADD:
		case RTM_DELETE:
			/* address related checks */
			sa = (struct sockaddr *)(rtm + 1);
			get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
			if ((dst = rti_info[RTAX_DST]) == NULL ||
			    dst->sa_family != AF_INET6)
				continue;

			if (IN6_IS_ADDR_LINKLOCAL(&SIN6(dst)->sin6_addr) ||
			    IN6_IS_ADDR_MULTICAST(&SIN6(dst)->sin6_addr))
				continue;

			if ((gw = rti_info[RTAX_GATEWAY]) == NULL ||
			    gw->sa_family != AF_LINK)
				continue;
			if (ifindex && SDL(gw)->sdl_index != ifindex)
				continue;

			if (rti_info[RTAX_NETMASK] == NULL)
				continue;

			/* found */
			*lenp = rtm->rtm_msglen;
			return (char *)rtm;
			/* NOTREACHED */
		case RTM_NEWADDR:
		case RTM_DELADDR:
			ifam = (struct ifa_msghdr *)rtm;

			/* address related checks */
			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, rti_info);
			if ((ifa = rti_info[RTAX_IFA]) == NULL ||
			    (ifa->sa_family != AF_INET &&
			     ifa->sa_family != AF_INET6))
				continue;

			if (ifa->sa_family == AF_INET6 &&
			    (IN6_IS_ADDR_LINKLOCAL(&SIN6(ifa)->sin6_addr) ||
			     IN6_IS_ADDR_MULTICAST(&SIN6(ifa)->sin6_addr)))
				continue;

			if (ifindex && ifam->ifam_index != ifindex)
				continue;

			/* found */
			*lenp = ifam->ifam_msglen;
			return (char *)rtm;
			/* NOTREACHED */
		case RTM_IFINFO:
			/* found */
			*lenp = rtm->rtm_msglen;
			return (char *)rtm;
			/* NOTREACHED */
		}
	}

	return (char *)rtm;
}
#undef FILTER_MATCH

struct in6_addr *
get_addr(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	sa = (struct sockaddr *)(rtm + 1);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	return(&SIN6(rti_info[RTAX_DST])->sin6_addr);
}

int
get_rtm_ifindex(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	sa = (struct sockaddr *)(rtm + 1);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	return(((struct sockaddr_dl *)rti_info[RTAX_GATEWAY])->sdl_index);
}

int
get_ifm_ifindex(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return ((int)ifm->ifm_index);
}

int
get_ifam_ifindex(char *buf)
{
	struct ifa_msghdr *ifam = (struct ifa_msghdr *)buf;

	return ((int)ifam->ifam_index);
}

int
get_ifm_flags(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return (ifm->ifm_flags);
}

int
get_prefixlen(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	u_char *p, *lim;
	
	sa = (struct sockaddr *)(rtm + 1);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
	sa = rti_info[RTAX_NETMASK];

	p = (u_char *)(&SIN6(sa)->sin6_addr);
	lim = (u_char *)sa + sa->sa_len;
	return prefixlen(p, lim);
}

int
prefixlen(u_char *p, u_char *lim)
{
	int masklen;

	for (masklen = 0; p < lim; p++) {
		switch (*p) {
		case 0xff:
			masklen += 8;
			break;
		case 0xfe:
			masklen += 7;
			break;
		case 0xfc:
			masklen += 6;
			break;
		case 0xf8:
			masklen += 5;
			break;
		case 0xf0:
			masklen += 4;
			break;
		case 0xe0:
			masklen += 3;
			break;
		case 0xc0:
			masklen += 2;
			break;
		case 0x80:
			masklen += 1;
			break;
		case 0x00:
			break;
		default:
			return(-1);
		}
	}

	return(masklen);
}

int
rtmsg_type(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return(rtm->rtm_type);
}

int
rtmsg_len(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return(rtm->rtm_msglen);
}

int
ifmsg_len(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return(ifm->ifm_msglen);
}

/*
 * alloc buffer and get if_msghdrs block from kernel,
 * and put them into the buffer
 */
static void
get_iflist(char **buf, size_t *size)
{
	int mib[6];

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, size, NULL, 0) < 0) {
		syslog(LOG_ERR, "<%s> sysctl: iflist size get failed",
		       __func__);
		exit(1);
	}
	if ((*buf = malloc(*size)) == NULL) {
		syslog(LOG_ERR, "<%s> malloc failed", __func__);
		exit(1);
	}
	if (sysctl(mib, 6, *buf, size, NULL, 0) < 0) {
		syslog(LOG_ERR, "<%s> sysctl: iflist get failed",
		       __func__);
		exit(1);
	}
	return;
}

/*
 * alloc buffer and parse if_msghdrs block passed as arg,
 * and init the buffer as list of pointers ot each of the if_msghdr.
 */
static void
parse_iflist(struct if_msghdr ***ifmlist_p, char *buf, size_t bufsize)
{
	int iflentry_size, malloc_size;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	char *lim;

	/*
	 * Estimate least size of an iflist entry, to be obtained from kernel.
	 * Should add sizeof(sockaddr) ??
	 */
	iflentry_size = sizeof(struct if_msghdr);
	/* roughly estimate max list size of pointers to each if_msghdr */
	malloc_size = (bufsize/iflentry_size) * sizeof(size_t);
	if ((*ifmlist_p = (struct if_msghdr **)malloc(malloc_size)) == NULL) {
		syslog(LOG_ERR, "<%s> malloc failed", __func__);
		exit(1);
	}

	lim = buf + bufsize;
	for (ifm = (struct if_msghdr *)buf; ifm < (struct if_msghdr *)lim;) {
		if (ifm->ifm_msglen == 0) {
			syslog(LOG_WARNING, "<%s> ifm_msglen is 0 "
			       "(buf=%p lim=%p ifm=%p)", __func__,
			       buf, lim, ifm);
			return;
		}

		if (ifm->ifm_type == RTM_IFINFO) {
			(*ifmlist_p)[ifm->ifm_index] = ifm;
		} else {
			syslog(LOG_ERR, "out of sync parsing NET_RT_IFLIST\n"
			       "expected %d, got %d\n msglen = %d\n"
			       "buf:%p, ifm:%p, lim:%p\n",
			       RTM_IFINFO, ifm->ifm_type, ifm->ifm_msglen,
			       buf, ifm, lim);
			exit (1);
		}
		for (ifam = (struct ifa_msghdr *)
			((char *)ifm + ifm->ifm_msglen);
		     ifam < (struct ifa_msghdr *)lim;
		     ifam = (struct ifa_msghdr *)
		     	((char *)ifam + ifam->ifam_msglen)) {
			/* just for safety */
			if (!ifam->ifam_msglen) {
				syslog(LOG_WARNING, "<%s> ifa_msglen is 0 "
				       "(buf=%p lim=%p ifam=%p)", __func__,
				       buf, lim, ifam);
				return;
			}
			if (ifam->ifam_type != RTM_NEWADDR)
				break;
		}
		ifm = (struct if_msghdr *)ifam;
	}
}

void
init_iflist()
{
	if (ifblock) {
		free(ifblock);
		ifblock_size = 0;
	}
	if (iflist)
		free(iflist);
	/* get iflist block from kernel */
	get_iflist(&ifblock, &ifblock_size);

	/* make list of pointers to each if_msghdr */
	parse_iflist(&iflist, ifblock, ifblock_size);
}

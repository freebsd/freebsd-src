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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <netinet6/in6_var.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <kvm.h>
#include <nlist.h>
#include <limits.h>

#include "rtsold.h"

static int ifsock;

static int getifa __P((char *name, struct in6_ifaddr *ifap));
static void get_rtaddrs __P((int addrs, struct sockaddr *sa,
			     struct sockaddr **rti_info));

int
ifinit()
{
	if ((ifsock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "socket: %s", strerror(errno));
		return(-1);
	}

	return(0);
}

int
interface_up(char *name)
{
	struct ifreq ifr;
	struct in6_ifaddr ifa;

	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	if (ioctl(ifsock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warnmsg(LOG_WARNING, __FUNCTION__, "ioctl(SIOCGIFFLAGS): %s",
		       strerror(errno));
		return(-1);
	}
	if (!(ifr.ifr_flags & IFF_UP)) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(ifsock, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
			warnmsg(LOG_ERR, __FUNCTION__,
				"ioctl(SIOCSIFFLAGS): %s", strerror(errno));
		}
		return(-1);
	}

	warnmsg(LOG_DEBUG, __FUNCTION__, "checking if %s is ready...", name);

	if (getifa(name, &ifa) < 0) {
		warnmsg(LOG_WARNING, __FUNCTION__,
			"getifa() failed, anyway I'll try");
		return 0;
	}

	if (!(ifa.ia6_flags & IN6_IFF_NOTREADY)) {
		warnmsg(LOG_DEBUG, __FUNCTION__,
			"%s is ready", name);
		return(0);
	}
	else {
		if (ifa.ia6_flags & IN6_IFF_TENTATIVE) {
			warnmsg(LOG_DEBUG, __FUNCTION__, "%s is tentative",
			       name);
			return IFS_TENTATIVE;
		}
		if (ifa.ia6_flags & IN6_IFF_DUPLICATED)
			warnmsg(LOG_DEBUG, __FUNCTION__, "%s is duplicated",
			       name);
		return -1;
	}
}

int
interface_status(struct ifinfo *ifinfo)
{
	char *ifname = ifinfo->ifname;
	struct ifreq ifr;
	struct ifmediareq ifmr;

	/* get interface flags */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(ifsock, SIOCGIFFLAGS, &ifr) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "ioctl(SIOCGIFFLAGS) on %s: %s",
		       ifname, strerror(errno));
		return(-1);
	}
	/*
	 * if one of UP and RUNNING flags is dropped,
	 * the interface is not active.
	 */
	if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		goto inactive;
	}

	/* Next, check carrier on the interface, if possible */
	if (!ifinfo->mediareqok)
		goto active;
	memset(&ifmr, 0, sizeof(ifmr));
	strncpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));

	if (ioctl(ifsock, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		if (errno != EINVAL) {
			warnmsg(LOG_DEBUG, __FUNCTION__,
				"ioctl(SIOCGIFMEDIA) on %s: %s",
			       ifname, strerror(errno));
			return(-1);
		}
		/*
		 * EINVAL simply means that the interface does not support
		 * the SIOCGIFMEDIA ioctl. We regard it alive.
		 */
		ifinfo->mediareqok = 0;
		goto active;
	}

	if (ifmr.ifm_status & IFM_AVALID) {
		switch(ifmr.ifm_active & IFM_NMASK) {
		 case IFM_ETHER:
			 if (ifmr.ifm_status & IFM_ACTIVE)
				 goto active;
			 else
				 goto inactive;
			 break;
		 default:
			 goto inactive;
		}
	}

  inactive:
	return(0);

  active:
	return(1);
}

#define	ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define	NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((caddr_t)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(u_long)) :\
			  			 sizeof(u_long)))
#define	ROUNDUP8(a) (1 + (((a) - 1) | 7))

int
lladdropt_length(struct sockaddr_dl *sdl)
{
	switch(sdl->sdl_type) {
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

	switch(sdl->sdl_type) {
	 case IFT_ETHER:
		 ndopt->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
		 addr = (char *)(ndopt + 1);
		 memcpy(addr, LLADDR(sdl), ETHER_ADDR_LEN);
		 break;
	 default:
		 warnmsg(LOG_ERR, __FUNCTION__,
			 "unsupported link type(%d)", sdl->sdl_type);
		 exit(1);
	}

	return;
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
getinet6sysctl(int code)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0) < 0)
		return -1;
	else
		return value;
}

/*------------------------------------------------------------*/

static struct nlist nl[] = {
#define	N_IFNET	0
	{ "_ifnet" },
	{ "" },
};

#define	KREAD(x, y, z) { \
	if (kvm_read(kvmd, (u_long)x, (void *)y, sizeof(z)) != sizeof(z)) { \
		warnmsg(LOG_ERR, __FUNCTION__, "kvm_read failed");	\
		goto bad;						\
	}								\
   }

static int
getifa(char *name, struct in6_ifaddr *ifap)
{
	u_short index;
	kvm_t *kvmd = NULL;
	char buf[_POSIX2_LINE_MAX];
	struct ifnet *ifp;
	struct ifnet ifnet;
	struct in6_ifaddr *ifa;

	if (!ifap)
		exit(1);

	index = (u_short)if_nametoindex(name);
	if (index == 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "if_nametoindex failed for %s",
		       name);
		goto bad;
	}
	if ((kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, buf)) == NULL) {
		warnmsg(LOG_ERR, __FUNCTION__, "kvm_openfiles failed");
		goto bad;
	}
	if (kvm_nlist(kvmd, nl) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "kvm_nlist failed");
		goto bad;
	}
	if (nl[N_IFNET].n_value == 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "symbol \"%s\" not found",
		       nl[N_IFNET].n_name);
		goto bad;
	}

	KREAD(nl[N_IFNET].n_value, &ifp, struct ifnet *);
	while (ifp) {
		KREAD(ifp, &ifnet, struct ifnet);
		if (ifnet.if_index == index)
			break;
		ifp = TAILQ_NEXT(&ifnet, if_link);
	}
	if (!ifp) {
		warnmsg(LOG_ERR, __FUNCTION__, "interface \"%s\" not found",
		       name);
		goto bad;
	}

	ifa = (struct in6_ifaddr *)TAILQ_FIRST(&ifnet.if_addrhead);
	while (ifa) {
		KREAD(ifa, ifap, *ifap);
		if (ifap->ia_addr.sin6_family == AF_INET6
		 && IN6_IS_ADDR_LINKLOCAL(&ifap->ia_addr.sin6_addr)) {
			kvm_close(kvmd);
			return 0;
		}

		ifa = (struct in6_ifaddr *)
			TAILQ_NEXT((struct ifaddr *)ifap, ifa_link);
	}
	warnmsg(LOG_ERR, __FUNCTION__, "no IPv6 link-local address for %s",
	       name);

  bad:
	if (kvmd)
		kvm_close(kvmd);
	return -1;
}

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

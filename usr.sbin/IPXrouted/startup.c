/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: startup.c,v 1.1 1995/10/26 21:28:26 julian Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)startup.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <nlist.h>
#include <stdlib.h>

struct	interface *ifnet;
int	lookforinterfaces = 1;
int	performnlist = 1;
int	gateway = 0;
int	externalinterfaces = 0;		/* # of remote and local interfaces */

void
quit(s)
	char *s;
{
	extern int errno;
	int sverrno = errno;

	(void) fprintf(stderr, "IPXroute: ");
	if (s)
		(void) fprintf(stderr, "%s: ", s);
	(void) fprintf(stderr, "%s\n", strerror(sverrno));
	exit(1);
	/* NOTREACHED */
}

struct rt_addrinfo info;
/* Sleazy use of local variables throughout file, warning!!!! */
#define netmask	info.rti_info[RTAX_NETMASK]
#define ifaaddr	info.rti_info[RTAX_IFA]
#define brdaddr	info.rti_info[RTAX_BRD]

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(cp, cplim, rtinfo)
	register caddr_t cp, cplim;
	register struct rt_addrinfo *rtinfo;
{
	register struct sockaddr *sa;
	register int i;

	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

/*
 * Find the network interfaces which have configured themselves.
 * If the interface is present but not yet up (for example an
 * ARPANET IMP), set the lookforinterfaces flag so we'll
 * come back later and look again.
 */
void
ifinit(void)
{
	struct interface ifs, *ifp;
	size_t needed;
	int mib[6], no_ipxaddr = 0, flags = 0;
	char *buf, *cplim, *cp;
	register struct if_msghdr *ifm;
	register struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl = 0;

        mib[0] = CTL_NET;
        mib[1] = PF_ROUTE;
        mib[2] = 0;
        mib[3] = AF_IPX;
        mib[4] = NET_RT_IFLIST;
        mib[5] = 0;
        if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
                quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
        if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
	lookforinterfaces = 0;
	cplim = buf + needed;
	for (cp = buf; cp < cplim; cp += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)cp;
		if (ifm->ifm_type == RTM_IFINFO) {
			bzero(&ifs, sizeof(ifs));
			ifs.int_flags = flags = ifm->ifm_flags | IFF_INTERFACE;
			if ((flags & IFF_UP) == 0 || no_ipxaddr)
				lookforinterfaces = 1;
			sdl = (struct sockaddr_dl *) (ifm + 1);
			sdl->sdl_data[sdl->sdl_nlen] = 0;
			no_ipxaddr = 1;
			continue;
		}
		if (ifm->ifm_type != RTM_NEWADDR)
			quit("ifinit: out of sync");
		if ((flags & IFF_UP) == 0)
			continue;
		ifam = (struct ifa_msghdr *)ifm;
		info.rti_addrs = ifam->ifam_addrs;
		rt_xaddrs((char *)(ifam + 1), cp + ifam->ifam_msglen, &info);
		if (ifaaddr == 0) {
			syslog(LOG_ERR, "%s: (get addr)", sdl->sdl_data);
			continue;
		}
		ifs.int_addr = *ifaaddr;
		if (ifs.int_addr.sa_family != AF_IPX)
			continue;
		no_ipxaddr = 0;
		if (ifs.int_flags & IFF_POINTOPOINT) {
			if (brdaddr == 0) {
				syslog(LOG_ERR, "%s: (get dstaddr)",
					sdl->sdl_data);
				continue;
			}
			if (brdaddr->sa_family == AF_UNSPEC) {
				lookforinterfaces = 1;
				continue;
			}
			ifs.int_dstaddr = *brdaddr;
		}
		if (ifs.int_flags & IFF_BROADCAST) {
			if (brdaddr == 0) {
				syslog(LOG_ERR, "%s: (get broadaddr)",
					sdl->sdl_data);
				continue;
			}
			ifs.int_dstaddr = *brdaddr;
		}
		/* 
		 * already known to us? 
		 * what makes a POINTOPOINT if unique is its dst addr,
		 * NOT its source address 
		 */
		if ( ((ifs.int_flags & IFF_POINTOPOINT) &&
			if_ifwithdstaddr(&ifs.int_dstaddr)) ||
			( ((ifs.int_flags & IFF_POINTOPOINT) == 0) &&
			if_ifwithaddr(&ifs.int_addr)))
			continue;
		/* no one cares about software loopback interfaces */
		if (ifs.int_flags & IFF_LOOPBACK)
			continue;
		ifp = (struct interface *)
			malloc(sdl->sdl_nlen + 1 + sizeof(ifs));
		if (ifp == 0) {
			syslog(LOG_ERR, "IPXrouted: out of memory\n");
			lookforinterfaces = 1;
			break;
		}
		*ifp = ifs;
		/*
		 * Count the # of directly connected networks
		 * and point to point links which aren't looped
		 * back to ourself.  This is used below to
		 * decide if we should be a routing ``supplier''.
		 */
		if ((ifs.int_flags & IFF_POINTOPOINT) == 0 ||
		    if_ifwithaddr(&ifs.int_dstaddr) == 0)
			externalinterfaces++;
		/*
		 * If we have a point-to-point link, we want to act
		 * as a supplier even if it's our only interface,
		 * as that's the only way our peer on the other end
		 * can tell that the link is up.
		 */
		if ((ifs.int_flags & IFF_POINTOPOINT) && supplier < 0)
			supplier = 1;
		ifp->int_name = (char *)(ifp + 1);
		strcpy(ifp->int_name, sdl->sdl_data);

		ifp->int_metric = ifam->ifam_metric;
		ifp->int_next = ifnet;
		ifnet = ifp;
		traceinit(ifp);
		addrouteforif(ifp);
	}
	if (externalinterfaces > 1 && supplier < 0)
		supplier = 1;
	free(buf);
}

void
addrouteforif(ifp)
	struct interface *ifp;
{
	struct sockaddr *dst;
	struct rt_entry *rt;

	if (ifp->int_flags & IFF_POINTOPOINT) {
		int (*match)();
		register struct interface *ifp2 = ifnet;
		
		dst = &ifp->int_dstaddr;

		/* Search for interfaces with the same net */
		ifp->int_sq.n = ifp->int_sq.p = &(ifp->int_sq);
		match = afswitch[dst->sa_family].af_netmatch;
		if (match)
		for (ifp2 = ifnet; ifp2; ifp2 =ifp2->int_next) {
			if (ifp->int_flags & IFF_POINTOPOINT == 0)
				continue;
			if ((*match)(&ifp2->int_dstaddr,&ifp->int_dstaddr)) {
				insque(&ifp2->int_sq,&ifp->int_sq);
				break;
			}
		}
	} else {
		dst = &ifp->int_broadaddr;
	}
	rt = rtlookup(dst);
	if (rt)
		rtdelete(rt);
	if (tracing)
		fprintf(stderr, "Adding route to interface %s\n", ifp->int_name);
	if (ifp->int_transitions++ > 0)
		syslog(LOG_ERR, "re-installing interface %s", ifp->int_name);
	rtadd(dst, &ifp->int_addr, ifp->int_metric, 0,
		ifp->int_flags & (IFF_INTERFACE|IFF_PASSIVE|IFF_REMOTE));
}


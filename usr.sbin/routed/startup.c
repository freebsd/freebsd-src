/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)startup.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <syslog.h>
#include <stdlib.h>
#include "pathnames.h"

struct	interface *ifnet;
struct	interface **ifnext = &ifnet;
int	lookforinterfaces = 1;
int	externalinterfaces = 0;		/* # of remote and local interfaces */
int	foundloopback;			/* valid flag for loopaddr */
struct	sockaddr loopaddr;		/* our address on loopback */


void
quit(s)
	char *s;
{
	extern int errno;
	int sverrno = errno;

	(void) fprintf(stderr, "route: ");
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
ifinit()
{
	struct interface ifs, *ifp;
	size_t needed;
	int mib[6], no_ipaddr = 0, flags = 0;
	char *buf, *cplim, *cp;
	register struct if_msghdr *ifm;
	register struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl;
        struct sockaddr_in *sin;
	u_long i;

        mib[0] = CTL_NET;
        mib[1] = PF_ROUTE;
        mib[2] = 0;
        mib[3] = AF_INET;
        mib[4] = NET_RT_IFLIST;
        mib[5] = 0;
        if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
                quit("route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
        if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		quit("actual retrieval of interface table");
	lookforinterfaces = 0;
	cplim = buf + needed;
	for (cp = buf; cp < cplim; cp += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)cp;
		if (ifm->ifm_type == RTM_IFINFO) {
			bzero(&ifs, sizeof(ifs));
			ifs.int_flags = flags = (0xffff & ifm->ifm_flags) | IFF_INTERFACE;
			if ((flags & IFF_UP) == 0 || no_ipaddr)
				lookforinterfaces = 1;
			sdl = (struct sockaddr_dl *) (ifm + 1);
			sdl->sdl_data[sdl->sdl_nlen] = 0;
			no_ipaddr = 1;
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
		if (ifs.int_addr.sa_family != AF_INET)
			continue;
		no_ipaddr = 0;
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
		/*
		 * already known to us?
		 * This allows multiple point-to-point links
		 * to share a source address (possibly with one
		 * other link), but assumes that there will not be
		 * multiple links with the same destination address.
		 */
		if (ifs.int_flags & IFF_POINTOPOINT) {
			if (if_ifwithdstaddr(&ifs.int_dstaddr))
				continue;
		} else if (if_ifwithaddr(&ifs.int_addr))
			continue;
		if (ifs.int_flags & IFF_LOOPBACK) {
			ifs.int_flags |= IFF_PASSIVE;
			foundloopback = 1;
			loopaddr = ifs.int_addr;
			for (ifp = ifnet; ifp; ifp = ifp->int_next)
			    if (ifp->int_flags & IFF_POINTOPOINT)
				add_ptopt_localrt(ifp);
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
		 * Use a minimum metric of one;
		 * treat the interface metric (default 0)
		 * as an increment to the hop count of one.
		 */
		ifs.int_metric = ifam->ifam_metric + 1;
		if (netmask == 0) {
				syslog(LOG_ERR, "%s: (get netmask)",
					sdl->sdl_data);
				continue;
		}
		sin = (struct sockaddr_in *)netmask;
		ifs.int_subnetmask = ntohl(sin->sin_addr.s_addr);
		sin = (struct sockaddr_in *)&ifs.int_addr;
		i = ntohl(sin->sin_addr.s_addr);
		if (IN_CLASSA(i))
			ifs.int_netmask = IN_CLASSA_NET;
		else if (IN_CLASSB(i))
			ifs.int_netmask = IN_CLASSB_NET;
		else
			ifs.int_netmask = IN_CLASSC_NET;
		ifs.int_net = i & ifs.int_netmask;
		ifs.int_subnet = i & ifs.int_subnetmask;
		if (ifs.int_subnetmask != ifs.int_netmask)
			ifs.int_flags |= IFF_SUBNET;
		ifp = (struct interface *)
			malloc(sdl->sdl_nlen + 1 + sizeof(ifs));
		if (ifp == 0) {
			printf("routed: out of memory\n");
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
		if ((ifs.int_flags & IFF_LOOPBACK) == 0 &&
		    ((ifs.int_flags & IFF_POINTOPOINT) == 0 ||
		    if_ifwithaddr(&ifs.int_dstaddr) == 0))
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
		*ifnext = ifp;
		ifnext = &ifp->int_next;
		traceinit(ifp);
		addrouteforif(ifp);
	}
	if (externalinterfaces > 1 && supplier < 0)
		supplier = 1;
	free(buf);
}

/*
 * Add route for interface if not currently installed.
 * Create route to other end if a point-to-point link,
 * otherwise a route to this (sub)network.
 * INTERNET SPECIFIC.
 */
addrouteforif(ifp)
	register struct interface *ifp;
{
	struct sockaddr_in net;
	struct sockaddr *dst;
	int state;
	register struct rt_entry *rt;

	if (ifp->int_flags & IFF_POINTOPOINT)
		dst = &ifp->int_dstaddr;
	else {
		bzero((char *)&net, sizeof (net));
		net.sin_family = AF_INET;
		net.sin_addr = inet_makeaddr(ifp->int_subnet, INADDR_ANY);
		dst = (struct sockaddr *)&net;
	}
	rt = rtfind(dst);
	if (rt &&
	    (rt->rt_state & (RTS_INTERFACE | RTS_INTERNAL)) == RTS_INTERFACE)
		return;
	if (rt)
		rtdelete(rt);
	/*
	 * If interface on subnetted network,
	 * install route to network as well.
	 * This is meant for external viewers.
	 */
	if ((ifp->int_flags & (IFF_SUBNET|IFF_POINTOPOINT)) == IFF_SUBNET) {
		struct in_addr subnet;

		subnet = net.sin_addr;
		net.sin_addr = inet_makeaddr(ifp->int_net, INADDR_ANY);
		rt = rtfind(dst);
		if (rt == 0)
			rtadd(dst, &ifp->int_addr, ifp->int_metric,
			    ((ifp->int_flags & (IFF_INTERFACE|IFF_REMOTE)) |
			    RTS_PASSIVE | RTS_INTERNAL | RTS_SUBNET));
		else if ((rt->rt_state & (RTS_INTERNAL|RTS_SUBNET)) == 
		    (RTS_INTERNAL|RTS_SUBNET) &&
		    ifp->int_metric < rt->rt_metric)
			rtchange(rt, &rt->rt_router, ifp->int_metric);
		net.sin_addr = subnet;
	}
	if (ifp->int_transitions++ > 0)
		syslog(LOG_ERR, "re-installing interface %s", ifp->int_name);
	state = ifp->int_flags &
	    (IFF_INTERFACE | IFF_PASSIVE | IFF_REMOTE | IFF_SUBNET);
	if (ifp->int_flags & IFF_POINTOPOINT &&
	    (ntohl(((struct sockaddr_in *)&ifp->int_dstaddr)->sin_addr.s_addr) &
	    ifp->int_netmask) != ifp->int_net)
		state &= ~RTS_SUBNET;
	if (ifp->int_flags & IFF_LOOPBACK)
		state |= RTS_EXTERNAL;
	rtadd(dst, &ifp->int_addr, ifp->int_metric, state);
	if (ifp->int_flags & IFF_POINTOPOINT && foundloopback)
		add_ptopt_localrt(ifp);
}

/*
 * Add route to local end of point-to-point using loopback.
 * If a route to this network is being sent to neighbors on other nets,
 * mark this route as subnet so we don't have to propagate it too.
 */
add_ptopt_localrt(ifp)
	register struct interface *ifp;
{
	struct rt_entry *rt;
	struct sockaddr *dst;
	struct sockaddr_in net;
	int state;

	state = RTS_INTERFACE | RTS_PASSIVE;

	/* look for route to logical network */
	bzero((char *)&net, sizeof (net));
	net.sin_family = AF_INET;
	net.sin_addr = inet_makeaddr(ifp->int_net, INADDR_ANY);
	dst = (struct sockaddr *)&net;
	rt = rtfind(dst);
	if (rt && rt->rt_state & RTS_INTERNAL)
		state |= RTS_SUBNET;

	dst = &ifp->int_addr;
	if (rt = rtfind(dst)) {
		if (rt && rt->rt_state & RTS_INTERFACE)
			return;
		rtdelete(rt);
	}
	rtadd(dst, &loopaddr, 1, state);
}

/*
 * As a concession to the ARPANET we read a list of gateways
 * from /etc/gateways and add them to our tables.  This file
 * exists at each ARPANET gateway and indicates a set of ``remote''
 * gateways (i.e. a gateway which we can't immediately determine
 * if it's present or not as we can do for those directly connected
 * at the hardware level).  If a gateway is marked ``passive''
 * in the file, then we assume it doesn't have a routing process
 * of our design and simply assume it's always present.  Those
 * not marked passive are treated as if they were directly
 * connected -- they're added into the interface list so we'll
 * send them routing updates.
 *
 * PASSIVE ENTRIES AREN'T NEEDED OR USED ON GATEWAYS RUNNING EGP.
 */
gwkludge()
{
	struct sockaddr_in dst, gate;
	FILE *fp;
	char *type, *dname, *gname, *qual, buf[BUFSIZ];
	struct interface *ifp;
	int metric, n;
	struct rt_entry route;

	fp = fopen(_PATH_GATEWAYS, "r");
	if (fp == NULL)
		return;
	qual = buf;
	dname = buf + 64;
	gname = buf + ((BUFSIZ - 64) / 3);
	type = buf + (((BUFSIZ - 64) * 2) / 3);
	bzero((char *)&dst, sizeof (dst));
	bzero((char *)&gate, sizeof (gate));
	bzero((char *)&route, sizeof(route));
/* format: {net | host} XX gateway XX metric DD [passive | external]\n */
#define	readentry(fp) \
	fscanf((fp), "%s %s gateway %s metric %d %s\n", \
		type, dname, gname, &metric, qual)
	for (;;) {
		if ((n = readentry(fp)) == EOF)
			break;
		if (!getnetorhostname(type, dname, &dst))
			continue;
		if (!gethostnameornumber(gname, &gate))
			continue;
		if (metric == 0)			/* XXX */
			metric = 1;
		if (strcmp(qual, "passive") == 0) {
			/*
			 * Passive entries aren't placed in our tables,
			 * only the kernel's, so we don't copy all of the
			 * external routing information within a net.
			 * Internal machines should use the default
			 * route to a suitable gateway (like us).
			 */
			route.rt_dst = *(struct sockaddr *) &dst;
			route.rt_router = *(struct sockaddr *) &gate;
			route.rt_flags = RTF_UP;
			if (strcmp(type, "host") == 0)
				route.rt_flags |= RTF_HOST;
			if (metric)
				route.rt_flags |= RTF_GATEWAY;
			(void) rtioctl(ADD, &route.rt_rt);
			continue;
		}
		if (strcmp(qual, "external") == 0) {
			/*
			 * Entries marked external are handled
			 * by other means, e.g. EGP,
			 * and are placed in our tables only
			 * to prevent overriding them
			 * with something else.
			 */
			rtadd(&dst, &gate, metric, RTS_EXTERNAL|RTS_PASSIVE);
			continue;
		}
		/* assume no duplicate entries */
		externalinterfaces++;
		ifp = (struct interface *)malloc(sizeof (*ifp));
		bzero((char *)ifp, sizeof (*ifp));
		ifp->int_flags = IFF_REMOTE;
		/* can't identify broadcast capability */
		ifp->int_net = inet_netof(dst.sin_addr);
		if (strcmp(type, "host") == 0) {
			ifp->int_flags |= IFF_POINTOPOINT;
			ifp->int_dstaddr = *((struct sockaddr *)&dst);
		}
		ifp->int_addr = *((struct sockaddr *)&gate);
		ifp->int_metric = metric;
		ifp->int_next = ifnet;
		ifnet = ifp;
		addrouteforif(ifp);
	}
	fclose(fp);
}

getnetorhostname(type, name, sin)
	char *type, *name;
	struct sockaddr_in *sin;
{

	if (strcmp(type, "net") == 0) {
		struct netent *np = getnetbyname(name);
		int n;

		if (np == 0)
			n = inet_network(name);
		else {
			if (np->n_addrtype != AF_INET)
				return (0);
			n = np->n_net;
			/*
			 * getnetbyname returns right-adjusted value.
			 */
			if (n < 128)
				n <<= IN_CLASSA_NSHIFT;
			else if (n < 65536)
				n <<= IN_CLASSB_NSHIFT;
			else
				n <<= IN_CLASSC_NSHIFT;
		}
		sin->sin_family = AF_INET;
		sin->sin_addr = inet_makeaddr(n, INADDR_ANY);
		return (1);
	}
	if (strcmp(type, "host") == 0) {
		struct hostent *hp = gethostbyname(name);

		if (hp == 0)
			sin->sin_addr.s_addr = inet_addr(name);
		else {
			if (hp->h_addrtype != AF_INET)
				return (0);
			bcopy(hp->h_addr, &sin->sin_addr, hp->h_length);
		}
		sin->sin_family = AF_INET;
		return (1);
	}
	return (0);
}

gethostnameornumber(name, sin)
	char *name;
	struct sockaddr_in *sin;
{
	struct hostent *hp;

	hp = gethostbyname(name);
	if (hp) {
		bcopy(hp->h_addr, &sin->sin_addr, hp->h_length);
		sin->sin_family = hp->h_addrtype;
		return (1);
	}
	sin->sin_addr.s_addr = inet_addr(name);
	sin->sin_family = AF_INET;
	return (sin->sin_addr.s_addr != -1);
}

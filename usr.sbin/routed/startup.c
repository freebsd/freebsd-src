/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
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
static char sccsid[] = "@(#)startup.c	5.19 (Berkeley) 2/28/91";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <syslog.h>
#include <stdlib.h>
#include "pathnames.h"

struct	interface *ifnet;
struct	interface **ifnext = &ifnet;
int	lookforinterfaces = 1;
int	externalinterfaces = 0;		/* # of remote and local interfaces */
int	foundloopback;			/* valid flag for loopaddr */
struct	sockaddr loopaddr;		/* our address on loopback */

/*
 * Find the network interfaces which have configured themselves.
 * If the interface is present but not yet up (for example an
 * ARPANET IMP), set the lookforinterfaces flag so we'll
 * come back later and look again.
 */
ifinit()
{
	struct interface ifs, *ifp;
	int s;
	char buf[BUFSIZ], *cp, *cplim;
        struct ifconf ifc;
        struct ifreq ifreq, *ifr;
        struct sockaddr_in *sin;
	u_long i;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket: %m");
		close(s);
                return;
	}
        ifc.ifc_len = sizeof (buf);
        ifc.ifc_buf = buf;
        if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
                syslog(LOG_ERR, "ioctl (get interface configuration)");
		close(s);
                return;
        }
        ifr = ifc.ifc_req;
	lookforinterfaces = 0;
#ifdef RTM_ADD
#define max(a, b) (a > b ? a : b)
#define size(p)	max((p).sa_len, sizeof(p))
#else
#define size(p) (sizeof (p))
#endif
	cplim = buf + ifc.ifc_len; /*skip over if's with big ifr_addr's */
	for (cp = buf; cp < cplim;
			cp += sizeof (ifr->ifr_name) + size(ifr->ifr_addr)) {
		ifr = (struct ifreq *)cp;
		bzero((char *)&ifs, sizeof(ifs));
		ifs.int_addr = ifr->ifr_addr;
		ifreq = *ifr;
                if (ioctl(s, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
                        syslog(LOG_ERR, "%s: ioctl (get interface flags)",
			    ifr->ifr_name);
                        continue;
                }
		ifs.int_flags = (ifreq.ifr_flags & IFF_KERNELPUN) | IFF_INTERFACE;
		if ((ifs.int_flags & IFF_UP) == 0 ||
		    ifr->ifr_addr.sa_family == AF_UNSPEC) {
			lookforinterfaces = 1;
			continue;
		}
		/* argh, this'll have to change sometime */
		if (ifs.int_addr.sa_family != AF_INET)
			continue;
                if (ifs.int_flags & IFF_POINTOPOINT) {
                        if (ioctl(s, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
                                syslog(LOG_ERR, "%s: ioctl (get dstaddr)",
				    ifr->ifr_name);
                                continue;
			}
			if (ifr->ifr_addr.sa_family == AF_UNSPEC) {
				lookforinterfaces = 1;
				continue;
			}
			ifs.int_dstaddr = ifreq.ifr_dstaddr;
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
                        if (ioctl(s, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
                                syslog(LOG_ERR, "%s: ioctl (get broadaddr)",
				    ifr->ifr_name);
                                continue;
                        }
#ifndef sun
			ifs.int_broadaddr = ifreq.ifr_broadaddr;
#else
			ifs.int_broadaddr = ifreq.ifr_addr;
#endif
		}
#ifdef SIOCGIFMETRIC
		if (ioctl(s, SIOCGIFMETRIC, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "%s: ioctl (get metric)",
			    ifr->ifr_name);
			ifs.int_metric = 0;
		} else
			ifs.int_metric = ifreq.ifr_metric;
#else
		ifs.int_metric = 0;
#endif
		/*
		 * Use a minimum metric of one;
		 * treat the interface metric (default 0)
		 * as an increment to the hop count of one.
		 */
		ifs.int_metric++;
		if (ioctl(s, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			syslog(LOG_ERR, "%s: ioctl (get netmask)",
			    ifr->ifr_name);
			continue;
		}
		sin = (struct sockaddr_in *)&ifreq.ifr_addr;
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
		ifp = (struct interface *)malloc(sizeof (struct interface));
		if (ifp == 0) {
			printf("routed: out of memory\n");
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
		ifp->int_name = malloc(strlen(ifr->ifr_name) + 1);
		if (ifp->int_name == 0) {
			fprintf(stderr, "routed: ifinit: out of memory\n");
			syslog(LOG_ERR, "routed: ifinit: out of memory\n");
			close(s);
			return;
		}
		strcpy(ifp->int_name, ifr->ifr_name);
		*ifnext = ifp;
		ifnext = &ifp->int_next;
		traceinit(ifp);
		addrouteforif(ifp);
	}
	if (externalinterfaces > 1 && supplier < 0)
		supplier = 1;
	close(s);
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
			(void) ioctl(s, SIOCADDRT, (char *)&route.rt_rt);
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

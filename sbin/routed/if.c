/*
 * Copyright (c) 1983, 1993
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

#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)if.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif
#ident "$Revision: 1.17 $"

#include "defs.h"
#include "pathnames.h"

struct	interface *ifnet;		/* all interfaces */
int	tot_interfaces;			/* # of remote and local interfaces */
int	rip_interfaces;			/* # of interfaces doing RIP */
int	foundloopback;			/* valid flag for loopaddr */
naddr	loopaddr;			/* our address on loopback */

struct timeval ifinit_timer;

int	have_ripv1_out;			/* have a RIPv1 interface */
int	have_ripv1_in;


/* Find the interface with an address
 */
struct interface *
ifwithaddr(naddr addr,
	   int	bcast,			/* notice IFF_BROADCAST address */
	   int	remote)			/* include IS_REMOTE interfaces */
{
	struct interface *ifp, *possible = 0;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_addr == addr
		    || ((ifp->int_if_flags & IFF_BROADCAST)
			&& ifp->int_brdaddr == addr
			&& bcast)) {
			if ((ifp->int_state & IS_REMOTE) && !remote)
				continue;

			if (!(ifp->int_state & IS_BROKE)
			    && !(ifp->int_state & IS_PASSIVE))
				return ifp;

			possible = ifp;
		}
	}

	return possible;
}


/* find the interface with a name
 */
struct interface *
ifwithname(char *name,			/* "ec0" or whatever */
	   naddr addr)			/* 0 or network address */
{
	struct interface *ifp;


	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
		if (!strcmp(ifp->int_name, name)
		    && (ifp->int_addr == addr
			|| (addr == 0 && !(ifp->int_state & IS_ALIAS))))
			return ifp;
	}
	return 0;
}


struct interface *
ifwithindex(u_short index)
{
	struct interface *ifp;


	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
		if (ifp->int_index == index)
			return ifp;
	}
	return 0;
}


/* Find an interface from which the specified address
 * should have come from.  Used for figuring out which
 * interface a packet came in on -- for tracing.
 */
struct interface *
iflookup(naddr addr)
{
	struct interface *ifp, *maybe;

	maybe = 0;
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_if_flags & IFF_POINTOPOINT) {
			if (ifp->int_dstaddr == addr)
				/* finished with a match */
				return ifp;

		} else {
			/* finished with an exact match */
			if (ifp->int_addr == addr)
				return ifp;
			if ((ifp->int_if_flags & IFF_BROADCAST)
			    && ifp->int_brdaddr == addr)
				return ifp;

			/* Look for the longest approximate match.
			 */
			if (on_net(addr, ifp->int_net, ifp->int_mask)
			    && (maybe == 0
				|| ifp->int_mask > maybe->int_mask))
				maybe = ifp;
		}
	}

	return maybe;
}


/* Return the classical netmask for an IP address.
 */
naddr
std_mask(naddr addr)			/* in network order */
{
	NTOHL(addr);			/* was a host, not a network */

	if (addr == 0)			/* default route has mask 0 */
		return 0;
	if (IN_CLASSA(addr))
		return IN_CLASSA_NET;
	if (IN_CLASSB(addr))
		return IN_CLASSB_NET;
	return IN_CLASSC_NET;
}


/* Find the netmask that would be inferred by RIPv1 listeners
 *	on the given interface for a given network.
 *	If no interface is specified, look for the best fitting	interface.
 */
naddr
ripv1_mask_net(naddr addr,		/* in network byte order */
	       struct interface *ifp)	/* as seen on this interface */
{
	naddr mask = 0;

	if (addr == 0)			/* default always has 0 mask */
		return mask;

	if (ifp != 0) {
		/* If the target network is that of the associated interface
		 * on which it arrived, then use the netmask of the interface.
		 */
		if (on_net(addr, ifp->int_net, ifp->int_std_mask))
			mask = ifp->int_ripv1_mask;

	} else {
		/* Examine all interfaces, and if it the target seems
		 * to have the same network number of an interface, use the
		 * netmask of that interface.  If there is more than one
		 * such interface, prefer the interface with the longest
		 * match.
		 */
		for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
			if (on_net(addr, ifp->int_std_net, ifp->int_std_mask)
			    && ifp->int_ripv1_mask > mask)
				mask = ifp->int_ripv1_mask;
		}
	}

	/* Otherwise, make the classic A/B/C guess.
	 */
	if (mask == 0)
		mask = std_mask(addr);

	return mask;
}


naddr
ripv1_mask_host(naddr addr,		/* in network byte order */
		struct interface *ifp)	/* as seen on this interface */
{
	naddr mask = ripv1_mask_net(addr, ifp);


	/* If the computed netmask does not mask the address,
	 * then assume it is a host address
	 */
	if ((ntohl(addr) & ~mask) != 0)
		mask = HOST_MASK;
	return mask;
}


/* See if a IP address looks reasonable as a destination
 */
int					/* 0=bad */
check_dst(naddr addr)
{
	NTOHL(addr);

	if (IN_CLASSA(addr)) {
		if (addr == 0)
			return 1;	/* default */

		addr >>= IN_CLASSA_NSHIFT;
		return (addr != 0 && addr != IN_LOOPBACKNET);
	}

	return (IN_CLASSB(addr) || IN_CLASSC(addr));
}


/* Delete an interface.
 */
static void
ifdel(struct interface *ifp)
{
	struct ip_mreq m;
	struct interface *ifp1;


	trace_if("Del", ifp);

	ifp->int_state |= IS_BROKE;

	/* unlink the interface
	 */
	if (rip_sock_mcast == ifp)
		rip_sock_mcast = 0;
	if (ifp->int_next != 0)
		ifp->int_next->int_prev = ifp->int_prev;
	if (ifp->int_prev != 0)
		ifp->int_prev->int_next = ifp->int_next;
	else
		ifnet = ifp->int_next;

	if (!(ifp->int_state & IS_ALIAS)) {
		/* delete aliases
		 */
		for (ifp1 = ifnet; 0 != ifp1; ifp1 = ifp1->int_next) {
			if (ifp1 != ifp
			    && !strcmp(ifp->int_name, ifp1->int_name))
				ifdel(ifp1);
		}

		if ((ifp->int_if_flags & IFF_MULTICAST)
#ifdef MCAST_PPP_BUG
		    && !(ifp->int_if_flags & IFF_POINTOPOINT)
#endif
		    && rip_sock >= 0) {
			m.imr_multiaddr.s_addr = htonl(INADDR_RIP_GROUP);
			m.imr_interface.s_addr = ((ifp->int_if_flags
						   & IFF_POINTOPOINT)
						  ? ifp->int_dstaddr
						  : ifp->int_addr);
			if (setsockopt(rip_sock,IPPROTO_IP,IP_DROP_MEMBERSHIP,
				       &m, sizeof(m)) < 0
			    && errno != EADDRNOTAVAIL
			    && !TRACEACTIONS)
				LOGERR("setsockopt(IP_DROP_MEMBERSHIP RIP)");
		}
		if (ifp->int_rip_sock >= 0) {
			(void)close(ifp->int_rip_sock);
			ifp->int_rip_sock = -1;
			fix_select();
		}

		tot_interfaces--;
		if (!IS_RIP_OFF(ifp->int_state))
			rip_interfaces--;

		/* Zap all routes associated with this interface.
		 * Assume routes just using gateways beyond this interface will
		 * timeout naturally, and have probably already died.
		 */
		(void)rn_walktree(rhead, walk_bad, 0);

		set_rdisc_mg(ifp, 0);
		if_bad_rdisc(ifp);
	}

	free(ifp);
}


/* Mark an interface ill.
 */
void
if_sick(struct interface *ifp)
{
	if (0 == (ifp->int_state & (IS_SICK | IS_BROKE))) {
		ifp->int_state |= IS_SICK;
		trace_if("Chg", ifp);

		LIM_SEC(ifinit_timer, now.tv_sec+CHECK_BAD_INTERVAL);
	}
}


/* Mark an interface dead.
 */
void
if_bad(struct interface *ifp)
{
	struct interface *ifp1;


	if (ifp->int_state & IS_BROKE)
		return;

	LIM_SEC(ifinit_timer, now.tv_sec+CHECK_BAD_INTERVAL);

	ifp->int_state |= (IS_BROKE | IS_SICK);
	ifp->int_state &= ~(IS_RIP_QUERIED | IS_ACTIVE);
	ifp->int_data.ts = 0;

	trace_if("Chg", ifp);

	if (!(ifp->int_state & IS_ALIAS)) {
		for (ifp1 = ifnet; 0 != ifp1; ifp1 = ifp1->int_next) {
			if (ifp1 != ifp
			    && !strcmp(ifp->int_name, ifp1->int_name))
				if_bad(ifp1);
		}
		(void)rn_walktree(rhead, walk_bad, 0);
		if_bad_rdisc(ifp);
	}
}


/* Mark an interface alive
 */
int					/* 1=it was dead */
if_ok(struct interface *ifp,
      char *type)
{
	struct interface *ifp1;


	if (!(ifp->int_state & IS_BROKE)) {
		if (ifp->int_state & IS_SICK) {
			trace_act("%sinterface %s to %s working better\n",
				  type,
				  ifp->int_name, naddr_ntoa(ifp->int_addr));
			ifp->int_state &= ~IS_SICK;
		}
		return 0;
	}

	msglog("%sinterface %s to %s restored",
	       type, ifp->int_name, naddr_ntoa(ifp->int_addr));
	ifp->int_state &= ~(IS_BROKE | IS_SICK);
	ifp->int_data.ts = 0;

	if (!(ifp->int_state & IS_ALIAS)) {
		for (ifp1 = ifnet; 0 != ifp1; ifp1 = ifp1->int_next) {
			if (ifp1 != ifp
			    && !strcmp(ifp->int_name, ifp1->int_name))
				if_ok(ifp1, type);
		}
		if_ok_rdisc(ifp);
	}
	return 1;
}


/* disassemble routing message
 */
void
rt_xaddrs(struct rt_addrinfo *info,
	  struct sockaddr *sa,
	  struct sockaddr *lim,
	  int addrs)
{
	int i;
#ifdef _HAVE_SA_LEN
	static struct sockaddr sa_zero;
#endif
#ifdef sgi
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))
#else
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) \
		    : sizeof(long))
#endif


	bzero(info, sizeof(*info));
	info->rti_addrs = addrs;
	for (i = 0; i < RTAX_MAX && sa < lim; i++) {
		if ((addrs & (1 << i)) == 0)
			continue;
#ifdef _HAVE_SA_LEN
		info->rti_info[i] = (sa->sa_len != 0) ? sa : &sa_zero;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(sa->sa_len));
#else
		info->rti_info[i] = sa;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(_FAKE_SA_LEN_DST(sa)));
#endif
	}
}


/* Find the network interfaces which have configured themselves.
 *	This must be done regularly, if only for extra addresses
 *	that come and go on interfaces.
 */
void
ifinit(void)
{
	static char *sysctl_buf;
	static size_t sysctl_buf_size = 0;
	uint complaints = 0;
	static u_int prev_complaints = 0;
#	define COMP_NOT_INET	0x001
#	define COMP_WIERD	0x002
#	define COMP_NOADDR	0x004
#	define COMP_BADADDR	0x008
#	define COMP_NODST	0x010
#	define COMP_NOBADR	0x020
#	define COMP_NOMASK	0x040
#	define COMP_DUP		0x080
#	define COMP_BAD_METRIC	0x100
#	define COMP_NETMASK	0x200

	struct interface ifs, ifs0, *ifp, *ifp1;
	struct rt_entry *rt;
	size_t needed;
	int mib[6];
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct sockaddr_dl *sdl;
	int in, ierr, out, oerr;
	struct intnet *intnetp;
	struct rt_addrinfo info;
#ifdef SIOCGIFMETRIC
	struct ifreq ifr;
#endif


	ifinit_timer.tv_sec = now.tv_sec + (supplier
					    ? CHECK_ACT_INTERVAL
					    : CHECK_QUIET_INTERVAL);

	/* mark all interfaces so we can get rid of thost that disappear */
	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next)
		ifp->int_state &= ~(IS_CHECKED | IS_DUP);

	/* Fetch the interface list, without too many system calls
	 * since we do it repeatedly.
	 */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	for (;;) {
		if ((needed = sysctl_buf_size) != 0) {
			if (sysctl(mib, 6, sysctl_buf,&needed, 0, 0) >= 0)
				break;
			if (errno != ENOMEM && errno != EFAULT)
				BADERR(1, "ifinit: get interface table");
			free(sysctl_buf);
			needed = 0;
		}
		if (sysctl(mib, 6, 0, &needed, 0, 0) < 0)
			BADERR(1,"ifinit: route-sysctl-estimate");
		sysctl_buf = rtmalloc(sysctl_buf_size = needed, "ifinit");
	}

	ifam_lim = (struct ifa_msghdr *)(sysctl_buf + needed);
	for (ifam = (struct ifa_msghdr *)sysctl_buf;
	     ifam < ifam_lim;
	     ifam = ifam2) {

		ifam2 = (struct ifa_msghdr*)((char*)ifam + ifam->ifam_msglen);

		if (ifam->ifam_type == RTM_IFINFO) {
			ifm = (struct if_msghdr *)ifam;
			/* make prototype structure for the IP aliases
			 */
			bzero(&ifs0, sizeof(ifs0));
			ifs0.int_rip_sock = -1;
			ifs0.int_index = ifm->ifm_index;
			ifs0.int_if_flags = ifm->ifm_flags;
			ifs0.int_state = IS_CHECKED;
			ifs0.int_act_time = now.tv_sec;
			ifs0.int_data.ts = now.tv_sec;
			ifs0.int_data.ipackets = ifm->ifm_data.ifi_ipackets;
			ifs0.int_data.ierrors = ifm->ifm_data.ifi_ierrors;
			ifs0.int_data.opackets = ifm->ifm_data.ifi_opackets;
			ifs0.int_data.oerrors = ifm->ifm_data.ifi_oerrors;
#ifdef sgi
			ifs0.int_data.odrops = ifm->ifm_data.ifi_odrops;
#endif
			sdl = (struct sockaddr_dl *)(ifm + 1);
			sdl->sdl_data[sdl->sdl_nlen] = 0;
			continue;
		}
		if (ifam->ifam_type != RTM_NEWADDR) {
			logbad(1,"ifinit: out of sync");
			continue;
		}

		rt_xaddrs(&info, (struct sockaddr *)(ifam+1),
			  (struct sockaddr *)ifam2,
			  ifam->ifam_addrs);

		if (INFO_IFA(&info) == 0) {
			if (iff_alive(ifs.int_if_flags)) {
				if (!(prev_complaints & COMP_NOADDR))
					msglog("%s has no address",
					       sdl->sdl_data);
				complaints |= COMP_NOADDR;
			}
			continue;
		}
		if (INFO_IFA(&info)->sa_family != AF_INET) {
			if (iff_alive(ifs.int_if_flags)) {
				if (!(prev_complaints & COMP_NOT_INET))
					trace_act("%s: not AF_INET\n",
						  sdl->sdl_data);
				complaints |= COMP_NOT_INET;
			}
			continue;
		}

		bcopy(&ifs0, &ifs, sizeof(ifs0));
		ifs0.int_state |= IS_ALIAS;	/* next will be an alias */

		ifs.int_addr = S_ADDR(INFO_IFA(&info));

		if (ntohl(ifs.int_addr)>>24 == 0
		    || ntohl(ifs.int_addr)>>24 == 0xff) {
			if (iff_alive(ifs.int_if_flags)) {
				if (!(prev_complaints & COMP_BADADDR))
					msglog("%s has a bad address",
					       sdl->sdl_data);
				complaints |= COMP_BADADDR;
			}
			continue;
		}

		if (ifs.int_if_flags & IFF_BROADCAST) {
			if (INFO_MASK(&info) == 0) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NOMASK))
						msglog("%s has no netmask",
						       sdl->sdl_data);
					complaints |= COMP_NOMASK;
				}
				continue;
			}
			ifs.int_dstaddr = ifs.int_addr;
			ifs.int_mask = ntohl(S_ADDR(INFO_MASK(&info)));
			ifs.int_ripv1_mask = ifs.int_mask;
			ifs.int_net = ntohl(ifs.int_addr) & ifs.int_mask;
			ifs.int_std_mask = std_mask(ifs.int_addr);
			if (ifs.int_mask != ifs.int_std_mask)
				ifs.int_state |= IS_SUBNET;

			if (INFO_BRD(&info) == 0) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NOBADR))
						msglog("%s has no"
						       " broadcast address",
						       sdl->sdl_data);
					complaints |= COMP_NOBADR;
				}
				continue;
			}
			ifs.int_brdaddr = S_ADDR(INFO_BRD(&info));

		} else if (ifs.int_if_flags & IFF_POINTOPOINT) {
			if (INFO_BRD(&info) == 0
			    || INFO_BRD(&info)->sa_family != AF_INET) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NODST))
						msglog("%s has a bad"
						       " destination address",
						       sdl->sdl_data);
					complaints |= COMP_NODST;
				}
				continue;
			}
			ifs.int_dstaddr = S_ADDR(INFO_BRD(&info));
			if (ntohl(ifs.int_dstaddr)>>24 == 0
			    || ntohl(ifs.int_dstaddr)>>24 == 0xff) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NODST))
						msglog("%s has a bad"
						       " destination address",
						       sdl->sdl_data);
					complaints |= COMP_NODST;
				}
				continue;
			}
			ifs.int_mask = HOST_MASK;
			ifs.int_ripv1_mask = ntohl(S_ADDR(INFO_MASK(&info)));
			ifs.int_net = ntohl(ifs.int_dstaddr);
			ifs.int_std_mask = std_mask(ifs.int_dstaddr);

		} else if (ifs.int_if_flags & IFF_LOOPBACK) {
			ifs.int_state |= IS_PASSIVE | IS_NO_RIP;
			ifs.int_dstaddr = ifs.int_addr;
			ifs.int_mask = HOST_MASK;
			ifs.int_ripv1_mask = HOST_MASK;
			ifs.int_net = ntohl(ifs.int_dstaddr);
			ifs.int_std_mask = std_mask(ifs.int_dstaddr);
			if (!foundloopback) {
				foundloopback = 1;
				loopaddr = ifs.int_addr;
			}

		} else {
			if (!(prev_complaints & COMP_WIERD))
				trace_act("%s is neither broadcast"
					  " nor point-to-point nor loopback",
					  sdl->sdl_data);
			complaints |= COMP_WIERD;
			continue;
		}
		ifs.int_std_net = ifs.int_net & ifs.int_std_mask;
		ifs.int_std_addr = htonl(ifs.int_std_net);

		/* Use a minimum metric of one.  Treat the interface metric
		 * (default 0) as an increment to the hop count of one.
		 *
		 * The metric obtained from the routing socket dump of
		 * interface addresses is wrong.  It is not set by the
		 * SIOCSIFMETRIC ioctl.
		 */
#ifdef SIOCGIFMETRIC
		strncpy(ifr.ifr_name, sdl->sdl_data, sizeof(ifr.ifr_name));
		if (ioctl(rt_sock, SIOCGIFMETRIC, &ifr) < 0) {
			DBGERR(1, "ioctl(SIOCGIFMETRIC)");
			ifs.int_metric = 0;
		} else {
			ifs.int_metric = ifr.ifr_metric;
		}
#else
		ifs.int_metric = ifam->ifam_metric;
#endif
		if (ifs.int_metric > HOPCNT_INFINITY) {
			ifs.int_metric = 0;
			if (!(prev_complaints & COMP_BAD_METRIC)
			    && iff_alive(ifs.int_if_flags)) {
				complaints |= COMP_BAD_METRIC;
				msglog("%s has a metric of %d",
				       sdl->sdl_data, ifs.int_metric);
			}
		}

		/* See if this is a familiar interface.
		 * If so, stop worrying about it if it is the same.
		 * Start it over if it now is to somewhere else, as happens
		 * frequently with PPP and SLIP.
		 */
		ifp = ifwithname(sdl->sdl_data, ((ifs.int_state & IS_ALIAS)
						 ? ifs.int_addr
						 : 0));
		if (ifp != 0) {
			ifp->int_state |= IS_CHECKED;

			if (0 != ((ifp->int_if_flags ^ ifs.int_if_flags)
				  & (IFF_BROADCAST
				     | IFF_LOOPBACK
				     | IFF_POINTOPOINT
				     | IFF_MULTICAST))
			    || 0 != ((ifp->int_state ^ ifs.int_state)
				     & IS_ALIAS)
			    || ifp->int_addr != ifs.int_addr
			    || ifp->int_brdaddr != ifs.int_brdaddr
			    || ifp->int_dstaddr != ifs.int_dstaddr
			    || ifp->int_mask != ifs.int_mask
			    || ifp->int_metric != ifs.int_metric) {
				/* Forget old information about
				 * a changed interface.
				 */
				trace_act("interface %s has changed\n",
					  ifp->int_name);
				ifdel(ifp);
				ifp = 0;
			}
		}

		if (ifp != 0) {
			/* The primary representative of an alias worries
			 * about how things are working.
			 */
			if (ifp->int_state & IS_ALIAS)
				continue;

			/* note interfaces that have been turned off
			 */
			if (!iff_alive(ifs.int_if_flags)) {
				if (iff_alive(ifp->int_if_flags)) {
					msglog("interface %s to %s turned off",
					       ifp->int_name,
					       naddr_ntoa(ifp->int_addr));
					if_bad(ifp);
					ifp->int_if_flags &= ~IFF_UP_RUNNING;
				}
				continue;
			}
			/* or that were off and are now ok */
			if (!iff_alive(ifp->int_if_flags)) {
				ifp->int_if_flags |= IFF_UP_RUNNING;
				(void)if_ok(ifp, "");
			}

			/* If it has been long enough,
			 * see if the interface is broken.
			 */
			if (now.tv_sec < ifp->int_data.ts+CHECK_BAD_INTERVAL)
				continue;

			in = ifs.int_data.ipackets - ifp->int_data.ipackets;
			ierr = ifs.int_data.ierrors - ifp->int_data.ierrors;
			out = ifs.int_data.opackets - ifp->int_data.opackets;
			oerr = ifs.int_data.oerrors - ifp->int_data.oerrors;
#ifdef sgi
			/* Through at least IRIX 6.2, PPP and SLIP
			 * count packets dropped by  the filters.
			 * But FDDI rings stuck non-operational count
			 * dropped packets as they wait for improvement.
			 */
			if (!(ifp->int_if_flags & IFF_POINTOPOINT))
				oerr += (ifs.int_data.odrops
					 - ifp->int_data.odrops);
#endif
			/* If the interface just awoke, restart the counters.
			 */
			if (ifp->int_data.ts == 0) {
				ifp->int_data = ifs.int_data;
				continue;
			}
			ifp->int_data = ifs.int_data;

			/* Withhold judgement when the short error
			 * counters wrap or the interface is reset.
			 */
			if (ierr < 0 || in < 0 || oerr < 0 || out < 0) {
				LIM_SEC(ifinit_timer,
					now.tv_sec+CHECK_BAD_INTERVAL);
				continue;
			}

			/* Withhold judgement when there is no traffic
			 */
			if (in == 0 && out == 0 && ierr == 0 && oerr == 0)
				continue;

			/* It is bad if input or output is not working.
			 * Require presistent problems before marking it dead.
			 */
			if ((in <= ierr && ierr > 0)
			    || (out <= oerr && oerr > 0)) {
				if (!(ifp->int_state & IS_SICK)) {
					trace_act("interface %s to %s"
						  " sick: in=%d ierr=%d"
						  " out=%d oerr=%d\n",
						  ifp->int_name,
						  naddr_ntoa(ifp->int_addr),
						  in, ierr, out, oerr);
					if_sick(ifp);
					continue;
				}
				if (!(ifp->int_state & IS_BROKE)) {
					msglog("interface %s to %s bad:"
					       " in=%d ierr=%d out=%d oerr=%d",
					       ifp->int_name,
					       naddr_ntoa(ifp->int_addr),
					       in, ierr, out, oerr);
					if_bad(ifp);
				}
				continue;
			}

			/* otherwise, it is active and healthy
			 */
			ifp->int_act_time = now.tv_sec;
			(void)if_ok(ifp, "");
			continue;
		}

		/* This is a new interface.
		 * If it is dead, forget it.
		 */
		if (!iff_alive(ifs.int_if_flags))
			continue;

		/* See if it duplicates an existing interface.
		 */
		for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
			if (ifp->int_mask != ifs.int_mask)
				continue;
			if (((ifp->int_addr != ifs.int_addr
			      && ifs.int_mask != HOST_MASK)
			     || (ifp->int_dstaddr != ifs.int_dstaddr
				 && ifs.int_mask == HOST_MASK)))
				continue;
			if (!iff_alive(ifp->int_if_flags))
				continue;
			/* Let one of our real interfaces be marked
			 * passive.
			 */
			if ((ifp->int_state & IS_PASSIVE)
			    && !(ifp->int_state & IS_EXTERNAL))
				continue;

			/* It does duplicate an existing interface,
			 * so complain about it, mark the other one
			 * duplicated, and for get this one.
			 */
			if (!(prev_complaints & COMP_DUP)) {
				complaints |= COMP_DUP;
				msglog("%s is duplicated by %s at %s",
				       sdl->sdl_data, ifp->int_name,
				       naddr_ntoa(ifp->int_addr));
			}
			ifp->int_state |= IS_DUP;
			break;
		}
		if (ifp != 0)
			continue;

		/* It is new and ok.  So make it real
		 */
		strncpy(ifs.int_name, sdl->sdl_data,
			MIN(sizeof(ifs.int_name)-1, sdl->sdl_nlen));
		get_parms(&ifs);

		/* Add it to the list of interfaces
		 */
		ifp = (struct interface *)rtmalloc(sizeof(*ifp), "ifinit");
		bcopy(&ifs, ifp, sizeof(*ifp));
		if (ifnet != 0) {
			ifp->int_next = ifnet;
			ifnet->int_prev = ifp;
		}
		ifnet = ifp;
		trace_if("Add", ifp);

		/* Notice likely bad netmask.
		 */
		if (!(prev_complaints & COMP_NETMASK)
		    && !(ifp->int_if_flags & IFF_POINTOPOINT)) {
			for (ifp1 = ifnet; 0 != ifp1; ifp1 = ifp1->int_next) {
				if (ifp1->int_mask == ifp->int_mask)
					continue;
				if (ifp1->int_if_flags & IFF_POINTOPOINT)
					continue;
				if (on_net(ifp->int_addr,
					   ifp1->int_net, ifp1->int_mask)
				    || on_net(ifp1->int_addr,
					      ifp->int_net, ifp->int_mask)) {
					msglog("possible netmask problem"
					       " betwen %s:%s and %s:%s",
					       ifp->int_name,
					       addrname(htonl(ifp->int_net),
							ifp->int_mask, 1),
					       ifp1->int_name,
					       addrname(htonl(ifp1->int_net),
							ifp1->int_mask, 1));
					complaints |= COMP_NETMASK;
				}
			}
		}

		/* Count the # of directly connected networks.
		 */
		if (!(ifp->int_state & IS_ALIAS)) {
			if (!(ifp->int_if_flags & IFF_LOOPBACK))
				tot_interfaces++;
			if (!IS_RIP_OFF(ifp->int_state))
				rip_interfaces++;
		}

		if_ok_rdisc(ifp);
		rip_on(ifp);
	}

	/* If we are multi-homed and have at least one interface
	 * listening to RIP, then output by default.
	 */
	if (!supplier_set && rip_interfaces > 1)
		set_supplier();

	/* If we are multi-homed, optionally advertise a route to
	 * our main address.
	 */
	if (advertise_mhome
	    || (tot_interfaces > 1
		&& mhome
		&& (ifp = ifwithaddr(myaddr, 0, 0)) != 0
		&& foundloopback)) {
		advertise_mhome = 1;
		rt = rtget(myaddr, HOST_MASK);
		if (rt != 0) {
			if (rt->rt_ifp != ifp
			    || rt->rt_router != loopaddr) {
				rtdelete(rt);
				rt = 0;
			} else {
				rtchange(rt, rt->rt_state | RS_MHOME,
					 loopaddr, loopaddr,
					 0, 0, ifp, rt->rt_time, 0);
			}
		}
		if (rt == 0)
			rtadd(myaddr, HOST_MASK, loopaddr, loopaddr,
			      0, 0, RS_MHOME, ifp);
	}

	for (ifp = ifnet; ifp != 0; ifp = ifp1) {
		ifp1 = ifp->int_next;	/* because we may delete it */

		/* Forget any interfaces that have disappeared.
		 */
		if (!(ifp->int_state & (IS_CHECKED | IS_REMOTE))) {
			trace_act("interface %s has disappeared\n",
				  ifp->int_name);
			ifdel(ifp);
			continue;
		}

		if ((ifp->int_state & IS_BROKE)
		    && !(ifp->int_state & IS_PASSIVE))
			LIM_SEC(ifinit_timer, now.tv_sec+CHECK_BAD_INTERVAL);

		/* If we ever have a RIPv1 interface, assume we always will.
		 * It might come back if it ever goes away.
		 */
		if (!(ifp->int_state & IS_NO_RIPV1_OUT) && supplier)
			have_ripv1_out = 1;
		if (!(ifp->int_state & IS_NO_RIPV1_IN))
			have_ripv1_in = 1;
	}

	for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
		/* Ensure there is always a network route for interfaces,
		 * after any dead interfaces have been deleted, which
		 * might affect routes for point-to-point links.
		 */
		addrouteforif(ifp);

		/* Add routes to the local end of point-to-point interfaces
		 * using loopback.
		 */
		if ((ifp->int_if_flags & IFF_POINTOPOINT)
		    && !(ifp->int_state & IS_REMOTE)
		    && foundloopback) {
			/* Delete any routes to the network address through
			 * foreign routers. Remove even static routes.
			 */
			del_static(ifp->int_addr, HOST_MASK, 0);
			rt = rtget(ifp->int_addr, HOST_MASK);
			if (rt != 0 && rt->rt_router != loopaddr) {
				rtdelete(rt);
				rt = 0;
			}
			if (rt != 0) {
				if (!(rt->rt_state & RS_LOCAL)
				    || rt->rt_metric > ifp->int_metric) {
					ifp1 = ifp;
				} else {
					ifp1 = rt->rt_ifp;
				}
				rtchange(rt,((rt->rt_state & ~RS_NET_SYN)
					     | (RS_IF|RS_LOCAL)),
					 loopaddr, loopaddr,
					 0, 0, ifp1, rt->rt_time, 0);
			} else {
				rtadd(ifp->int_addr, HOST_MASK,
				      loopaddr, loopaddr,
				      0, 0, (RS_IF | RS_LOCAL), ifp);
			}
		}
	}

	/* add the authority routes */
	for (intnetp = intnets; intnetp!=0; intnetp = intnetp->intnet_next) {
		rt = rtget(intnetp->intnet_addr, intnetp->intnet_mask);
		if (rt != 0
		    && !(rt->rt_state & RS_NO_NET_SYN)
		    && !(rt->rt_state & RS_NET_INT)) {
			rtdelete(rt);
			rt = 0;
		}
		if (rt == 0)
			rtadd(intnetp->intnet_addr, intnetp->intnet_mask,
			      loopaddr, loopaddr, intnetp->intnet_metric-1,
			      0, RS_NET_SYN | RS_NET_INT, 0);
	}

	prev_complaints = complaints;
}


static void
check_net_syn(struct interface *ifp)
{
	struct rt_entry *rt;


	/* Turn on the need to automatically synthesize a network route
	 * for this interface only if we are running RIPv1 on some other
	 * interface that is on a different class-A,B,or C network.
	 */
	if (have_ripv1_out || have_ripv1_in) {
		ifp->int_state |= IS_NEED_NET_SYN;
		rt = rtget(ifp->int_std_addr, ifp->int_std_mask);
		if (rt != 0
		    && 0 == (rt->rt_state & RS_NO_NET_SYN)
		    && (!(rt->rt_state & RS_NET_SYN)
			|| rt->rt_metric > ifp->int_metric)) {
			rtdelete(rt);
			rt = 0;
		}
		if (rt == 0)
			rtadd(ifp->int_std_addr, ifp->int_std_mask,
			      ifp->int_addr, ifp->int_addr,
			      ifp->int_metric, 0, RS_NET_SYN, ifp);

	} else {
		ifp->int_state &= ~IS_NEED_NET_SYN;

		rt = rtget(ifp->int_std_addr,
			   ifp->int_std_mask);
		if (rt != 0
		    && (rt->rt_state & RS_NET_SYN)
		    && rt->rt_ifp == ifp)
			rtbad_sub(rt);
	}
}


/* Add route for interface if not currently installed.
 * Create route to other end if a point-to-point link,
 * otherwise a route to this (sub)network.
 */
void
addrouteforif(struct interface *ifp)
{
	struct rt_entry *rt;
	naddr dst, gate;


	/* skip sick interfaces
	 */
	if (ifp->int_state & IS_BROKE)
		return;

	/* If the interface on a subnet, then install a RIPv1 route to
	 * the network as well (unless it is sick).
	 */
	if (ifp->int_state & IS_SUBNET)
		check_net_syn(ifp);

	if (ifp->int_state & IS_REMOTE) {
		dst = ifp->int_addr;
		gate = ifp->int_dstaddr;
		/* If we are going to send packets to the gateway,
		 * it must be reachable using our physical interfaces
		 */
		if (!(ifp->int_state && IS_EXTERNAL)
		    && !rtfind(ifp->int_dstaddr)
		    && ifp->int_transitions == 0) {
			msglog("unreachable gateway %s in "
			       _PATH_GATEWAYS" entry %s",
			       naddr_ntoa(gate), ifp->int_name);
			return;
		}

	} else {
		dst = (0 != (ifp->int_if_flags & (IFF_POINTOPOINT
						  | IFF_LOOPBACK))
		       ? ifp->int_dstaddr
		       : htonl(ifp->int_net));
		gate = ifp->int_addr;
	}

	/* We are finished if the correct main interface route exists.
	 * The right route must be for the right interface, not synthesized
	 * from a subnet, be a "gateway" or not as appropriate, and so forth.
	 */
	del_static(dst, ifp->int_mask, 0);
	rt = rtget(dst, ifp->int_mask);
	if (rt != 0) {
		if ((rt->rt_ifp != ifp
		     || rt->rt_router != ifp->int_addr)
		    && (!(ifp->int_state & IS_DUP)
			|| rt->rt_ifp == 0
			|| (rt->rt_ifp->int_state & IS_BROKE))) {
			rtdelete(rt);
			rt = 0;
		} else {
			rtchange(rt, ((rt->rt_state | RS_IF)
				      & ~(RS_NET_SYN | RS_LOCAL)),
				 ifp->int_addr, ifp->int_addr,
				 ifp->int_metric, 0, ifp, now.tv_sec, 0);
		}
	}
	if (rt == 0) {
		if (ifp->int_transitions++ > 0)
			trace_act("re-install interface %s\n",
				  ifp->int_name);

		rtadd(dst, ifp->int_mask, gate, gate,
		      ifp->int_metric, 0, RS_IF, ifp);
	}
}

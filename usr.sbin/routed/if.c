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

#ifndef lint
static char sccsid[] = "@(#)if.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#ident "$Revision: 1.1.3.1 $"

#include "defs.h"
#include "pathnames.h"

struct	interface *ifnet;		/* all interfaces */
int	tot_interfaces;			/* # of remote and local interfaces */
int	rip_interfaces;			/* # of interfaces doing RIP */
int	foundloopback;			/* valid flag for loopaddr */
naddr	loopaddr;			/* our address on loopback */

struct timeval ifinit_timer;

int	have_ripv1;			/* have a RIPv1 interface */


/* Find the interface with an address
 */
struct interface *
ifwithaddr(naddr addr,
	   int	bcast,			/* notice IFF_BROADCAST address */
	   int	remote)			/* include IS_REMOTE interfaces */
{
	struct interface *ifp, *possible = 0;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if ((ifp->int_state & IS_REMOTE) && !remote)
			continue;

		if (ifp->int_addr == addr
		    || ((ifp->int_if_flags & IFF_BROADCAST)
			&& ifp->int_brdaddr == addr
			&& bcast)) {
			if (!(ifp->int_state & IS_BROKE))
				return ifp;
			possible = ifp;
		}
	}

	return possible;
}


/* find the interface with a name
 */
struct interface *
ifwithname(char *name,			/* enp0 or whatever */
	   naddr addr)			/* 0 or network address */
{
	struct interface *ifp;


	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
		if (!strcmp(ifp->int_name, name)
		    && ((addr == 0 && !(ifp->int_state & IS_ALIAS)
			 || ifp->int_addr == addr)))
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
	int twice;

	twice = 0;
	maybe = 0;
	do {
		for (ifp = ifnet; ifp; ifp = ifp->int_next) {
			/* finished with an exact match */
			if (ifp->int_addr == addr)
				return ifp;

			if ((ifp->int_if_flags & IFF_BROADCAST)
			    && ifp->int_brdaddr == addr)
				return ifp;

			if ((ifp->int_if_flags & IFF_POINTOPOINT)
			    && ifp->int_dstaddr == addr)
				return ifp;

			/* Look for the longest approximate match.
			 */
			if (on_net(addr,
				   ifp->int_net, ifp->int_mask)
			    && (maybe == 0
				|| ifp->int_mask > maybe->int_mask))
				maybe = ifp;
		}

		if (maybe != 0)
			return maybe;

		/* See if an interface has come up since we checked.
		 */
		ifinit();
	} while (twice++ == 0);

	return 0;
}


/* Return the classical netmask for an IP address.
 */
naddr
std_mask(naddr addr)
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


/* find the netmask that would be inferred by RIPv1 listeners
 * on the given interface
 */
naddr
ripv1_mask_net(naddr addr,		/* in network byte order */
	       struct interface *ifp1,	/* as seen on this interface */
	       struct interface *ifp2)	/* but not this interface */
{
	naddr mask = 0;

	if (addr == 0)			/* default always has 0 mask */
		return mask;

	if (ifp1 != 0) {
		/* If the target is that of the associated interface on which
		 * it arrived, then use the netmask of the interface.
		 */
		if (on_net(addr, ifp1->int_net, ifp1->int_std_mask))
			mask = ifp1->int_mask;

	} else {
		/* Examine all interfaces, and if it the target seems
		 * to have the same network number of an interface, use the
		 * netmask of that interface.  If there is more than one
		 * such interface, prefer the interface with the longest
		 * match.
		 */
		for (ifp1 = ifnet; ifp1 != 0; ifp1 = ifp1->int_next) {
			if (ifp1 != ifp2
			    && !(ifp1->int_if_flags & IFF_POINTOPOINT)
			    && on_net(addr,
				      ifp1->int_std_net, ifp1->int_std_mask)
			    && ifp1->int_mask > mask)
				mask = ifp1->int_mask;
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
		struct interface *ifp1,	/* as seen on this interface */
		struct interface *ifp2)	/* but not this interface */
{
	naddr mask = ripv1_mask_net(addr, ifp1, ifp2);


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


	if (TRACEACTIONS)
		trace_if("Del", ifp);

	if (!(ifp->int_state & IS_ALIAS)) {
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
				       &m, sizeof(m)) < 0)
				DBGERR(1,"setsockopt(IP_DROP_MEMBERSHIP"
				       " RIP)");
		}
		if (ifp->int_rip_sock >= 0) {
			(void)close(ifp->int_rip_sock);
			ifp->int_rip_sock = -1;
			fix_select();
		}
		set_rdisc_mg(ifp, 0);
	}

	/* Zap all routes associated with this interface.
	 * Assume routes just using gateways beyond this interface will
	 * timeout naturally, and have probably already died.
	 */
	ifp->int_state |= IS_BROKE;
	(void)rn_walktree(rhead, walk_bad, 0);
	ifbad_rdisc(ifp);

	if (!(ifp->int_state & IS_ALIAS)) {
		tot_interfaces--;
		if (0 == (ifp->int_state & (IS_NO_RIP_IN|IS_PASSIVE)))
			rip_interfaces--;
	}

	/* unlink and forget the interface */
	if (rip_sock_mcast == ifp)
		rip_sock_mcast = 0;
	if (ifp->int_next != 0)
		ifp->int_next->int_prev = ifp->int_prev;
	if (ifp->int_prev != 0)
		ifp->int_prev->int_next = ifp->int_next;
	else
		ifnet = ifp->int_next;

	if (!(ifp->int_state & IS_ALIAS)) {
		/* delete aliases of primary interface */
		for (ifp1 = ifnet; 0 != ifp1; ifp1 = ifp1->int_next) {
			if (!strcmp(ifp->int_name, ifp1->int_name))
				ifdel(ifp1);
		}
	}

	free(ifp);
}


/* Mark an interface dead.
 */
void
ifbad(struct interface *ifp,
      char *pat)
{
	if (ifp->int_state & IS_BROKE)
		return;

	if (pat)
		msglog(pat, ifp->int_name, naddr_ntoa(ifp->int_addr));

	LIM_SEC(ifinit_timer, now.tv_sec+CHECK_BAD_INTERVAL);

	ifp->int_state |= IS_BROKE;
	ifp->int_state &= ~(IS_RIP_QUERIED | IS_ACTIVE | IS_QUIET);
	ifp->int_quiet_time = now.tv_sec - MaxMaxAdvertiseInterval;
	ifp->int_data_ts = 0;

	trace_if("Chg", ifp);

	(void)rn_walktree(rhead, walk_bad, 0);

	ifbad_rdisc(ifp);
}


/* Mark an interface alive
 */
int					/* 1=it was dead */
ifok(struct interface *ifp,
     char *type)
{
	if (!(ifp->int_state & IS_BROKE))
		return 0;

	msglog("%sinterface %s to %s restored",
	       type, ifp->int_name, naddr_ntoa(ifp->int_addr));
	ifp->int_state &= ~IS_BROKE;
	ifp->int_data_ts = 0;

	ifok_rdisc(ifp);
	return 1;
}


struct rt_addrinfo rtinfo;

/* disassemble routing message
 */
void
rt_xaddrs(struct sockaddr *sa,
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


	bzero(rtinfo.rti_info, sizeof(rtinfo.rti_info));
	rtinfo.rti_addrs = addrs;

	for (i = 0; i < RTAX_MAX && sa < lim; i++) {
		if ((addrs & (1 << i)) == 0)
			continue;
#ifdef _HAVE_SA_LEN
		rtinfo.rti_info[i] = (sa->sa_len != 0) ? sa : &sa_zero;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(sa->sa_len));
#else
		rtinfo.rti_info[i] = sa;
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
#	define COMP_NOT_INET	0x01
#	define COMP_WIERD	0x02
#	define COMP_NOADDR	0x04
#	define COMP_NODST	0x08
#	define COMP_NOBADR	0x10
#	define COMP_NOMASK	0x20
#	define COMP_DUP		0x40

	struct interface ifs, ifs0, *ifp, *ifp1;
	struct rt_entry *rt;
	size_t needed;
	int mib[6];
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct sockaddr_dl *sdl;
	int in, ierr, out, oerr;
	struct intnet *intnetp;
#ifdef SIOCGIFMETRIC
	struct ifreq ifr;
#endif


	ifinit_timer.tv_sec = now.tv_sec + (supplier
					    ? CHECK_ACT_INTERVAL
					    : CHECK_QUIET_INTERVAL);

	/* mark all interfaces so we can get rid of thost that disappear */
	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next)
		ifp->int_state &= ~IS_CHECKED;

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
		if ((sysctl_buf = malloc(sysctl_buf_size = needed)) == 0)
			BADERR(1,"ifinit: malloc");
	}

	ifam_lim = (struct ifa_msghdr *)(sysctl_buf + needed);
	for (ifam = (struct ifa_msghdr *)sysctl_buf;
	     ifam < ifam_lim;
	     ifam = ifam2) {

		ifam2 = (struct ifa_msghdr*)((char*)ifam + ifam->ifam_msglen);

		if (ifam->ifam_type == RTM_IFINFO) {
			ifm = (struct if_msghdr *)ifam;
			bzero(&ifs0, sizeof(ifs0));
			ifs0.int_rip_sock = -1;
			ifs0.int_index = ifm->ifm_index;
			ifs0.int_if_flags = ifm->ifm_flags;
			ifs0.int_state = IS_CHECKED;
			ifs0.int_act_time = now.tv_sec;
			ifs0.int_quiet_time = (now.tv_sec
					       - MaxMaxAdvertiseInterval);
			ifs0.int_data_ts = now.tv_sec;
			ifs0.int_data_ipackets = ifm->ifm_data.ifi_ipackets;
			ifs0.int_data_ierrors = ifm->ifm_data.ifi_ierrors;
			ifs0.int_data_opackets = ifm->ifm_data.ifi_opackets;
			ifs0.int_data_oerrors = ifm->ifm_data.ifi_oerrors;
#ifdef sgi
			ifs0.int_data_odrops = ifm->ifm_data.ifi_odrops;
#endif
			sdl = (struct sockaddr_dl *)(ifm + 1);
			sdl->sdl_data[sdl->sdl_nlen] = 0;
			continue;
		}
		if (ifam->ifam_type != RTM_NEWADDR) {
			DBGERR(1,"ifinit: out of sync");
			continue;
		}

		rt_xaddrs((struct sockaddr *)(ifam+1),
			  (struct sockaddr *)ifam2,
			  ifam->ifam_addrs);

		if (RTINFO_IFA == 0) {
			if (iff_alive(ifs.int_if_flags)) {
				if (!(prev_complaints & COMP_NOADDR))
					msglog("%s has a bad address",
					       sdl->sdl_data);
				complaints |= COMP_NOADDR;
			}
			continue;
		}
		if (RTINFO_IFA->sa_family != AF_INET) {
			if (iff_alive(ifs.int_if_flags)) {
				if (!(prev_complaints & COMP_NOT_INET))
					trace_msg("%s: not AF_INET",
						  sdl->sdl_data);
				complaints |= COMP_NOT_INET;
			}
			continue;
		}

		bcopy(&ifs0, &ifs, sizeof(ifs0));
		ifs0.int_state |= IS_ALIAS; /* next will be an alias */

		ifs.int_addr = S_ADDR(RTINFO_IFA);

		if (ifs.int_if_flags & IFF_BROADCAST) {
			if (RTINFO_NETMASK == 0) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NOMASK))
						msglog("%s has no netmask",
						       sdl->sdl_data);
					complaints |= COMP_NOMASK;
				}
				continue;
			}
			ifs.int_mask = ntohl(S_ADDR(RTINFO_NETMASK));
			ifs.int_net = ntohl(ifs.int_addr) & ifs.int_mask;
			ifs.int_std_mask = std_mask(ifs.int_addr);
			if (ifs.int_mask != ifs.int_std_mask)
				ifs.int_state |= IS_SUBNET;

			if (RTINFO_BRD == 0) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NOBADR))
						msglog("%s has no"
						       " broadcast address",
						       sdl->sdl_data);
					complaints |= COMP_NOBADR;
				}
				continue;
			}
			ifs.int_brdaddr = S_ADDR(RTINFO_BRD);

		} else if (ifs.int_if_flags & IFF_POINTOPOINT) {
			if (RTINFO_BRD == 0
			    || RTINFO_BRD->sa_family != AF_INET) {
				if (iff_alive(ifs.int_if_flags)) {
					if (!(prev_complaints & COMP_NODST))
						msglog("%s has a bad"
						       " destination address",
						       sdl->sdl_data);
					complaints |= COMP_NODST;
				}
				continue;
			}
			ifs.int_dstaddr = S_ADDR(RTINFO_BRD);
			ifs.int_net = ntohl(ifs.int_dstaddr);
			ifs.int_mask = HOST_MASK;
			ifs.int_std_mask = std_mask(ifs.int_dstaddr);

		} else if (ifs.int_if_flags & IFF_LOOPBACK) {
			ifs.int_state |= IS_PASSIVE;
			ifs.int_dstaddr = ifs.int_addr;
			ifs.int_net = ntohl(ifs.int_dstaddr);
			ifs.int_mask = HOST_MASK;
			ifs.int_std_mask = std_mask(ifs.int_dstaddr);
			if (!foundloopback) {
				foundloopback = 1;
				loopaddr = ifs.int_addr;
			}

		} else {
			if (TRACEACTIONS
			    && !(prev_complaints & COMP_WIERD))
				msglog("%s is neither broadcast"
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
			ifs.int_metric = HOPCNT_INFINITY;
		} else {
			ifs.int_metric = ifr.ifr_metric+1;
		}
#else
		ifs.int_metric = ifam->ifam_metric+1;
#endif
		if (ifs.int_metric >= HOPCNT_INFINITY)
			ifs.int_metric = HOPCNT_INFINITY;

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
				trace_msg("interface %s has changed",
					  ifp->int_name);
				ifdel(ifp);
				ifp = 0;
			}
		}

		if (ifp != 0) {
			/* note interfaces that have been turned off
			 */
			if (!iff_alive(ifs.int_if_flags)) {
				if (iff_alive(ifp->int_if_flags))
					ifbad(ifp, "interface %s to %s"
					      " turned off");
				ifp->int_if_flags &= ~IFF_UP_RUNNING;
				continue;
			}
			/* or that were off and are now ok */
			if (!iff_alive(ifp->int_if_flags)) {
				ifp->int_if_flags |= IFF_UP_RUNNING;
				(void)ifok(ifp, "");
			}

			/* If it has been long enough,
			 * see if the interface is broken.
			 */
			if (now.tv_sec < ifp->int_data_ts+CHECK_BAD_INTERVAL)
				continue;

			in = ifs.int_data_ipackets - ifp->int_data_ipackets;
			ierr = ifs.int_data_ierrors - ifp->int_data_ierrors;
			out = ifs.int_data_opackets - ifp->int_data_opackets;
#ifdef sgi
			oerr = (ifs.int_data_oerrors - ifp->int_data_oerrors
				+ ifs.int_data_odrops - ifp->int_data_odrops);
#else
			oerr = ifs.int_data_oerrors - ifp->int_data_oerrors;
#endif

			ifp->int_data_ipackets = ifs.int_data_ipackets;
			ifp->int_data_ierrors = ifs.int_data_ierrors;
			ifp->int_data_opackets = ifs.int_data_opackets;
			ifp->int_data_oerrors = ifs.int_data_oerrors;
#ifdef sgi
			ifp->int_data_odrops = ifs.int_data_odrops;
#endif

			/* If the interface just awoke, restart the counters.
			 */
			if (ifp->int_data_ts == 0) {
				ifp->int_data_ts = now.tv_sec;
				continue;
			}
			ifp->int_data_ts = now.tv_sec;

			/* Withhold judgement when the short error
			 * counters wrap or the interface is reset.
			 */
			if (ierr < 0 || in < 0 || oerr < 0 || out < 0)
				continue;

			/* Withhold judgement when there is no traffic
			 */
			if (in == 0 && out == 0 && ierr == 0 && oerr == 0) {
				if (!(ifp->int_state & IS_QUIET)) {
					ifp->int_state |= IS_QUIET;
					ifp->int_quiet_time = now.tv_sec;
				}
				continue;
			}

			/* It is bad if input or output is not working
			 */
			if ((in <= ierr && ierr > 0)
			    || (out <= oerr && oerr > 0)) {
				ifbad(ifp,"interface %s to %s not working");
				continue;
			}

			/* otherwise, it is active and healthy
			 */
			ifp->int_act_time = now.tv_sec;
			ifp->int_state &= ~IS_QUIET;
			if (ifok(ifp, ""))
				addrouteforif(ifp);
			continue;
		}

		/* See if this new interface duplicates an existing
		 * interface.
		 */
		for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
			if (ifp->int_addr == ifs.int_addr
			    && ifp->int_mask == ifs.int_mask)
				break;
		}
		if (ifp != 0) {
			if (iff_alive(ifs.int_if_flags)) {
				if (!(prev_complaints & COMP_DUP))
					msglog("%s is duplicated by %s at %s",
					       sdl->sdl_data, ifp->int_name,
					       naddr_ntoa(ifp->int_addr));
				complaints |= COMP_DUP;
			}
			continue;
		}

		strncpy(ifs.int_name, sdl->sdl_data,
			MIN(sizeof(ifs.int_name)-1, sdl->sdl_nlen));

		get_parms(&ifs);

		ifok_rdisc(&ifs);

		/* create the interface */
		ifp = (struct interface *)malloc(sizeof(*ifp));
		if (ifp == 0)
			BADERR(1,"ifinit: out of memory");
		bcopy(&ifs, ifp, sizeof(*ifp));
		if (ifnet != 0) {
			ifp->int_next = ifnet;
			ifnet->int_prev = ifp;
		}
		ifnet = ifp;

		/* Count the # of directly connected networks.
		 */
		if (!(ifp->int_state & IS_ALIAS)) {
			if (!(ifp->int_if_flags & IFF_LOOPBACK))
				tot_interfaces++;
			if (0 == (ifp->int_state & (IS_NO_RIP_IN|IS_PASSIVE)))
				rip_interfaces++;
		}

		/* note dead interfaces */
		if (iff_alive(ifs.int_if_flags)) {
			set_rdisc_mg(ifp, 1);
		} else {
			LIM_SEC(ifinit_timer, now.tv_sec+CHECK_BAD_INTERVAL);
			ifp->int_state |= IS_BROKE;
		}

		if (TRACEACTIONS)
			trace_if("Add", ifp);

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
		del_static(myaddr, HOST_MASK, 0);
		rt = rtget(myaddr, HOST_MASK);
		if (rt != 0) {
			if (rt->rt_ifp != ifp
			    || rt->rt_router != loopaddr) {
				rtdelete(rt);
				rt = 0;
			} else {
				rtchange(rt, rt->rt_state | RS_MHOME,
					 loopaddr, loopaddr,
					 ifp->int_metric, 0,
					 ifp, rt->rt_time, 0);
			}
		}
		if (rt == 0)
			rtadd(myaddr, HOST_MASK, loopaddr, loopaddr,
			      ifp->int_metric, 0, RS_MHOME, ifp);
	}

	for (ifp = ifnet; ifp != 0; ifp = ifp1) {
		ifp1 = ifp->int_next;	/* because we may delete it */

		/* Forget any interfaces that have disappeared.
		 */
		if (!(ifp->int_state & (IS_CHECKED | IS_REMOTE))) {
			trace_msg("interface %s has disappeared",
				  ifp->int_name);
			ifdel(ifp);
			continue;
		}

		if (ifp->int_state & IS_BROKE)
			LIM_SEC(ifinit_timer, now.tv_sec+CHECK_BAD_INTERVAL);

		/* If we ever have a RIPv1 interface, assume we always will.
		 * It might come back if it ever goes away.
		 */
		if ((ifp->int_state & IS_NO_RIPV2_OUT)
		    && !(ifp->int_if_flags & IFF_LOOPBACK))
			have_ripv1 = 1;
	}

	/* add the authority interfaces */
	for (intnetp = intnets; intnetp!=0; intnetp = intnetp->intnet_next) {
		rt = rtget(intnetp->intnet_addr, intnetp->intnet_mask);
		if (rt != 0
		    && !(rt->rt_state & RS_IF)
		    && !(rt->rt_state & RS_NET_INT)) {
			rtdelete(rt);
			rt = 0;
		}
		if (rt == 0)
			rtadd(intnetp->intnet_addr, intnetp->intnet_mask,
			      loopaddr, loopaddr,
			      1, 0, RS_NET_INT, 0);
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
				rtchange(rt,((rt->rt_state | (RS_IF|RS_LOCAL))
					     & ~RS_NET_S),
					 loopaddr, loopaddr,
					 ifp1->int_metric, 0,
					 ifp1, rt->rt_time, 0);
			} else {
				rtadd(ifp->int_addr, HOST_MASK,
				      loopaddr, loopaddr,
				      ifp->int_metric, 0,
				      (RS_IF | RS_LOCAL), ifp);
			}
		}
	}

	prev_complaints = complaints;
}


static void
add_net_sub(struct interface *ifp,
	      naddr dst,
	      naddr mask,
	      u_int state)
{
	struct rt_entry *rt;

	rt = rtget(dst, mask);
	if (rt != 0) {
		if (0 != (rt->rt_state & (RS_STATIC | RS_LOCAL
					  | RS_MHOME | RS_GW)))
			return;

		if ((rt->rt_state & state) != state
		    || rt->rt_metric != NET_S_METRIC) {
			rtchange(rt, rt->rt_state | state,
				 rt->rt_gate, rt->rt_router,
				 NET_S_METRIC, rt->rt_tag,
				 rt->rt_ifp, rt->rt_time, 0);
		}
		return;
	}

	rtadd(dst, mask, ifp->int_addr, ifp->int_addr,
	      NET_S_METRIC, 0, state, ifp);
}


static void
check_net_sub(struct interface *ifp)
{
	struct interface *ifp2;
	struct rt_entry *rt;

	/* See if there are any RIPv1 listeners, to determine if
	 * we need to synthesize a network route for an interface
	 * on a subnet.
	 */
	for (ifp2 = ifnet; ifp2; ifp2 = ifp2->int_next) {
		if (ifp2 != ifp
		    && !(ifp->int_state & IS_PASSIVE)
		    && !(ifp->int_state & IS_NO_RIPV1_OUT)
		    && !on_net(ifp->int_addr,
			       ifp2->int_std_net,
			       ifp2->int_std_mask))
			break;
	}

	/* only if running RIPv1 somewhere */
	if (ifp2 != 0) {
		ifp->int_state |= IS_NEED_NET_SUB;
		add_net_sub(ifp, ifp->int_std_addr, ifp->int_std_mask,
			    RS_IF | RS_NET_SUB);

	} else {
		ifp->int_state &= ~IS_NEED_NET_SUB;

		rt = rtget(ifp->int_std_addr,
			   ifp->int_std_mask);
		if (rt != 0
		    && 0 != (rt->rt_state & RS_NET_S)
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
	naddr dst, mask;


	/* skip sick interfaces
	 */
	if (ifp->int_state & IS_BROKE)
		return;

	/* If the interface on a subnet, then install a RIPv1 route to
	 * the network as well (unless it is sick).
	 */
	if (ifp->int_metric != HOPCNT_INFINITY
	    && !(ifp->int_state & IS_PASSIVE)) {
		if (ifp->int_state & IS_SUBNET) {
			check_net_sub(ifp);

		} else if ((ifp->int_if_flags & IFF_POINTOPOINT)
			   && ridhosts) {

			/* The (dis)appearance of other interfaces can change
			 * the parent (sub)net.
			 */
			mask = ripv1_mask_net(ifp->int_dstaddr,0,ifp);
			if (mask != ifp->int_host_mask) {
				rt = rtget(ifp->int_host_addr,
					   ifp->int_host_mask);
				ifp->int_host_addr = htonl(ntohl(ifp->int_dstaddr)
							   & mask);
				ifp->int_host_mask = mask;
				if (rt != 0
				    && (rt->rt_state & RS_NET_S)
				    && rt->rt_ifp == ifp)
					rtbad_sub(rt);
			}

			add_net_sub(ifp, ifp->int_host_addr,
				    ifp->int_host_mask,
				    RS_IF | RS_NET_HOST);
		}
	}

	dst = (0 != (ifp->int_if_flags & (IFF_POINTOPOINT | IFF_LOOPBACK))
	       ? ifp->int_dstaddr
	       : htonl(ifp->int_net));

	/* We are finished if the right, main interface route exists.
	 * The right route must be for the right interface, not synthesized
	 * from a subnet, be a "gateway" or not as appropriate, and so forth.
	 */
	del_static(dst, ifp->int_mask, 0);
	rt = rtget(dst, ifp->int_mask);
	if (rt != 0) {
		if (rt->rt_ifp != ifp
		    || rt->rt_router != ifp->int_addr) {
			rtdelete(rt);
			rt = 0;
		} else {
			rtchange(rt, ((rt->rt_state | RS_IF)
				      & ~(RS_NET_S | RS_LOCAL)),
				 ifp->int_addr, ifp->int_addr,
				 ifp->int_metric, 0, ifp, now.tv_sec, 0);
		}
	}
	if (rt == 0) {
		if (ifp->int_transitions++ > 0)
			trace_msg("re-install interface %s", ifp->int_name);

		rtadd(dst, ifp->int_mask, ifp->int_addr, ifp->int_addr,
		      ifp->int_metric, 0, RS_IF, ifp);
	}
}

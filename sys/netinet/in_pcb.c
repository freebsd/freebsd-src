/*-
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2007 Robert N. M. Watson
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
 *	@(#)in_pcb.c	8.4 (Berkeley) 5/24/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_ipsec.h"
#include "opt_inet6.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */


#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/key.h>
#endif /* IPSEC */

#include <security/mac/mac_framework.h>

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
int	ipport_lowfirstauto  = IPPORT_RESERVED - 1;	/* 1023 */
int	ipport_lowlastauto = IPPORT_RESERVEDSTART;	/* 600 */
int	ipport_firstauto = IPPORT_HIFIRSTAUTO;		/* 49152 */
int	ipport_lastauto  = IPPORT_HILASTAUTO;		/* 65535 */
int	ipport_hifirstauto = IPPORT_HIFIRSTAUTO;	/* 49152 */
int	ipport_hilastauto  = IPPORT_HILASTAUTO;		/* 65535 */

/*
 * Reserved ports accessible only to root. There are significant
 * security considerations that must be accounted for when changing these,
 * but the security benefits can be great. Please be careful.
 */
int	ipport_reservedhigh = IPPORT_RESERVED - 1;	/* 1023 */
int	ipport_reservedlow = 0;

/* Variables dealing with random ephemeral port allocation. */
int	ipport_randomized = 1;	/* user controlled via sysctl */
int	ipport_randomcps = 10;	/* user controlled via sysctl */
int	ipport_randomtime = 45;	/* user controlled via sysctl */
int	ipport_stoprandom = 0;	/* toggled by ipport_tick */
int	ipport_tcpallocs;
int	ipport_tcplastcount;

#define RANGECHK(var, min, max) \
	if ((var) < (min)) { (var) = (min); } \
	else if ((var) > (max)) { (var) = (max); }

static int
sysctl_net_ipport_check(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (error == 0) {
		RANGECHK(ipport_lowfirstauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(ipport_lowlastauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(ipport_firstauto, IPPORT_RESERVED, IPPORT_MAX);
		RANGECHK(ipport_lastauto, IPPORT_RESERVED, IPPORT_MAX);
		RANGECHK(ipport_hifirstauto, IPPORT_RESERVED, IPPORT_MAX);
		RANGECHK(ipport_hilastauto, IPPORT_RESERVED, IPPORT_MAX);
	}
	return (error);
}

#undef RANGECHK

SYSCTL_NODE(_net_inet_ip, IPPROTO_IP, portrange, CTLFLAG_RW, 0, "IP Ports");

SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowfirst, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lowfirstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowlast, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lowlastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, first, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_firstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, last, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hifirst, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_hifirstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hilast, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_hilastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, reservedhigh,
	   CTLFLAG_RW|CTLFLAG_SECURE, &ipport_reservedhigh, 0, "");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, reservedlow,
	   CTLFLAG_RW|CTLFLAG_SECURE, &ipport_reservedlow, 0, "");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, randomized, CTLFLAG_RW,
	   &ipport_randomized, 0, "Enable random port allocation");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, randomcps, CTLFLAG_RW,
	   &ipport_randomcps, 0, "Maximum number of random port "
	   "allocations before switching to a sequental one");
SYSCTL_INT(_net_inet_ip_portrange, OID_AUTO, randomtime, CTLFLAG_RW,
	   &ipport_randomtime, 0, "Minimum time to keep sequental port "
	   "allocation before switching to a random one");

/*
 * in_pcb.c: manage the Protocol Control Blocks.
 *
 * NOTE: It is assumed that most of these functions will be called with
 * the pcbinfo lock held, and often, the inpcb lock held, as these utility
 * functions often modify hash chains or addresses in pcbs.
 */

/*
 * Allocate a PCB and associate it with the socket.
 * On success return with the PCB locked.
 */
int
in_pcballoc(struct socket *so, struct inpcbinfo *pcbinfo)
{
	struct inpcb *inp;
	int error;

	INP_INFO_WLOCK_ASSERT(pcbinfo);
	error = 0;
	inp = uma_zalloc(pcbinfo->ipi_zone, M_NOWAIT);
	if (inp == NULL)
		return (ENOBUFS);
	bzero(inp, inp_zero_size);
	inp->inp_pcbinfo = pcbinfo;
	inp->inp_socket = so;
#ifdef MAC
	error = mac_inpcb_init(inp, M_NOWAIT);
	if (error != 0)
		goto out;
	SOCK_LOCK(so);
	mac_inpcb_create(so, inp);
	SOCK_UNLOCK(so);
#endif

#ifdef IPSEC
	error = ipsec_init_policy(so, &inp->inp_sp);
	if (error != 0)
		goto out;
#endif /*IPSEC*/
#ifdef INET6
	if (INP_SOCKAF(so) == AF_INET6) {
		inp->inp_vflag |= INP_IPV6PROTO;
		if (ip6_v6only)
			inp->inp_flags |= IN6P_IPV6_V6ONLY;
	}
#endif
	LIST_INSERT_HEAD(pcbinfo->ipi_listhead, inp, inp_list);
	pcbinfo->ipi_count++;
	so->so_pcb = (caddr_t)inp;
#ifdef INET6
	if (ip6_auto_flowlabel)
		inp->inp_flags |= IN6P_AUTOFLOWLABEL;
#endif
	INP_LOCK(inp);
	inp->inp_gencnt = ++pcbinfo->ipi_gencnt;
	
#if defined(IPSEC) || defined(MAC)
out:
	if (error != 0)
		uma_zfree(pcbinfo->ipi_zone, inp);
#endif
	return (error);
}

int
in_pcbbind(struct inpcb *inp, struct sockaddr *nam, struct ucred *cred)
{
	int anonport, error;

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	if (inp->inp_lport != 0 || inp->inp_laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	anonport = inp->inp_lport == 0 && (nam == NULL ||
	    ((struct sockaddr_in *)nam)->sin_port == 0);
	error = in_pcbbind_setup(inp, nam, &inp->inp_laddr.s_addr,
	    &inp->inp_lport, cred);
	if (error)
		return (error);
	if (in_pcbinshash(inp) != 0) {
		inp->inp_laddr.s_addr = INADDR_ANY;
		inp->inp_lport = 0;
		return (EAGAIN);
	}
	if (anonport)
		inp->inp_flags |= INP_ANONPORT;
	return (0);
}

/*
 * Set up a bind operation on a PCB, performing port allocation
 * as required, but do not actually modify the PCB. Callers can
 * either complete the bind by setting inp_laddr/inp_lport and
 * calling in_pcbinshash(), or they can just use the resulting
 * port and address to authorise the sending of a once-off packet.
 *
 * On error, the values of *laddrp and *lportp are not changed.
 */
int
in_pcbbind_setup(struct inpcb *inp, struct sockaddr *nam, in_addr_t *laddrp,
    u_short *lportp, struct ucred *cred)
{
	struct socket *so = inp->inp_socket;
	unsigned short *lastport;
	struct sockaddr_in *sin;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct in_addr laddr;
	u_short lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error, prison = 0;
	int dorandom;

	INP_INFO_WLOCK_ASSERT(pcbinfo);
	INP_LOCK_ASSERT(inp);

	if (TAILQ_EMPTY(&in_ifaddrhead)) /* XXX broken! */
		return (EADDRNOTAVAIL);
	laddr.s_addr = *laddrp;
	if (nam != NULL && laddr.s_addr != INADDR_ANY)
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
		wild = INPLOOKUP_WILDCARD;
	if (nam) {
		sin = (struct sockaddr_in *)nam;
		if (nam->sa_len != sizeof (*sin))
			return (EINVAL);
#ifdef notdef
		/*
		 * We should check the family, but old programs
		 * incorrectly fail to initialize it.
		 */
		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);
#endif
		if (sin->sin_addr.s_addr != INADDR_ANY)
			if (prison_ip(cred, 0, &sin->sin_addr.s_addr))
				return(EINVAL);
		if (sin->sin_port != *lportp) {
			/* Don't allow the port to change. */
			if (*lportp != 0)
				return (EINVAL);
			lport = sin->sin_port;
		}
		/* NB: lport is left as 0 if the port isn't being changed. */
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow complete duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (sin->sin_addr.s_addr != INADDR_ANY) {
			sin->sin_port = 0;		/* yech... */
			bzero(&sin->sin_zero, sizeof(sin->sin_zero));
			if (ifa_ifwithaddr((struct sockaddr *)sin) == 0)
				return (EADDRNOTAVAIL);
		}
		laddr = sin->sin_addr;
		if (lport) {
			struct inpcb *t;
			struct tcptw *tw;

			/* GROSS */
			if (ntohs(lport) <= ipport_reservedhigh &&
			    ntohs(lport) >= ipport_reservedlow &&
			    priv_check_cred(cred, PRIV_NETINET_RESERVEDPORT,
			    0))
				return (EACCES);
			if (jailed(cred))
				prison = 1;
			if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
			    priv_check_cred(so->so_cred,
			    PRIV_NETINET_REUSEPORT, 0) != 0) {
				t = in_pcblookup_local(inp->inp_pcbinfo,
				    sin->sin_addr, lport,
				    prison ? 0 :  INPLOOKUP_WILDCARD);
	/*
	 * XXX
	 * This entire block sorely needs a rewrite.
	 */
				if (t &&
				    ((t->inp_vflag & INP_TIMEWAIT) == 0) &&
				    (so->so_type != SOCK_STREAM ||
				     ntohl(t->inp_faddr.s_addr) == INADDR_ANY) &&
				    (ntohl(sin->sin_addr.s_addr) != INADDR_ANY ||
				     ntohl(t->inp_laddr.s_addr) != INADDR_ANY ||
				     (t->inp_socket->so_options &
					 SO_REUSEPORT) == 0) &&
				    (so->so_cred->cr_uid !=
				     t->inp_socket->so_cred->cr_uid))
					return (EADDRINUSE);
			}
			if (prison && prison_ip(cred, 0, &sin->sin_addr.s_addr))
				return (EADDRNOTAVAIL);
			t = in_pcblookup_local(pcbinfo, sin->sin_addr,
			    lport, prison ? 0 : wild);
			if (t && (t->inp_vflag & INP_TIMEWAIT)) {
				/*
				 * XXXRW: If an incpb has had its timewait
				 * state recycled, we treat the address as
				 * being in use (for now).  This is better
				 * than a panic, but not desirable.
				 */
				tw = intotw(inp);
				if (tw == NULL ||
				    (reuseport & tw->tw_so_options) == 0)
					return (EADDRINUSE);
			} else if (t &&
			    (reuseport & t->inp_socket->so_options) == 0) {
#ifdef INET6
				if (ntohl(sin->sin_addr.s_addr) !=
				    INADDR_ANY ||
				    ntohl(t->inp_laddr.s_addr) !=
				    INADDR_ANY ||
				    INP_SOCKAF(so) ==
				    INP_SOCKAF(t->inp_socket))
#endif
				return (EADDRINUSE);
			}
		}
	}
	if (*lportp != 0)
		lport = *lportp;
	if (lport == 0) {
		u_short first, last;
		int count;

		if (laddr.s_addr != INADDR_ANY)
			if (prison_ip(cred, 0, &laddr.s_addr))
				return (EINVAL);

		if (inp->inp_flags & INP_HIGHPORT) {
			first = ipport_hifirstauto;	/* sysctl */
			last  = ipport_hilastauto;
			lastport = &pcbinfo->ipi_lasthi;
		} else if (inp->inp_flags & INP_LOWPORT) {
			error = priv_check_cred(cred,
			    PRIV_NETINET_RESERVEDPORT, 0);
			if (error)
				return error;
			first = ipport_lowfirstauto;	/* 1023 */
			last  = ipport_lowlastauto;	/* 600 */
			lastport = &pcbinfo->ipi_lastlow;
		} else {
			first = ipport_firstauto;	/* sysctl */
			last  = ipport_lastauto;
			lastport = &pcbinfo->ipi_lastport;
		}
		/*
		 * For UDP, use random port allocation as long as the user
		 * allows it.  For TCP (and as of yet unknown) connections,
		 * use random port allocation only if the user allows it AND
		 * ipport_tick() allows it.
		 */
		if (ipport_randomized &&
			(!ipport_stoprandom || pcbinfo == &udbinfo))
			dorandom = 1;
		else
			dorandom = 0;
		/*
		 * It makes no sense to do random port allocation if
		 * we have the only port available.
		 */
		if (first == last)
			dorandom = 0;
		/* Make sure to not include UDP packets in the count. */
		if (pcbinfo != &udbinfo)
			ipport_tcpallocs++;
		/*
		 * Simple check to ensure all ports are not used up causing
		 * a deadlock here.
		 *
		 * We split the two cases (up and down) so that the direction
		 * is not being tested on each round of the loop.
		 */
		if (first > last) {
			/*
			 * counting down
			 */
			if (dorandom)
				*lastport = first -
					    (arc4random() % (first - last));
			count = first - last;

			do {
				if (count-- < 0)	/* completely used? */
					return (EADDRNOTAVAIL);
				--*lastport;
				if (*lastport > first || *lastport < last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup_local(pcbinfo, laddr, lport,
			    wild));
		} else {
			/*
			 * counting up
			 */
			if (dorandom)
				*lastport = first +
					    (arc4random() % (last - first));
			count = last - first;

			do {
				if (count-- < 0)	/* completely used? */
					return (EADDRNOTAVAIL);
				++*lastport;
				if (*lastport < first || *lastport > last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup_local(pcbinfo, laddr, lport,
			    wild));
		}
	}
	if (prison_ip(cred, 0, &laddr.s_addr))
		return (EINVAL);
	*laddrp = laddr.s_addr;
	*lportp = lport;
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(struct inpcb *inp, struct sockaddr *nam, struct ucred *cred)
{
	u_short lport, fport;
	in_addr_t laddr, faddr;
	int anonport, error;

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	lport = inp->inp_lport;
	laddr = inp->inp_laddr.s_addr;
	anonport = (lport == 0);
	error = in_pcbconnect_setup(inp, nam, &laddr, &lport, &faddr, &fport,
	    NULL, cred);
	if (error)
		return (error);

	/* Do the initial binding of the local address if required. */
	if (inp->inp_laddr.s_addr == INADDR_ANY && inp->inp_lport == 0) {
		inp->inp_lport = lport;
		inp->inp_laddr.s_addr = laddr;
		if (in_pcbinshash(inp) != 0) {
			inp->inp_laddr.s_addr = INADDR_ANY;
			inp->inp_lport = 0;
			return (EAGAIN);
		}
	}

	/* Commit the remaining changes. */
	inp->inp_lport = lport;
	inp->inp_laddr.s_addr = laddr;
	inp->inp_faddr.s_addr = faddr;
	inp->inp_fport = fport;
	in_pcbrehash(inp);

	if (anonport)
		inp->inp_flags |= INP_ANONPORT;
	return (0);
}

/*
 * Set up for a connect from a socket to the specified address.
 * On entry, *laddrp and *lportp should contain the current local
 * address and port for the PCB; these are updated to the values
 * that should be placed in inp_laddr and inp_lport to complete
 * the connect.
 *
 * On success, *faddrp and *fportp will be set to the remote address
 * and port. These are not updated in the error case.
 *
 * If the operation fails because the connection already exists,
 * *oinpp will be set to the PCB of that connection so that the
 * caller can decide to override it. In all other cases, *oinpp
 * is set to NULL.
 */
int
in_pcbconnect_setup(struct inpcb *inp, struct sockaddr *nam,
    in_addr_t *laddrp, u_short *lportp, in_addr_t *faddrp, u_short *fportp,
    struct inpcb **oinpp, struct ucred *cred)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;
	struct in_ifaddr *ia;
	struct sockaddr_in sa;
	struct ucred *socred;
	struct inpcb *oinp;
	struct in_addr laddr, faddr;
	u_short lport, fport;
	int error;

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	if (oinpp != NULL)
		*oinpp = NULL;
	if (nam->sa_len != sizeof (*sin))
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	laddr.s_addr = *laddrp;
	lport = *lportp;
	faddr = sin->sin_addr;
	fport = sin->sin_port;
	socred = inp->inp_socket->so_cred;
	if (laddr.s_addr == INADDR_ANY && jailed(socred)) {
		bzero(&sa, sizeof(sa));
		sa.sin_addr.s_addr = htonl(prison_getip(socred));
		sa.sin_len = sizeof(sa);
		sa.sin_family = AF_INET;
		error = in_pcbbind_setup(inp, (struct sockaddr *)&sa,
		    &laddr.s_addr, &lport, cred);
		if (error)
			return (error);
	}
	if (!TAILQ_EMPTY(&in_ifaddrhead)) {
		/*
		 * If the destination address is INADDR_ANY,
		 * use the primary local address.
		 * If the supplied address is INADDR_BROADCAST,
		 * and the primary interface supports broadcast,
		 * choose the broadcast address for that interface.
		 */
		if (faddr.s_addr == INADDR_ANY)
			faddr = IA_SIN(TAILQ_FIRST(&in_ifaddrhead))->sin_addr;
		else if (faddr.s_addr == (u_long)INADDR_BROADCAST &&
		    (TAILQ_FIRST(&in_ifaddrhead)->ia_ifp->if_flags &
		    IFF_BROADCAST))
			faddr = satosin(&TAILQ_FIRST(
			    &in_ifaddrhead)->ia_broadaddr)->sin_addr;
	}
	if (laddr.s_addr == INADDR_ANY) {
		ia = (struct in_ifaddr *)0;
		/*
		 * If route is known our src addr is taken from the i/f,
		 * else punt.
		 *
		 * Find out route to destination
		 */
		if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0)
			ia = ip_rtaddr(faddr);
		/*
		 * If we found a route, use the address corresponding to
		 * the outgoing interface.
		 * 
		 * Otherwise assume faddr is reachable on a directly connected
		 * network and try to find a corresponding interface to take
		 * the source address from.
		 */
		if (ia == 0) {
			bzero(&sa, sizeof(sa));
			sa.sin_addr = faddr;
			sa.sin_len = sizeof(sa);
			sa.sin_family = AF_INET;

			ia = ifatoia(ifa_ifwithdstaddr(sintosa(&sa)));
			if (ia == 0)
				ia = ifatoia(ifa_ifwithnet(sintosa(&sa)));
			if (ia == 0)
				return (ENETUNREACH);
		}
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(ntohl(faddr.s_addr)) &&
		    inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
					if (ia->ia_ifp == ifp)
						break;
				if (ia == 0)
					return (EADDRNOTAVAIL);
			}
		}
		laddr = ia->ia_addr.sin_addr;
	}

	oinp = in_pcblookup_hash(inp->inp_pcbinfo, faddr, fport, laddr, lport,
	    0, NULL);
	if (oinp != NULL) {
		if (oinpp != NULL)
			*oinpp = oinp;
		return (EADDRINUSE);
	}
	if (lport == 0) {
		error = in_pcbbind_setup(inp, NULL, &laddr.s_addr, &lport,
		    cred);
		if (error)
			return (error);
	}
	*laddrp = laddr.s_addr;
	*lportp = lport;
	*faddrp = faddr.s_addr;
	*fportp = fport;
	return (0);
}

void
in_pcbdisconnect(struct inpcb *inp)
{

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_fport = 0;
	in_pcbrehash(inp);
}

/*
 * In the old world order, in_pcbdetach() served two functions: to detach the
 * pcb from the socket/potentially free the socket, and to free the pcb
 * itself.  In the new world order, the protocol code is responsible for
 * managing the relationship with the socket, and this code simply frees the
 * pcb.
 */
void
in_pcbdetach(struct inpcb *inp)
{

	KASSERT(inp->inp_socket != NULL, ("in_pcbdetach: inp_socket == NULL"));
	inp->inp_socket->so_pcb = NULL;
	inp->inp_socket = NULL;
}

void
in_pcbfree(struct inpcb *inp)
{
	struct inpcbinfo *ipi = inp->inp_pcbinfo;

	KASSERT(inp->inp_socket == NULL, ("in_pcbfree: inp_socket != NULL"));
	INP_INFO_WLOCK_ASSERT(ipi);
	INP_LOCK_ASSERT(inp);

#ifdef IPSEC
	ipsec4_delete_pcbpolicy(inp);
#endif /*IPSEC*/
	inp->inp_gencnt = ++ipi->ipi_gencnt;
	in_pcbremlists(inp);
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
	if (inp->inp_moptions != NULL)
		inp_freemoptions(inp->inp_moptions);
	inp->inp_vflag = 0;
	
#ifdef MAC
	mac_inpcb_destroy(inp);
#endif
	INP_UNLOCK(inp);
	uma_zfree(ipi->ipi_zone, inp);
}

/*
 * TCP needs to maintain its inpcb structure after the TCP connection has
 * been torn down.  However, it must be disconnected from the inpcb hashes as
 * it must not prevent binding of future connections to the same port/ip
 * combination by other inpcbs.
 */
void
in_pcbdrop(struct inpcb *inp)
{

	INP_INFO_WLOCK_ASSERT(inp->inp_pcbinfo);
	INP_LOCK_ASSERT(inp);

	inp->inp_vflag |= INP_DROPPED;
	if (inp->inp_lport) {
		struct inpcbport *phd = inp->inp_phd;

		LIST_REMOVE(inp, inp_hash);
		LIST_REMOVE(inp, inp_portlist);
		if (LIST_FIRST(&phd->phd_pcblist) == NULL) {
			LIST_REMOVE(phd, phd_hash);
			free(phd, M_PCB);
		}
		inp->inp_lport = 0;
	}
}

/*
 * Common routines to return the socket addresses associated with inpcbs.
 */
struct sockaddr *
in_sockaddr(in_port_t port, struct in_addr *addr_p)
{
	struct sockaddr_in *sin;

	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		M_WAITOK | M_ZERO);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr = *addr_p;
	sin->sin_port = port;

	return (struct sockaddr *)sin;
}

int
in_getsockaddr(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *inp;
	struct in_addr addr;
	in_port_t port;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in_getsockaddr: inp == NULL"));

	INP_LOCK(inp);
	port = inp->inp_lport;
	addr = inp->inp_laddr;
	INP_UNLOCK(inp);

	*nam = in_sockaddr(port, &addr);
	return 0;
}

int
in_getpeeraddr(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *inp;
	struct in_addr addr;
	in_port_t port;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in_getpeeraddr: inp == NULL"));

	INP_LOCK(inp);
	port = inp->inp_fport;
	addr = inp->inp_faddr;
	INP_UNLOCK(inp);

	*nam = in_sockaddr(port, &addr);
	return 0;
}

void
in_pcbnotifyall(struct inpcbinfo *pcbinfo, struct in_addr faddr, int errno,
    struct inpcb *(*notify)(struct inpcb *, int))
{
	struct inpcb *inp, *ninp;
	struct inpcbhead *head;

	INP_INFO_WLOCK(pcbinfo);
	head = pcbinfo->ipi_listhead;
	for (inp = LIST_FIRST(head); inp != NULL; inp = ninp) {
		INP_LOCK(inp);
		ninp = LIST_NEXT(inp, inp_list);
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV4) == 0) {
			INP_UNLOCK(inp);
			continue;
		}
#endif
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == NULL) {
			INP_UNLOCK(inp);
			continue;
		}
		if ((*notify)(inp, errno))
			INP_UNLOCK(inp);
	}
	INP_INFO_WUNLOCK(pcbinfo);
}

void
in_pcbpurgeif0(struct inpcbinfo *pcbinfo, struct ifnet *ifp)
{
	struct inpcb *inp;
	struct ip_moptions *imo;
	int i, gap;

	INP_INFO_RLOCK(pcbinfo);
	LIST_FOREACH(inp, pcbinfo->ipi_listhead, inp_list) {
		INP_LOCK(inp);
		imo = inp->inp_moptions;
		if ((inp->inp_vflag & INP_IPV4) &&
		    imo != NULL) {
			/*
			 * Unselect the outgoing interface if it is being
			 * detached.
			 */
			if (imo->imo_multicast_ifp == ifp)
				imo->imo_multicast_ifp = NULL;

			/*
			 * Drop multicast group membership if we joined
			 * through the interface being detached.
			 */
			for (i = 0, gap = 0; i < imo->imo_num_memberships;
			    i++) {
				if (imo->imo_membership[i]->inm_ifp == ifp) {
					in_delmulti(imo->imo_membership[i]);
					gap++;
				} else if (gap != 0)
					imo->imo_membership[i - gap] =
					    imo->imo_membership[i];
			}
			imo->imo_num_memberships -= gap;
		}
		INP_UNLOCK(inp);
	}
	INP_INFO_RUNLOCK(pcbinfo);
}

/*
 * Lookup a PCB based on the local address and port.
 */
#define INP_LOOKUP_MAPPED_PCB_COST	3
struct inpcb *
in_pcblookup_local(struct inpcbinfo *pcbinfo, struct in_addr laddr,
    u_int lport_arg, int wild_okay)
{
	struct inpcb *inp;
#ifdef INET6
	int matchwild = 3 + INP_LOOKUP_MAPPED_PCB_COST;
#else
	int matchwild = 3;
#endif
	int wildcard;
	u_short lport = lport_arg;

	INP_INFO_WLOCK_ASSERT(pcbinfo);

	if (!wild_okay) {
		struct inpcbhead *head;
		/*
		 * Look for an unconnected (wildcard foreign addr) PCB that
		 * matches the local address and port we're looking for.
		 */
		head = &pcbinfo->ipi_hashbase[INP_PCBHASH(INADDR_ANY, lport,
		    0, pcbinfo->ipi_hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_faddr.s_addr == INADDR_ANY &&
			    inp->inp_laddr.s_addr == laddr.s_addr &&
			    inp->inp_lport == lport) {
				/*
				 * Found.
				 */
				return (inp);
			}
		}
		/*
		 * Not found.
		 */
		return (NULL);
	} else {
		struct inpcbporthead *porthash;
		struct inpcbport *phd;
		struct inpcb *match = NULL;
		/*
		 * Best fit PCB lookup.
		 *
		 * First see if this local port is in use by looking on the
		 * port hash list.
		 */
		porthash = &pcbinfo->ipi_porthashbase[INP_PCBPORTHASH(lport,
		    pcbinfo->ipi_porthashmask)];
		LIST_FOREACH(phd, porthash, phd_hash) {
			if (phd->phd_port == lport)
				break;
		}
		if (phd != NULL) {
			/*
			 * Port is in use by one or more PCBs. Look for best
			 * fit.
			 */
			LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
				wildcard = 0;
#ifdef INET6
				if ((inp->inp_vflag & INP_IPV4) == 0)
					continue;
				/*
				 * We never select the PCB that has
				 * INP_IPV6 flag and is bound to :: if
				 * we have another PCB which is bound
				 * to 0.0.0.0.  If a PCB has the
				 * INP_IPV6 flag, then we set its cost
				 * higher than IPv4 only PCBs.
				 *
				 * Note that the case only happens
				 * when a socket is bound to ::, under
				 * the condition that the use of the
				 * mapped address is allowed.
				 */
				if ((inp->inp_vflag & INP_IPV6) != 0)
					wildcard += INP_LOOKUP_MAPPED_PCB_COST;
#endif
				if (inp->inp_faddr.s_addr != INADDR_ANY)
					wildcard++;
				if (inp->inp_laddr.s_addr != INADDR_ANY) {
					if (laddr.s_addr == INADDR_ANY)
						wildcard++;
					else if (inp->inp_laddr.s_addr != laddr.s_addr)
						continue;
				} else {
					if (laddr.s_addr != INADDR_ANY)
						wildcard++;
				}
				if (wildcard < matchwild) {
					match = inp;
					matchwild = wildcard;
					if (matchwild == 0) {
						break;
					}
				}
			}
		}
		return (match);
	}
}
#undef INP_LOOKUP_MAPPED_PCB_COST

/*
 * Lookup PCB in hash list.
 */
struct inpcb *
in_pcblookup_hash(struct inpcbinfo *pcbinfo, struct in_addr faddr,
    u_int fport_arg, struct in_addr laddr, u_int lport_arg, int wildcard,
    struct ifnet *ifp)
{
	struct inpcbhead *head;
	struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;

	INP_INFO_RLOCK_ASSERT(pcbinfo);

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->ipi_hashbase[INP_PCBHASH(faddr.s_addr, lport, fport,
	    pcbinfo->ipi_hashmask)];
	LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
		if ((inp->inp_vflag & INP_IPV4) == 0)
			continue;
#endif
		if (inp->inp_faddr.s_addr == faddr.s_addr &&
		    inp->inp_laddr.s_addr == laddr.s_addr &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport)
			return (inp);
	}

	/*
	 * Then look for a wildcard match, if requested.
	 */
	if (wildcard) {
		struct inpcb *local_wild = NULL;
#ifdef INET6
		struct inpcb *local_wild_mapped = NULL;
#endif

		head = &pcbinfo->ipi_hashbase[INP_PCBHASH(INADDR_ANY, lport,
		    0, pcbinfo->ipi_hashmask)];
		LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_faddr.s_addr == INADDR_ANY &&
			    inp->inp_lport == lport) {
				if (ifp && ifp->if_type == IFT_FAITH &&
				    (inp->inp_flags & INP_FAITH) == 0)
					continue;
				if (inp->inp_laddr.s_addr == laddr.s_addr)
					return (inp);
				else if (inp->inp_laddr.s_addr == INADDR_ANY) {
#ifdef INET6
					if (INP_CHECK_SOCKAF(inp->inp_socket,
							     AF_INET6))
						local_wild_mapped = inp;
					else
#endif
						local_wild = inp;
				}
			}
		}
#ifdef INET6
		if (local_wild == NULL)
			return (local_wild_mapped);
#endif
		return (local_wild);
	}
	return (NULL);
}

/*
 * Insert PCB onto various hash lists.
 */
int
in_pcbinshash(struct inpcb *inp)
{
	struct inpcbhead *pcbhash;
	struct inpcbporthead *pcbporthash;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbport *phd;
	u_int32_t hashkey_faddr;

	INP_INFO_WLOCK_ASSERT(pcbinfo);
	INP_LOCK_ASSERT(inp);

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6)
		hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX */;
	else
#endif /* INET6 */
	hashkey_faddr = inp->inp_faddr.s_addr;

	pcbhash = &pcbinfo->ipi_hashbase[INP_PCBHASH(hashkey_faddr,
		 inp->inp_lport, inp->inp_fport, pcbinfo->ipi_hashmask)];

	pcbporthash = &pcbinfo->ipi_porthashbase[
	    INP_PCBPORTHASH(inp->inp_lport, pcbinfo->ipi_porthashmask)];

	/*
	 * Go through port list and look for a head for this lport.
	 */
	LIST_FOREACH(phd, pcbporthash, phd_hash) {
		if (phd->phd_port == inp->inp_lport)
			break;
	}
	/*
	 * If none exists, malloc one and tack it on.
	 */
	if (phd == NULL) {
		MALLOC(phd, struct inpcbport *, sizeof(struct inpcbport), M_PCB, M_NOWAIT);
		if (phd == NULL) {
			return (ENOBUFS); /* XXX */
		}
		phd->phd_port = inp->inp_lport;
		LIST_INIT(&phd->phd_pcblist);
		LIST_INSERT_HEAD(pcbporthash, phd, phd_hash);
	}
	inp->inp_phd = phd;
	LIST_INSERT_HEAD(&phd->phd_pcblist, inp, inp_portlist);
	LIST_INSERT_HEAD(pcbhash, inp, inp_hash);
	return (0);
}

/*
 * Move PCB to the proper hash bucket when { faddr, fport } have  been
 * changed. NOTE: This does not handle the case of the lport changing (the
 * hashed port list would have to be updated as well), so the lport must
 * not change after in_pcbinshash() has been called.
 */
void
in_pcbrehash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbhead *head;
	u_int32_t hashkey_faddr;

	INP_INFO_WLOCK_ASSERT(pcbinfo);
	INP_LOCK_ASSERT(inp);

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6)
		hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX */;
	else
#endif /* INET6 */
	hashkey_faddr = inp->inp_faddr.s_addr;

	head = &pcbinfo->ipi_hashbase[INP_PCBHASH(hashkey_faddr,
		inp->inp_lport, inp->inp_fport, pcbinfo->ipi_hashmask)];

	LIST_REMOVE(inp, inp_hash);
	LIST_INSERT_HEAD(head, inp, inp_hash);
}

/*
 * Remove PCB from various lists.
 */
void
in_pcbremlists(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;

	INP_INFO_WLOCK_ASSERT(pcbinfo);
	INP_LOCK_ASSERT(inp);

	inp->inp_gencnt = ++pcbinfo->ipi_gencnt;
	if (inp->inp_lport) {
		struct inpcbport *phd = inp->inp_phd;

		LIST_REMOVE(inp, inp_hash);
		LIST_REMOVE(inp, inp_portlist);
		if (LIST_FIRST(&phd->phd_pcblist) == NULL) {
			LIST_REMOVE(phd, phd_hash);
			free(phd, M_PCB);
		}
	}
	LIST_REMOVE(inp, inp_list);
	pcbinfo->ipi_count--;
}

/*
 * A set label operation has occurred at the socket layer, propagate the
 * label change into the in_pcb for the socket.
 */
void
in_pcbsosetlabel(struct socket *so)
{
#ifdef MAC
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in_pcbsosetlabel: so->so_pcb == NULL"));

	INP_LOCK(inp);
	SOCK_LOCK(so);
	mac_inpcb_sosetlabel(so, inp);
	SOCK_UNLOCK(so);
	INP_UNLOCK(inp);
#endif
}

/*
 * ipport_tick runs once per second, determining if random port allocation
 * should be continued.  If more than ipport_randomcps ports have been
 * allocated in the last second, then we return to sequential port
 * allocation. We return to random allocation only once we drop below
 * ipport_randomcps for at least ipport_randomtime seconds.
 */
void
ipport_tick(void *xtp)
{

	if (ipport_tcpallocs <= ipport_tcplastcount + ipport_randomcps) {
		if (ipport_stoprandom > 0)
			ipport_stoprandom--;
	} else
		ipport_stoprandom = ipport_randomtime;
	ipport_tcplastcount = ipport_tcpallocs;
	callout_reset(&ipport_tick_callout, hz, ipport_tick, NULL);
}

#ifdef DDB
static void
db_print_indent(int indent)
{
	int i;

	for (i = 0; i < indent; i++)
		db_printf(" ");
}

static void
db_print_inconninfo(struct in_conninfo *inc, const char *name, int indent)
{
	char faddr_str[48], laddr_str[48];

	db_print_indent(indent);
	db_printf("%s at %p\n", name, inc);

	indent += 2;

#ifdef INET6
	if (inc->inc_flags == 1) {
		/* IPv6. */
		ip6_sprintf(laddr_str, &inc->inc6_laddr);
		ip6_sprintf(faddr_str, &inc->inc6_faddr);
	} else {
#endif
		/* IPv4. */
		inet_ntoa_r(inc->inc_laddr, laddr_str);
		inet_ntoa_r(inc->inc_faddr, faddr_str);
#ifdef INET6
	}
#endif
	db_print_indent(indent);
	db_printf("inc_laddr %s   inc_lport %u\n", laddr_str,
	    ntohs(inc->inc_lport));
	db_print_indent(indent);
	db_printf("inc_faddr %s   inc_fport %u\n", faddr_str,
	    ntohs(inc->inc_fport));
}

static void
db_print_inpflags(int inp_flags)
{
	int comma;

	comma = 0;
	if (inp_flags & INP_RECVOPTS) {
		db_printf("%sINP_RECVOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVRETOPTS) {
		db_printf("%sINP_RECVRETOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVDSTADDR) {
		db_printf("%sINP_RECVDSTADDR", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_HDRINCL) {
		db_printf("%sINP_HDRINCL", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_HIGHPORT) {
		db_printf("%sINP_HIGHPORT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_LOWPORT) {
		db_printf("%sINP_LOWPORT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_ANONPORT) {
		db_printf("%sINP_ANONPORT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVIF) {
		db_printf("%sINP_RECVIF", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_MTUDISC) {
		db_printf("%sINP_MTUDISC", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_FAITH) {
		db_printf("%sINP_FAITH", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_RECVTTL) {
		db_printf("%sINP_RECVTTL", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & INP_DONTFRAG) {
		db_printf("%sINP_DONTFRAG", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_IPV6_V6ONLY) {
		db_printf("%sIN6P_IPV6_V6ONLY", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_PKTINFO) {
		db_printf("%sIN6P_PKTINFO", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_HOPLIMIT) {
		db_printf("%sIN6P_HOPLIMIT", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_HOPOPTS) {
		db_printf("%sIN6P_HOPOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_DSTOPTS) {
		db_printf("%sIN6P_DSTOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_RTHDR) {
		db_printf("%sIN6P_RTHDR", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_RTHDRDSTOPTS) {
		db_printf("%sIN6P_RTHDRDSTOPTS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_TCLASS) {
		db_printf("%sIN6P_TCLASS", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_AUTOFLOWLABEL) {
		db_printf("%sIN6P_AUTOFLOWLABEL", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_RFC2292) {
		db_printf("%sIN6P_RFC2292", comma ? ", " : "");
		comma = 1;
	}
	if (inp_flags & IN6P_MTU) {
		db_printf("IN6P_MTU%s", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_inpvflag(u_char inp_vflag)
{
	int comma;

	comma = 0;
	if (inp_vflag & INP_IPV4) {
		db_printf("%sINP_IPV4", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_IPV6) {
		db_printf("%sINP_IPV6", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_IPV6PROTO) {
		db_printf("%sINP_IPV6PROTO", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_TIMEWAIT) {
		db_printf("%sINP_TIMEWAIT", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_ONESBCAST) {
		db_printf("%sINP_ONESBCAST", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_DROPPED) {
		db_printf("%sINP_DROPPED", comma ? ", " : "");
		comma  = 1;
	}
	if (inp_vflag & INP_SOCKREF) {
		db_printf("%sINP_SOCKREF", comma ? ", " : "");
		comma  = 1;
	}
}

void
db_print_inpcb(struct inpcb *inp, const char *name, int indent)
{

	db_print_indent(indent);
	db_printf("%s at %p\n", name, inp);

	indent += 2;

	db_print_indent(indent);
	db_printf("inp_flow: 0x%x\n", inp->inp_flow);

	db_print_inconninfo(&inp->inp_inc, "inp_conninfo", indent);

	db_print_indent(indent);
	db_printf("inp_ppcb: %p   inp_pcbinfo: %p   inp_socket: %p\n",
	    inp->inp_ppcb, inp->inp_pcbinfo, inp->inp_socket);

	db_print_indent(indent);
	db_printf("inp_label: %p   inp_flags: 0x%x (",
	   inp->inp_label, inp->inp_flags);
	db_print_inpflags(inp->inp_flags);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("inp_sp: %p   inp_vflag: 0x%x (", inp->inp_sp,
	    inp->inp_vflag);
	db_print_inpvflag(inp->inp_vflag);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("inp_ip_ttl: %d   inp_ip_p: %d   inp_ip_minttl: %d\n",
	    inp->inp_ip_ttl, inp->inp_ip_p, inp->inp_ip_minttl);

	db_print_indent(indent);
#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		db_printf("in6p_options: %p   in6p_outputopts: %p   "
		    "in6p_moptions: %p\n", inp->in6p_options,
		    inp->in6p_outputopts, inp->in6p_moptions);
		db_printf("in6p_icmp6filt: %p   in6p_cksum %d   "
		    "in6p_hops %u\n", inp->in6p_icmp6filt, inp->in6p_cksum,
		    inp->in6p_hops);
	} else
#endif
	{
		db_printf("inp_ip_tos: %d   inp_ip_options: %p   "
		    "inp_ip_moptions: %p\n", inp->inp_ip_tos,
		    inp->inp_options, inp->inp_moptions);
	}

	db_print_indent(indent);
	db_printf("inp_phd: %p   inp_gencnt: %ju\n", inp->inp_phd,
	    (uintmax_t)inp->inp_gencnt);
}

DB_SHOW_COMMAND(inpcb, db_show_inpcb)
{
	struct inpcb *inp;

	if (!have_addr) {
		db_printf("usage: show inpcb <addr>\n");
		return;
	}
	inp = (struct inpcb *)addr;

	db_print_inpcb(inp, "inpcb", 0);
}
#endif

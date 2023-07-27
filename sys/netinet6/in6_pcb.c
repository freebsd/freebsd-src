/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
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
 *	$KAME: in6_pcb.c,v 1.31 2001/05/21 05:45:10 jinmei Exp $
 */

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_route.h"
#include "opt_rss.h"

#include <sys/hash.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/smr.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/jail.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_llatbl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>

#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/in_pcb.h>
#include <netinet/in_pcb_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_fib.h>
#include <netinet6/scope6_var.h>

int
in6_pcbsetport(struct in6_addr *laddr, struct inpcb *inp, struct ucred *cred)
{
	struct socket *so = inp->inp_socket;
	u_int16_t lport = 0;
	int error, lookupflags = 0;
#ifdef INVARIANTS
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
#endif

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(pcbinfo);

	error = prison_local_ip6(cred, laddr,
	    ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0));
	if (error)
		return(error);

	/* XXX: this is redundant when called from in6_pcbbind */
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT|SO_REUSEPORT_LB)) == 0)
		lookupflags = INPLOOKUP_WILDCARD;

	inp->inp_flags |= INP_ANONPORT;

	error = in_pcb_lport(inp, NULL, &lport, cred, lookupflags);
	if (error != 0)
		return (error);

	inp->inp_lport = lport;
	if (in_pcbinshash(inp) != 0) {
		inp->in6p_laddr = in6addr_any;
		inp->inp_lport = 0;
		return (EAGAIN);
	}

	return (0);
}

int
in6_pcbbind(struct inpcb *inp, struct sockaddr_in6 *sin6, struct ucred *cred)
{
	struct socket *so = inp->inp_socket;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	u_short	lport = 0;
	int error, lookupflags = 0;
	int reuseport = (so->so_options & SO_REUSEPORT);

	/*
	 * XXX: Maybe we could let SO_REUSEPORT_LB set SO_REUSEPORT bit here
	 * so that we don't have to add to the (already messy) code below.
	 */
	int reuseport_lb = (so->so_options & SO_REUSEPORT_LB);

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(pcbinfo);

	if (inp->inp_lport || !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		return (EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT|SO_REUSEPORT_LB)) == 0)
		lookupflags = INPLOOKUP_WILDCARD;
	if (sin6 == NULL) {
		if ((error = prison_local_ip6(cred, &inp->in6p_laddr,
		    ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0))) != 0)
			return (error);
	} else {
		KASSERT(sin6->sin6_family == AF_INET6,
		    ("%s: invalid address family for %p", __func__, sin6));
		KASSERT(sin6->sin6_len == sizeof(*sin6),
		    ("%s: invalid address length for %p", __func__, sin6));

		if ((error = sa6_embedscope(sin6, V_ip6_use_defzone)) != 0)
			return(error);

		if ((error = prison_local_ip6(cred, &sin6->sin6_addr,
		    ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0))) != 0)
			return (error);

		lport = sin6->sin6_port;
		if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow compepte duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) != 0)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
			/*
			 * XXX: How to deal with SO_REUSEPORT_LB here?
			 * Treat same as SO_REUSEPORT for now.
			 */
			if ((so->so_options &
			    (SO_REUSEADDR|SO_REUSEPORT_LB)) != 0)
				reuseport_lb = SO_REUSEADDR|SO_REUSEPORT_LB;
		} else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			struct epoch_tracker et;
			struct ifaddr *ifa;

			sin6->sin6_port = 0;		/* yech... */
			NET_EPOCH_ENTER(et);
			if ((ifa = ifa_ifwithaddr((struct sockaddr *)sin6)) ==
			    NULL &&
			    (inp->inp_flags & INP_BINDANY) == 0) {
				NET_EPOCH_EXIT(et);
				return (EADDRNOTAVAIL);
			}

			/*
			 * XXX: bind to an anycast address might accidentally
			 * cause sending a packet with anycast source address.
			 * We should allow to bind to a deprecated address, since
			 * the application dares to use it.
			 */
			if (ifa != NULL &&
			    ((struct in6_ifaddr *)ifa)->ia6_flags &
			    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|IN6_IFF_DETACHED)) {
				NET_EPOCH_EXIT(et);
				return (EADDRNOTAVAIL);
			}
			NET_EPOCH_EXIT(et);
		}
		if (lport) {
			struct inpcb *t;

			/* GROSS */
			if (ntohs(lport) <= V_ipport_reservedhigh &&
			    ntohs(lport) >= V_ipport_reservedlow &&
			    priv_check_cred(cred, PRIV_NETINET_RESERVEDPORT))
				return (EACCES);
			if (!IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) &&
			    priv_check_cred(inp->inp_cred, PRIV_NETINET_REUSEPORT) != 0) {
				t = in6_pcblookup_local(pcbinfo,
				    &sin6->sin6_addr, lport,
				    INPLOOKUP_WILDCARD, cred);
				if (t != NULL &&
				    (so->so_type != SOCK_STREAM ||
				     IN6_IS_ADDR_UNSPECIFIED(&t->in6p_faddr)) &&
				    (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) ||
				     !IN6_IS_ADDR_UNSPECIFIED(&t->in6p_laddr) ||
				     (t->inp_socket->so_options & SO_REUSEPORT) ||
				     (t->inp_socket->so_options & SO_REUSEPORT_LB) == 0) &&
				    (inp->inp_cred->cr_uid !=
				     t->inp_cred->cr_uid))
					return (EADDRINUSE);

#ifdef INET
				if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
				    IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
					struct sockaddr_in sin;

					in6_sin6_2_sin(&sin, sin6);
					t = in_pcblookup_local(pcbinfo,
					    sin.sin_addr, lport,
					    INPLOOKUP_WILDCARD, cred);
					if (t != NULL &&
					    (so->so_type != SOCK_STREAM ||
					     ntohl(t->inp_faddr.s_addr) ==
					      INADDR_ANY) &&
					    (inp->inp_cred->cr_uid !=
					     t->inp_cred->cr_uid))
						return (EADDRINUSE);
				}
#endif
			}
			t = in6_pcblookup_local(pcbinfo, &sin6->sin6_addr,
			    lport, lookupflags, cred);
			if (t && (reuseport & t->inp_socket->so_options) == 0 &&
			    (reuseport_lb & t->inp_socket->so_options) == 0) {
				return (EADDRINUSE);
			}
#ifdef INET
			if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
			    IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
				struct sockaddr_in sin;

				in6_sin6_2_sin(&sin, sin6);
				t = in_pcblookup_local(pcbinfo, sin.sin_addr,
				   lport, lookupflags, cred);
				if (t &&
				    (reuseport & t->inp_socket->so_options) == 0 &&
				    (reuseport_lb & t->inp_socket->so_options) == 0 &&
				    (ntohl(t->inp_laddr.s_addr) != INADDR_ANY ||
				        (t->inp_vflag & INP_IPV6PROTO) != 0)) {
					return (EADDRINUSE);
				}
			}
#endif
		}
		inp->in6p_laddr = sin6->sin6_addr;
	}
	if (lport == 0) {
		if ((error = in6_pcbsetport(&inp->in6p_laddr, inp, cred)) != 0) {
			/* Undo an address bind that may have occurred. */
			inp->in6p_laddr = in6addr_any;
			return (error);
		}
	} else {
		inp->inp_lport = lport;
		if (in_pcbinshash(inp) != 0) {
			inp->in6p_laddr = in6addr_any;
			inp->inp_lport = 0;
			return (EAGAIN);
		}
	}
	return (0);
}

/*
 *   Transform old in6_pcbconnect() into an inner subroutine for new
 *   in6_pcbconnect(): Do some validity-checking on the remote
 *   address (in mbuf 'nam') and then determine local host address
 *   (i.e., which interface) to use to access that remote host.
 *
 *   This preserves definition of in6_pcbconnect(), while supporting a
 *   slightly different version for T/TCP.  (This is more than
 *   a bit of a kludge, but cleaning up the internal interfaces would
 *   have forced minor changes in every protocol).
 */
static int
in6_pcbladdr(struct inpcb *inp, struct sockaddr_in6 *sin6,
    struct in6_addr *plocal_addr6)
{
	int error = 0;
	int scope_ambiguous = 0;
	struct in6_addr in6a;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);	/* XXXRW: why? */

	if (sin6->sin6_port == 0)
		return (EADDRNOTAVAIL);

	if (sin6->sin6_scope_id == 0 && !V_ip6_use_defzone)
		scope_ambiguous = 1;
	if ((error = sa6_embedscope(sin6, V_ip6_use_defzone)) != 0)
		return(error);

	if (!CK_STAILQ_EMPTY(&V_in6_ifaddrhead)) {
		/*
		 * If the destination address is UNSPECIFIED addr,
		 * use the loopback addr, e.g ::1.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			sin6->sin6_addr = in6addr_loopback;
	}
	if ((error = prison_remote_ip6(inp->inp_cred, &sin6->sin6_addr)) != 0)
		return (error);

	error = in6_selectsrc_socket(sin6, inp->in6p_outputopts,
	    inp, inp->inp_cred, scope_ambiguous, &in6a, NULL);
	if (error)
		return (error);
	if (IN6_IS_ADDR_UNSPECIFIED(&in6a))
		return (EHOSTUNREACH);

	/*
	 * Do not update this earlier, in case we return with an error.
	 *
	 * XXX: this in6_selectsrc_socket result might replace the bound local
	 * address with the address specified by setsockopt(IPV6_PKTINFO).
	 * Is it the intended behavior?
	 */
	*plocal_addr6 = in6a;

	/*
	 * Don't do pcblookup call here; return interface in
	 * plocal_addr6
	 * and exit to caller, that will do the lookup.
	 */

	return (0);
}

/*
 * Outer subroutine:
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(struct inpcb *inp, struct sockaddr_in6 *sin6, struct ucred *cred,
    bool rehash __unused)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct sockaddr_in6 laddr6;
	int error;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(pcbinfo);
	KASSERT(sin6->sin6_family == AF_INET6,
	    ("%s: invalid address family for %p", __func__, sin6));
	KASSERT(sin6->sin6_len == sizeof(*sin6),
	    ("%s: invalid address length for %p", __func__, sin6));
	KASSERT(IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr),
	    ("%s: inp is already connected", __func__));

	bzero(&laddr6, sizeof(laddr6));
	laddr6.sin6_family = AF_INET6;

#ifdef ROUTE_MPATH
	if (CALC_FLOWID_OUTBOUND) {
		uint32_t hash_type, hash_val;

		hash_val = fib6_calc_software_hash(&inp->in6p_laddr,
		    &sin6->sin6_addr, 0, sin6->sin6_port,
		    inp->inp_socket->so_proto->pr_protocol, &hash_type);
		inp->inp_flowid = hash_val;
		inp->inp_flowtype = hash_type;
	}
#endif
	/*
	 * Call inner routine, to assign local interface address.
	 * in6_pcbladdr() may automatically fill in sin6_scope_id.
	 */
	if ((error = in6_pcbladdr(inp, sin6, &laddr6.sin6_addr)) != 0)
		return (error);

	if (in6_pcblookup_hash_locked(pcbinfo, &sin6->sin6_addr,
	    sin6->sin6_port, IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) ?
	    &laddr6.sin6_addr : &inp->in6p_laddr, inp->inp_lport, 0,
	    M_NODOM) != NULL)
		return (EADDRINUSE);
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		if (inp->inp_lport == 0) {
			error = in_pcb_lport_dest(inp,
			    (struct sockaddr *) &laddr6, &inp->inp_lport,
			    (struct sockaddr *) sin6, sin6->sin6_port, cred,
			    INPLOOKUP_WILDCARD);
			if (error)
				return (error);
		}
		inp->in6p_laddr = laddr6.sin6_addr;
	}
	inp->in6p_faddr = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	/* update flowinfo - draft-itojun-ipv6-flowlabel-api-00 */
	inp->inp_flow &= ~IPV6_FLOWLABEL_MASK;
	if (inp->inp_flags & IN6P_AUTOFLOWLABEL)
		inp->inp_flow |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);

	if ((inp->inp_flags & INP_INHASHLIST) != 0) {
		in_pcbrehash(inp);
	} else {
		in_pcbinshash(inp);
	}

	return (0);
}

void
in6_pcbdisconnect(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);
	KASSERT(inp->inp_smr == SMR_SEQ_INVALID,
	    ("%s: inp %p was already disconnected", __func__, inp));

	in_pcbremhash_locked(inp);

	/* See the comment in in_pcbinshash(). */
	inp->inp_smr = smr_advance(inp->inp_pcbinfo->ipi_smr);

	/* XXX-MJ torn writes are visible to SMR lookup */
	memset(&inp->in6p_laddr, 0, sizeof(inp->in6p_laddr));
	memset(&inp->in6p_faddr, 0, sizeof(inp->in6p_faddr));
	inp->inp_fport = 0;
	/* clear flowinfo - draft-itojun-ipv6-flowlabel-api-00 */
	inp->inp_flow &= ~IPV6_FLOWLABEL_MASK;
}

struct sockaddr *
in6_sockaddr(in_port_t port, struct in6_addr *addr_p)
{
	struct sockaddr_in6 *sin6;

	sin6 = malloc(sizeof *sin6, M_SONAME, M_WAITOK);
	bzero(sin6, sizeof *sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_port = port;
	sin6->sin6_addr = *addr_p;
	(void)sa6_recoverscope(sin6); /* XXX: should catch errors */

	return (struct sockaddr *)sin6;
}

struct sockaddr *
in6_v4mapsin6_sockaddr(in_port_t port, struct in_addr *addr_p)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 *sin6_p;

	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = port;
	sin.sin_addr = *addr_p;

	sin6_p = malloc(sizeof *sin6_p, M_SONAME,
		M_WAITOK);
	in6_sin_2_v4mapsin6(&sin, sin6_p);

	return (struct sockaddr *)sin6_p;
}

int
in6_getsockaddr(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *inp;
	struct in6_addr addr;
	in_port_t port;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_getsockaddr: inp == NULL"));

	INP_RLOCK(inp);
	port = inp->inp_lport;
	addr = inp->in6p_laddr;
	INP_RUNLOCK(inp);

	*nam = in6_sockaddr(port, &addr);
	return 0;
}

int
in6_getpeeraddr(struct socket *so, struct sockaddr **nam)
{
	struct inpcb *inp;
	struct in6_addr addr;
	in_port_t port;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_getpeeraddr: inp == NULL"));

	INP_RLOCK(inp);
	port = inp->inp_fport;
	addr = inp->in6p_faddr;
	INP_RUNLOCK(inp);

	*nam = in6_sockaddr(port, &addr);
	return 0;
}

int
in6_mapped_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct	inpcb *inp;
	int	error;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_mapped_sockaddr: inp == NULL"));

#ifdef INET
	if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) == INP_IPV4) {
		error = in_getsockaddr(so, nam);
		if (error == 0)
			in6_sin_2_v4mapsin6_in_sock(nam);
	} else
#endif
	{
		/* scope issues will be handled in in6_getsockaddr(). */
		error = in6_getsockaddr(so, nam);
	}

	return error;
}

int
in6_mapped_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct	inpcb *inp;
	int	error;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_mapped_peeraddr: inp == NULL"));

#ifdef INET
	if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) == INP_IPV4) {
		error = in_getpeeraddr(so, nam);
		if (error == 0)
			in6_sin_2_v4mapsin6_in_sock(nam);
	} else
#endif
	/* scope issues will be handled in in6_getpeeraddr(). */
	error = in6_getpeeraddr(so, nam);

	return error;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 */
static bool
inp_match6(const struct inpcb *inp, void *v __unused)
{

	return ((inp->inp_vflag & INP_IPV6) != 0);
}

void
in6_pcbnotify(struct inpcbinfo *pcbinfo, struct sockaddr_in6 *sa6_dst,
    u_int fport_arg, const struct sockaddr_in6 *src, u_int lport_arg,
    int errno, void *cmdarg,
    struct inpcb *(*notify)(struct inpcb *, int))
{
	struct inpcb_iterator inpi = INP_ITERATOR(pcbinfo, INPLOOKUP_WLOCKPCB,
	    inp_match6, NULL);
	struct inpcb *inp;
	struct sockaddr_in6 sa6_src;
	u_short	fport = fport_arg, lport = lport_arg;
	u_int32_t flowinfo;

	if (IN6_IS_ADDR_UNSPECIFIED(&sa6_dst->sin6_addr))
		return;

	/*
	 * note that src can be NULL when we get notify by local fragmentation.
	 */
	sa6_src = (src == NULL) ? sa6_any : *src;
	flowinfo = sa6_src.sin6_flowinfo;

	while ((inp = inp_next(&inpi)) != NULL) {
		INP_WLOCK_ASSERT(inp);
		/*
		 * If the error designates a new path MTU for a destination
		 * and the application (associated with this socket) wanted to
		 * know the value, notify.
		 * XXX: should we avoid to notify the value to TCP sockets?
		 */
		if (errno == EMSGSIZE && cmdarg != NULL)
			ip6_notify_pmtu(inp, sa6_dst, *(uint32_t *)cmdarg);

		/*
		 * Detect if we should notify the error. If no source and
		 * destination ports are specified, but non-zero flowinfo and
		 * local address match, notify the error. This is the case
		 * when the error is delivered with an encrypted buffer
		 * by ESP. Otherwise, just compare addresses and ports
		 * as usual.
		 */
		if (lport == 0 && fport == 0 && flowinfo &&
		    inp->inp_socket != NULL &&
		    flowinfo == (inp->inp_flow & IPV6_FLOWLABEL_MASK) &&
		    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, &sa6_src.sin6_addr))
			goto do_notify;
		else if (!IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr,
					     &sa6_dst->sin6_addr) ||
			 inp->inp_socket == 0 ||
			 (lport && inp->inp_lport != lport) ||
			 (!IN6_IS_ADDR_UNSPECIFIED(&sa6_src.sin6_addr) &&
			  !IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr,
					      &sa6_src.sin6_addr)) ||
			 (fport && inp->inp_fport != fport)) {
			continue;
		}

	  do_notify:
		if (notify)
			(*notify)(inp, errno);
	}
}

/*
 * Lookup a PCB based on the local address and port.  Caller must hold the
 * hash lock.  No inpcb locks or references are acquired.
 */
struct inpcb *
in6_pcblookup_local(struct inpcbinfo *pcbinfo, struct in6_addr *laddr,
    u_short lport, int lookupflags, struct ucred *cred)
{
	struct inpcb *inp;
	int matchwild = 3, wildcard;

	KASSERT((lookupflags & ~(INPLOOKUP_WILDCARD)) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));

	INP_HASH_LOCK_ASSERT(pcbinfo);

	if ((lookupflags & INPLOOKUP_WILDCARD) == 0) {
		struct inpcbhead *head;
		/*
		 * Look for an unconnected (wildcard foreign addr) PCB that
		 * matches the local address and port we're looking for.
		 */
		head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
		    pcbinfo->ipi_hashmask)];
		CK_LIST_FOREACH(inp, head, inp_hash_wild) {
			/* XXX inp locking */
			if ((inp->inp_vflag & INP_IPV6) == 0)
				continue;
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) &&
			    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr) &&
			    inp->inp_lport == lport) {
				/* Found. */
				if (prison_equal_ip6(cred->cr_prison,
				    inp->inp_cred->cr_prison))
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
		CK_LIST_FOREACH(phd, porthash, phd_hash) {
			if (phd->phd_port == lport)
				break;
		}
		if (phd != NULL) {
			/*
			 * Port is in use by one or more PCBs. Look for best
			 * fit.
			 */
			CK_LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
				wildcard = 0;
				if (!prison_equal_ip6(cred->cr_prison,
				    inp->inp_cred->cr_prison))
					continue;
				/* XXX inp locking */
				if ((inp->inp_vflag & INP_IPV6) == 0)
					continue;
				if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr))
					wildcard++;
				if (!IN6_IS_ADDR_UNSPECIFIED(
					&inp->in6p_laddr)) {
					if (IN6_IS_ADDR_UNSPECIFIED(laddr))
						wildcard++;
					else if (!IN6_ARE_ADDR_EQUAL(
					    &inp->in6p_laddr, laddr))
						continue;
				} else {
					if (!IN6_IS_ADDR_UNSPECIFIED(laddr))
						wildcard++;
				}
				if (wildcard < matchwild) {
					match = inp;
					matchwild = wildcard;
					if (matchwild == 0)
						break;
				}
			}
		}
		return (match);
	}
}

static bool
in6_multi_match(const struct inpcb *inp, void *v __unused)
{

	if ((inp->inp_vflag & INP_IPV6) && inp->in6p_moptions != NULL)
		return (true);
	else
		return (false);
}

void
in6_pcbpurgeif0(struct inpcbinfo *pcbinfo, struct ifnet *ifp)
{
	struct inpcb_iterator inpi = INP_ITERATOR(pcbinfo, INPLOOKUP_RLOCKPCB,
	    in6_multi_match, NULL);
	struct inpcb *inp;
	struct in6_multi *inm;
	struct in6_mfilter *imf;
	struct ip6_moptions *im6o;

	IN6_MULTI_LOCK_ASSERT();

	while ((inp = inp_next(&inpi)) != NULL) {
		INP_RLOCK_ASSERT(inp);

		im6o = inp->in6p_moptions;
		/*
		 * Unselect the outgoing ifp for multicast if it
		 * is being detached.
		 */
		if (im6o->im6o_multicast_ifp == ifp)
			im6o->im6o_multicast_ifp = NULL;
		/*
		 * Drop multicast group membership if we joined
		 * through the interface being detached.
		 */
restart:
		IP6_MFILTER_FOREACH(imf, &im6o->im6o_head) {
			if ((inm = imf->im6f_in6m) == NULL)
				continue;
			if (inm->in6m_ifp != ifp)
				continue;
			ip6_mfilter_remove(&im6o->im6o_head, imf);
			in6_leavegroup_locked(inm, NULL);
			ip6_mfilter_free(imf);
			goto restart;
		}
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in6_losing(struct inpcb *inp)
{

	RO_INVALIDATE_CACHE(&inp->inp_route6);
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
struct inpcb *
in6_rtchange(struct inpcb *inp, int errno __unused)
{

	RO_INVALIDATE_CACHE(&inp->inp_route6);
	return inp;
}

static bool
in6_pcblookup_lb_numa_match(const struct inpcblbgroup *grp, int domain)
{
	return (domain == M_NODOM || domain == grp->il_numa_domain);
}

static struct inpcb *
in6_pcblookup_lbgroup(const struct inpcbinfo *pcbinfo,
    const struct in6_addr *faddr, uint16_t fport, const struct in6_addr *laddr,
    uint16_t lport, uint8_t domain)
{
	const struct inpcblbgrouphead *hdr;
	struct inpcblbgroup *grp;
	struct inpcblbgroup *jail_exact, *jail_wild, *local_exact, *local_wild;

	INP_HASH_LOCK_ASSERT(pcbinfo);

	hdr = &pcbinfo->ipi_lbgrouphashbase[
	    INP_PCBPORTHASH(lport, pcbinfo->ipi_lbgrouphashmask)];

	/*
	 * Search for an LB group match based on the following criteria:
	 * - prefer jailed groups to non-jailed groups
	 * - prefer exact source address matches to wildcard matches
	 * - prefer groups bound to the specified NUMA domain 
	 */
	jail_exact = jail_wild = local_exact = local_wild = NULL;
	CK_LIST_FOREACH(grp, hdr, il_list) {
		bool injail;

#ifdef INET
		if (!(grp->il_vflag & INP_IPV6))
			continue;
#endif
		if (grp->il_lport != lport)
			continue;

		injail = prison_flag(grp->il_cred, PR_IP6) != 0;
		if (injail && prison_check_ip6_locked(grp->il_cred->cr_prison,
		    laddr) != 0)
			continue;

		if (IN6_ARE_ADDR_EQUAL(&grp->il6_laddr, laddr)) {
			if (injail) {
				jail_exact = grp;
				if (in6_pcblookup_lb_numa_match(grp, domain))
					/* This is a perfect match. */
					goto out;
			} else if (local_exact == NULL ||
			    in6_pcblookup_lb_numa_match(grp, domain)) {
				local_exact = grp;
			}
		} else if (IN6_IS_ADDR_UNSPECIFIED(&grp->il6_laddr)) {
			if (injail) {
				if (jail_wild == NULL ||
				    in6_pcblookup_lb_numa_match(grp, domain))
					jail_wild = grp;
			} else if (local_wild == NULL ||
			    in6_pcblookup_lb_numa_match(grp, domain)) {
				local_wild = grp;
			}
		}
	}

	if (jail_exact != NULL)
		grp = jail_exact;
	else if (jail_wild != NULL)
		grp = jail_wild;
	else if (local_exact != NULL)
		grp = local_exact;
	else
		grp = local_wild;
	if (grp == NULL)
		return (NULL);
out:
	return (grp->il_inp[INP6_PCBLBGROUP_PKTHASH(faddr, lport, fport) %
	    grp->il_inpcnt]);
}

static bool
in6_pcblookup_exact_match(const struct inpcb *inp, const struct in6_addr *faddr,
    u_short fport, const struct in6_addr *laddr, u_short lport)
{
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV6) == 0)
		return (false);
	if (IN6_ARE_ADDR_EQUAL(&inp->in6p_faddr, faddr) &&
	    IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr) &&
	    inp->inp_fport == fport && inp->inp_lport == lport)
		return (true);
	return (false);
}

static struct inpcb *
in6_pcblookup_hash_exact(struct inpcbinfo *pcbinfo,
    const struct in6_addr *faddr, u_short fport,
    const struct in6_addr *laddr, u_short lport)
{
	struct inpcbhead *head;
	struct inpcb *inp;

	INP_HASH_LOCK_ASSERT(pcbinfo);

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->ipi_hash_exact[INP6_PCBHASH(faddr, lport, fport,
	    pcbinfo->ipi_hashmask)];
	CK_LIST_FOREACH(inp, head, inp_hash_exact) {
		if (in6_pcblookup_exact_match(inp, faddr, fport, laddr, lport))
			return (inp);
	}
	return (NULL);
}

typedef enum {
	INPLOOKUP_MATCH_NONE = 0,
	INPLOOKUP_MATCH_WILD = 1,
	INPLOOKUP_MATCH_LADDR = 2,
} inp_lookup_match_t;

static inp_lookup_match_t
in6_pcblookup_wild_match(const struct inpcb *inp, const struct in6_addr *laddr,
    u_short lport)
{
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV6) == 0)
		return (INPLOOKUP_MATCH_NONE);
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) ||
	    inp->inp_lport != lport)
		return (INPLOOKUP_MATCH_NONE);
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		return (INPLOOKUP_MATCH_WILD);
	if (IN6_ARE_ADDR_EQUAL(&inp->in6p_laddr, laddr))
		return (INPLOOKUP_MATCH_LADDR);
	return (INPLOOKUP_MATCH_NONE);
}

#define	INP_LOOKUP_AGAIN	((struct inpcb *)(uintptr_t)-1)

static struct inpcb *
in6_pcblookup_hash_wild_smr(struct inpcbinfo *pcbinfo,
    const struct in6_addr *faddr, u_short fport, const struct in6_addr *laddr,
    u_short lport, const inp_lookup_t lockflags)
{
	struct inpcbhead *head;
	struct inpcb *inp;

	KASSERT(SMR_ENTERED(pcbinfo->ipi_smr),
	    ("%s: not in SMR read section", __func__));

	head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
	    pcbinfo->ipi_hashmask)];
	CK_LIST_FOREACH(inp, head, inp_hash_wild) {
		inp_lookup_match_t match;

		match = in6_pcblookup_wild_match(inp, laddr, lport);
		if (match == INPLOOKUP_MATCH_NONE)
			continue;

		if (__predict_true(inp_smr_lock(inp, lockflags))) {
			match = in6_pcblookup_wild_match(inp, laddr, lport);
			if (match != INPLOOKUP_MATCH_NONE &&
			    prison_check_ip6_locked(inp->inp_cred->cr_prison,
			    laddr) == 0)
				return (inp);
			inp_unlock(inp, lockflags);
		}

		/*
		 * The matching socket disappeared out from under us.  Fall back
		 * to a serialized lookup.
		 */
		return (INP_LOOKUP_AGAIN);
	}
	return (NULL);
}

static struct inpcb *
in6_pcblookup_hash_wild_locked(struct inpcbinfo *pcbinfo,
    const struct in6_addr *faddr, u_short fport, const struct in6_addr *laddr,
    u_short lport)
{
	struct inpcbhead *head;
	struct inpcb *inp, *jail_wild, *local_exact, *local_wild;

	INP_HASH_LOCK_ASSERT(pcbinfo);

	/*
	 * Order of socket selection - we always prefer jails.
	 *      1. jailed, non-wild.
	 *      2. jailed, wild.
	 *      3. non-jailed, non-wild.
	 *      4. non-jailed, wild.
	 */
	head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
	    pcbinfo->ipi_hashmask)];
	local_wild = local_exact = jail_wild = NULL;
	CK_LIST_FOREACH(inp, head, inp_hash_wild) {
		inp_lookup_match_t match;
		bool injail;

		match = in6_pcblookup_wild_match(inp, laddr, lport);
		if (match == INPLOOKUP_MATCH_NONE)
			continue;

		injail = prison_flag(inp->inp_cred, PR_IP6) != 0;
		if (injail) {
			if (prison_check_ip6_locked(
			    inp->inp_cred->cr_prison, laddr) != 0)
				continue;
		} else {
			if (local_exact != NULL)
				continue;
		}

		if (match == INPLOOKUP_MATCH_LADDR) {
			if (injail)
				return (inp);
			else
				local_exact = inp;
		} else {
			if (injail)
				jail_wild = inp;
			else
				local_wild = inp;
		}
	}

	if (jail_wild != NULL)
		return (jail_wild);
	if (local_exact != NULL)
		return (local_exact);
	if (local_wild != NULL)
		return (local_wild);
	return (NULL);
}

struct inpcb *
in6_pcblookup_hash_locked(struct inpcbinfo *pcbinfo,
    const struct in6_addr *faddr, u_int fport_arg,
    const struct in6_addr *laddr, u_int lport_arg,
    int lookupflags, uint8_t numa_domain)
{
	struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;

	KASSERT((lookupflags & ~INPLOOKUP_WILDCARD) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));
	KASSERT(!IN6_IS_ADDR_UNSPECIFIED(faddr),
	    ("%s: invalid foreign address", __func__));
	KASSERT(!IN6_IS_ADDR_UNSPECIFIED(laddr),
	    ("%s: invalid local address", __func__));
	INP_HASH_LOCK_ASSERT(pcbinfo);

	inp = in6_pcblookup_hash_exact(pcbinfo, faddr, fport, laddr, lport);
	if (inp != NULL)
		return (inp);

	if ((lookupflags & INPLOOKUP_WILDCARD) != 0) {
		inp = in6_pcblookup_lbgroup(pcbinfo, faddr, fport, laddr,
		    lport, numa_domain);
		if (inp == NULL) {
			inp = in6_pcblookup_hash_wild_locked(pcbinfo, faddr,
			    fport, laddr, lport);
		}
	}
	return (inp);
}

static struct inpcb *
in6_pcblookup_hash(struct inpcbinfo *pcbinfo, const struct in6_addr *faddr,
    u_int fport, const struct in6_addr *laddr, u_int lport, int lookupflags,
    uint8_t numa_domain)
{
	struct inpcb *inp;
	const inp_lookup_t lockflags = lookupflags & INPLOOKUP_LOCKMASK;

	KASSERT((lookupflags & (INPLOOKUP_RLOCKPCB | INPLOOKUP_WLOCKPCB)) != 0,
	    ("%s: LOCKPCB not set", __func__));

	INP_HASH_WLOCK(pcbinfo);
	inp = in6_pcblookup_hash_locked(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags & ~INPLOOKUP_LOCKMASK, numa_domain);
	if (inp != NULL && !inp_trylock(inp, lockflags)) {
		in_pcbref(inp);
		INP_HASH_WUNLOCK(pcbinfo);
		inp_lock(inp, lockflags);
		if (in_pcbrele(inp, lockflags))
			/* XXX-MJ or retry until we get a negative match? */
			inp = NULL;
	} else {
		INP_HASH_WUNLOCK(pcbinfo);
	}
	return (inp);
}

static struct inpcb *
in6_pcblookup_hash_smr(struct inpcbinfo *pcbinfo, struct in6_addr *faddr,
    u_int fport_arg, struct in6_addr *laddr, u_int lport_arg, int lookupflags,
    uint8_t numa_domain)
{
	struct inpcb *inp;
	const inp_lookup_t lockflags = lookupflags & INPLOOKUP_LOCKMASK;
	const u_short fport = fport_arg, lport = lport_arg;

	KASSERT((lookupflags & ~INPLOOKUP_MASK) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));
	KASSERT((lookupflags & (INPLOOKUP_RLOCKPCB | INPLOOKUP_WLOCKPCB)) != 0,
	    ("%s: LOCKPCB not set", __func__));

	smr_enter(pcbinfo->ipi_smr);
	inp = in6_pcblookup_hash_exact(pcbinfo, faddr, fport, laddr, lport);
	if (inp != NULL) {
		if (__predict_true(inp_smr_lock(inp, lockflags))) {
			if (__predict_true(in6_pcblookup_exact_match(inp,
			    faddr, fport, laddr, lport)))
				return (inp);
			inp_unlock(inp, lockflags);
		}
		/*
		 * We failed to lock the inpcb, or its connection state changed
		 * out from under us.  Fall back to a precise search.
		 */
		return (in6_pcblookup_hash(pcbinfo, faddr, fport, laddr, lport,
		    lookupflags, numa_domain));
	}

	if ((lookupflags & INPLOOKUP_WILDCARD) != 0) {
		inp = in6_pcblookup_lbgroup(pcbinfo, faddr, fport,
		    laddr, lport, numa_domain);
		if (inp != NULL) {
			if (__predict_true(inp_smr_lock(inp, lockflags))) {
				if (__predict_true(in6_pcblookup_wild_match(inp,
				    laddr, lport) != INPLOOKUP_MATCH_NONE))
					return (inp);
				inp_unlock(inp, lockflags);
			}
			inp = INP_LOOKUP_AGAIN;
		} else {
			inp = in6_pcblookup_hash_wild_smr(pcbinfo, faddr, fport,
			    laddr, lport, lockflags);
		}
		if (inp == INP_LOOKUP_AGAIN) {
			return (in6_pcblookup_hash(pcbinfo, faddr, fport, laddr,
			    lport, lookupflags, numa_domain));
		}
	}

	if (inp == NULL)
		smr_exit(pcbinfo->ipi_smr);

	return (inp);
}

/*
 * Public inpcb lookup routines, accepting a 4-tuple, and optionally, an mbuf
 * from which a pre-calculated hash value may be extracted.
 */
struct inpcb *
in6_pcblookup(struct inpcbinfo *pcbinfo, struct in6_addr *faddr, u_int fport,
    struct in6_addr *laddr, u_int lport, int lookupflags,
    struct ifnet *ifp __unused)
{
	return (in6_pcblookup_hash_smr(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags, M_NODOM));
}

struct inpcb *
in6_pcblookup_mbuf(struct inpcbinfo *pcbinfo, struct in6_addr *faddr,
    u_int fport, struct in6_addr *laddr, u_int lport, int lookupflags,
    struct ifnet *ifp __unused, struct mbuf *m)
{
	return (in6_pcblookup_hash_smr(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags, m->m_pkthdr.numa_domain));
}

void
init_sin6(struct sockaddr_in6 *sin6, struct mbuf *m, int srcordst)
{
	struct ip6_hdr *ip;

	ip = mtod(m, struct ip6_hdr *);
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = srcordst ? ip->ip6_dst : ip->ip6_src;

	(void)sa6_recoverscope(sin6); /* XXX: should catch errors... */

	return;
}

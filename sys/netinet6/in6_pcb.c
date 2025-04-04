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
 */

#include <sys/cdefs.h>
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
#include <sys/sysctl.h>
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
#include <net/vnet.h>

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

SYSCTL_DECL(_net_inet6);
SYSCTL_DECL(_net_inet6_ip6);
VNET_DEFINE_STATIC(int, connect_in6addr_wild) = 1;
#define	V_connect_in6addr_wild	VNET(connect_in6addr_wild)
SYSCTL_INT(_net_inet6_ip6, OID_AUTO, connect_in6addr_wild,
    CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(connect_in6addr_wild), 0,
    "Allow connecting to the unspecified address for connect(2)");

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

/*
 * Determine whether the inpcb can be bound to the specified address/port tuple.
 */
static int
in6_pcbbind_avail(struct inpcb *inp, const struct sockaddr_in6 *sin6, int fib,
    int sooptions, int lookupflags, struct ucred *cred)
{
	const struct in6_addr *laddr;
	int reuseport, reuseport_lb;
	u_short lport;

	INP_LOCK_ASSERT(inp);
	INP_HASH_LOCK_ASSERT(inp->inp_pcbinfo);

	laddr = &sin6->sin6_addr;
	lport = sin6->sin6_port;

	reuseport = (sooptions & SO_REUSEPORT);
	reuseport_lb = (sooptions & SO_REUSEPORT_LB);

	if (IN6_IS_ADDR_MULTICAST(laddr)) {
		/*
		 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
		 * allow compepte duplication of binding if
		 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
		 * and a multicast address is bound on both
		 * new and duplicated sockets.
		 */
		if ((sooptions & (SO_REUSEADDR | SO_REUSEPORT)) != 0)
			reuseport = SO_REUSEADDR | SO_REUSEPORT;
		/*
		 * XXX: How to deal with SO_REUSEPORT_LB here?
		 * Treat same as SO_REUSEPORT for now.
		 */
		if ((sooptions & (SO_REUSEADDR | SO_REUSEPORT_LB)) != 0)
			reuseport_lb = SO_REUSEADDR | SO_REUSEPORT_LB;
	} else if (!IN6_IS_ADDR_UNSPECIFIED(laddr)) {
		struct sockaddr_in6 sin6;
		struct epoch_tracker et;
		struct ifaddr *ifa;

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = *laddr;

		NET_EPOCH_ENTER(et);
		if ((ifa = ifa_ifwithaddr((const struct sockaddr *)&sin6)) ==
		    NULL && (inp->inp_flags & INP_BINDANY) == 0) {
			NET_EPOCH_EXIT(et);
			return (EADDRNOTAVAIL);
		}

		/*
		 * We used to prohibit binding to an anycast address here,
		 * based on RFC3513, but that restriction was removed in
		 * RFC4291.
		 */
		if (ifa != NULL &&
		    ((struct in6_ifaddr *)ifa)->ia6_flags &
		    (IN6_IFF_NOTREADY | IN6_IFF_DETACHED)) {
			NET_EPOCH_EXIT(et);
			return (EADDRNOTAVAIL);
		}
		NET_EPOCH_EXIT(et);
	}

	if (lport != 0) {
		struct inpcb *t;

		if (ntohs(lport) <= V_ipport_reservedhigh &&
		    ntohs(lport) >= V_ipport_reservedlow &&
		    priv_check_cred(cred, PRIV_NETINET_RESERVEDPORT))
			return (EACCES);

		if (!IN6_IS_ADDR_MULTICAST(laddr) &&
		    priv_check_cred(inp->inp_cred, PRIV_NETINET_REUSEPORT) !=
		    0) {
			/*
			 * If a socket owned by a different user is already
			 * bound to this port, fail.  In particular, SO_REUSE*
			 * can only be used to share a port among sockets owned
			 * by the same user.
			 *
			 * However, we can share a port with a connected socket
			 * which has a unique 4-tuple.
			 */
			t = in6_pcblookup_local(inp->inp_pcbinfo, laddr, lport,
			    RT_ALL_FIBS, INPLOOKUP_WILDCARD, cred);
			if (t != NULL &&
			    (inp->inp_socket->so_type != SOCK_STREAM ||
			     IN6_IS_ADDR_UNSPECIFIED(&t->in6p_faddr)) &&
			    (inp->inp_cred->cr_uid != t->inp_cred->cr_uid))
				return (EADDRINUSE);

#ifdef INET
			if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
			    IN6_IS_ADDR_UNSPECIFIED(laddr)) {
				struct sockaddr_in sin;

				in6_sin6_2_sin(&sin, sin6);
				t = in_pcblookup_local(inp->inp_pcbinfo,
				    sin.sin_addr, lport, RT_ALL_FIBS,
				    INPLOOKUP_WILDCARD, cred);
				if (t != NULL &&
				    (inp->inp_socket->so_type != SOCK_STREAM ||
				     in_nullhost(t->inp_faddr)) &&
				    (inp->inp_cred->cr_uid !=
				     t->inp_cred->cr_uid))
					return (EADDRINUSE);
			}
#endif
		}
		t = in6_pcblookup_local(inp->inp_pcbinfo, laddr, lport,
		    fib, lookupflags, cred);
		if (t != NULL && ((reuseport | reuseport_lb) &
		    t->inp_socket->so_options) == 0)
			return (EADDRINUSE);
#ifdef INET
		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
		    IN6_IS_ADDR_UNSPECIFIED(laddr)) {
			struct sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6);
			t = in_pcblookup_local(inp->inp_pcbinfo, sin.sin_addr,
			   lport, RT_ALL_FIBS, lookupflags, cred);
			if (t != NULL && ((reuseport | reuseport_lb) &
			    t->inp_socket->so_options) == 0 &&
			    (!in_nullhost(t->inp_laddr) ||
			     (t->inp_vflag & INP_IPV6PROTO) != 0)) {
				return (EADDRINUSE);
			}
		}
#endif
	}
	return (0);
}

int
in6_pcbbind(struct inpcb *inp, struct sockaddr_in6 *sin6, int flags,
    struct ucred *cred)
{
	struct socket *so = inp->inp_socket;
	u_short	lport = 0;
	int error, fib, lookupflags, sooptions;

	INP_WLOCK_ASSERT(inp);
	INP_HASH_WLOCK_ASSERT(inp->inp_pcbinfo);

	if (inp->inp_lport || !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		return (EINVAL);

	lookupflags = 0;
	sooptions = atomic_load_int(&so->so_options);
	if ((sooptions & (SO_REUSEADDR | SO_REUSEPORT | SO_REUSEPORT_LB)) == 0)
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

		fib = (flags & INPBIND_FIB) != 0 ? inp->inp_inc.inc_fibnum :
		    RT_ALL_FIBS;

		/* See if this address/port combo is available. */
		error = in6_pcbbind_avail(inp, sin6, fib, sooptions, lookupflags,
		    cred);
		if (error != 0)
			return (error);

		lport = sin6->sin6_port;
		inp->in6p_laddr = sin6->sin6_addr;
	}
	if ((flags & INPBIND_FIB) != 0)
		inp->inp_flags |= INP_BOUNDFIB;
	if (lport == 0) {
		if ((error = in6_pcbsetport(&inp->in6p_laddr, inp, cred)) != 0) {
			/* Undo an address bind that may have occurred. */
			inp->inp_flags &= ~INP_BOUNDFIB;
			inp->in6p_laddr = in6addr_any;
			return (error);
		}
	} else {
		inp->inp_lport = lport;
		if (in_pcbinshash(inp) != 0) {
			inp->inp_flags &= ~INP_BOUNDFIB;
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
    struct in6_addr *plocal_addr6, bool sas_required)
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

	if (V_connect_in6addr_wild && !CK_STAILQ_EMPTY(&V_in6_ifaddrhead)) {
		/*
		 * If the destination address is UNSPECIFIED addr,
		 * use the loopback addr, e.g ::1.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			sin6->sin6_addr = in6addr_loopback;
	} else if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		return (ENETUNREACH);
	}

	if ((error = prison_remote_ip6(inp->inp_cred, &sin6->sin6_addr)) != 0)
		return (error);

	if (sas_required) {
		error = in6_selectsrc_socket(sin6, inp->in6p_outputopts,
		    inp, inp->inp_cred, scope_ambiguous, &in6a, NULL);
		if (error)
			return (error);
	} else {
		/*
		 * Source address selection isn't required when syncache
		 * has already established connection and both source and
		 * destination addresses was chosen.
		 *
		 * This also includes the case when fwd_tag was used to
		 * select source address in tcp_input().
		 */
		in6a = inp->in6p_laddr;
	}

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
    bool sas_required)
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
	if ((error = in6_pcbladdr(inp, sin6, &laddr6.sin6_addr,
	    sas_required)) != 0)
		return (error);

	if (in6_pcblookup_hash_locked(pcbinfo, &sin6->sin6_addr,
	    sin6->sin6_port, IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) ?
	    &laddr6.sin6_addr : &inp->in6p_laddr, inp->inp_lport, 0,
	    M_NODOM, RT_ALL_FIBS) != NULL)
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
		error = in_pcbinshash(inp);
		MPASS(error == 0);
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

int
in6_getsockaddr(struct socket *so, struct sockaddr *sa)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_getsockaddr: inp == NULL"));

	*(struct sockaddr_in6 *)sa = (struct sockaddr_in6 ){
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_family = AF_INET6,
		.sin6_port = inp->inp_lport,
		.sin6_addr = inp->in6p_laddr,
	};
	/* XXX: should catch errors */
	(void)sa6_recoverscope((struct sockaddr_in6 *)sa);

	return (0);
}

int
in6_getpeeraddr(struct socket *so, struct sockaddr *sa)
{
	struct inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_getpeeraddr: inp == NULL"));

	*(struct sockaddr_in6 *)sa = (struct sockaddr_in6 ){
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_family = AF_INET6,
		.sin6_port = inp->inp_fport,
		.sin6_addr = inp->in6p_faddr,
	};
	/* XXX: should catch errors */
	(void)sa6_recoverscope((struct sockaddr_in6 *)sa);

	return (0);
}

int
in6_mapped_sockaddr(struct socket *so, struct sockaddr *sa)
{
	int	error;
#ifdef INET
	struct	inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_mapped_sockaddr: inp == NULL"));

	if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) == INP_IPV4) {
		struct sockaddr_in sin;

		error = in_getsockaddr(so, (struct sockaddr *)&sin);
		if (error == 0)
			in6_sin_2_v4mapsin6(&sin, (struct sockaddr_in6 *)sa);
	} else
#endif
	{
		/* scope issues will be handled in in6_getsockaddr(). */
		error = in6_getsockaddr(so, sa);
	}

	return error;
}

int
in6_mapped_peeraddr(struct socket *so, struct sockaddr *sa)
{
	int	error;
#ifdef INET
	struct	inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("in6_mapped_peeraddr: inp == NULL"));

	if ((inp->inp_vflag & (INP_IPV4 | INP_IPV6)) == INP_IPV4) {
		struct sockaddr_in sin;

		error = in_getpeeraddr(so, (struct sockaddr *)&sin);
		if (error == 0)
			in6_sin_2_v4mapsin6(&sin, (struct sockaddr_in6 *)sa);
	} else
#endif
	{
		/* scope issues will be handled in in6_getpeeraddr(). */
		error = in6_getpeeraddr(so, sa);
	}

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
in6_pcblookup_local(struct inpcbinfo *pcbinfo, const struct in6_addr *laddr,
    u_short lport, int fib, int lookupflags, struct ucred *cred)
{
	struct inpcb *inp;
	int matchwild = 3, wildcard;

	KASSERT((lookupflags & ~(INPLOOKUP_WILDCARD)) == 0,
	    ("%s: invalid lookup flags %d", __func__, lookupflags));
	KASSERT(fib == RT_ALL_FIBS || (fib >= 0 && fib < V_rt_numfibs),
	    ("%s: invalid fib %d", __func__, fib));

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
			    inp->inp_lport == lport && (fib == RT_ALL_FIBS ||
			    inp->inp_inc.inc_fibnum == fib)) {
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
		struct inpcbhead *porthash;
		struct inpcb *match = NULL;

		/*
		 * Port is in use by one or more PCBs. Look for best
		 * fit.
		 */
		porthash = &pcbinfo->ipi_porthashbase[INP_PCBPORTHASH(lport,
		    pcbinfo->ipi_porthashmask)];
		CK_LIST_FOREACH(inp, porthash, inp_portlist) {
			if (inp->inp_lport != lport)
				continue;
			if (!prison_equal_ip6(cred->cr_prison,
			    inp->inp_cred->cr_prison))
				continue;
			/* XXX inp locking */
			if ((inp->inp_vflag & INP_IPV6) == 0)
				continue;
			if (fib != RT_ALL_FIBS &&
			    inp->inp_inc.inc_fibnum != fib)
				continue;
			wildcard = 0;
			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr))
				wildcard++;
			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
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
in6_pcblookup_lb_match(const struct inpcblbgroup *grp, int domain, int fib)
{
	return ((domain == M_NODOM || domain == grp->il_numa_domain) &&
	    (fib == RT_ALL_FIBS || fib == grp->il_fibnum));
}

static struct inpcb *
in6_pcblookup_lbgroup(const struct inpcbinfo *pcbinfo,
    const struct in6_addr *faddr, uint16_t fport, const struct in6_addr *laddr,
    uint16_t lport, uint8_t domain, int fib)
{
	const struct inpcblbgrouphead *hdr;
	struct inpcblbgroup *grp;
	struct inpcblbgroup *jail_exact, *jail_wild, *local_exact, *local_wild;
	struct inpcb *inp;
	u_int count;

	INP_HASH_LOCK_ASSERT(pcbinfo);
	NET_EPOCH_ASSERT();

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
				if (in6_pcblookup_lb_match(grp, domain, fib))
					/* This is a perfect match. */
					goto out;
			} else if (local_exact == NULL ||
			    in6_pcblookup_lb_match(grp, domain, fib)) {
				local_exact = grp;
			}
		} else if (IN6_IS_ADDR_UNSPECIFIED(&grp->il6_laddr)) {
			if (injail) {
				if (jail_wild == NULL ||
				    in6_pcblookup_lb_match(grp, domain, fib))
					jail_wild = grp;
			} else if (local_wild == NULL ||
			    in6_pcblookup_lb_match(grp, domain, fib)) {
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
	/*
	 * Synchronize with in_pcblbgroup_insert().
	 */
	count = atomic_load_acq_int(&grp->il_inpcnt);
	if (count == 0)
		return (NULL);
	inp = grp->il_inp[INP6_PCBLBGROUP_PKTHASH(faddr, lport, fport) % count];
	KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
	return (inp);
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
    u_short lport, int fib)
{
	/* XXX inp locking */
	if ((inp->inp_vflag & INP_IPV6) == 0)
		return (INPLOOKUP_MATCH_NONE);
	if (!IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr) ||
	    inp->inp_lport != lport)
		return (INPLOOKUP_MATCH_NONE);
	if (fib != RT_ALL_FIBS && inp->inp_inc.inc_fibnum != fib)
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
    const struct in6_addr *laddr, u_short lport, int fib,
    const inp_lookup_t lockflags)
{
	struct inpcbhead *head;
	struct inpcb *inp;

	KASSERT(SMR_ENTERED(pcbinfo->ipi_smr),
	    ("%s: not in SMR read section", __func__));

	head = &pcbinfo->ipi_hash_wild[INP_PCBHASH_WILD(lport,
	    pcbinfo->ipi_hashmask)];
	CK_LIST_FOREACH(inp, head, inp_hash_wild) {
		inp_lookup_match_t match;

		match = in6_pcblookup_wild_match(inp, laddr, lport, fib);
		if (match == INPLOOKUP_MATCH_NONE)
			continue;

		if (__predict_true(inp_smr_lock(inp, lockflags))) {
			match = in6_pcblookup_wild_match(inp, laddr, lport,
			    fib);
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
    const struct in6_addr *laddr, u_short lport, int fib)
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

		match = in6_pcblookup_wild_match(inp, laddr, lport, fib);
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
    int lookupflags, uint8_t numa_domain, int fib)
{
	struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;

	KASSERT((lookupflags & ~(INPLOOKUP_WILDCARD | INPLOOKUP_FIB)) == 0,
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
		    lport, numa_domain, fib);
		if (inp == NULL) {
			inp = in6_pcblookup_hash_wild_locked(pcbinfo,
			    laddr, lport, fib);
		}
	}
	return (inp);
}

static struct inpcb *
in6_pcblookup_hash(struct inpcbinfo *pcbinfo, const struct in6_addr *faddr,
    u_int fport, const struct in6_addr *laddr, u_int lport, int lookupflags,
    uint8_t numa_domain, int fib)
{
	struct inpcb *inp;
	const inp_lookup_t lockflags = lookupflags & INPLOOKUP_LOCKMASK;

	KASSERT((lookupflags & (INPLOOKUP_RLOCKPCB | INPLOOKUP_WLOCKPCB)) != 0,
	    ("%s: LOCKPCB not set", __func__));

	INP_HASH_WLOCK(pcbinfo);
	inp = in6_pcblookup_hash_locked(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags & ~INPLOOKUP_LOCKMASK, numa_domain, fib);
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
in6_pcblookup_hash_smr(struct inpcbinfo *pcbinfo, const struct in6_addr *faddr,
    u_int fport_arg, const struct in6_addr *laddr, u_int lport_arg,
    int lookupflags, uint8_t numa_domain, int fib)
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
		    lookupflags, numa_domain, fib));
	}

	if ((lookupflags & INPLOOKUP_WILDCARD) != 0) {
		inp = in6_pcblookup_lbgroup(pcbinfo, faddr, fport,
		    laddr, lport, numa_domain, fib);
		if (inp != NULL) {
			if (__predict_true(inp_smr_lock(inp, lockflags))) {
				if (__predict_true(in6_pcblookup_wild_match(inp,
				    laddr, lport, fib) != INPLOOKUP_MATCH_NONE))
					return (inp);
				inp_unlock(inp, lockflags);
			}
			inp = INP_LOOKUP_AGAIN;
		} else {
			inp = in6_pcblookup_hash_wild_smr(pcbinfo, laddr, lport,
			    fib, lockflags);
		}
		if (inp == INP_LOOKUP_AGAIN) {
			return (in6_pcblookup_hash(pcbinfo, faddr, fport, laddr,
			    lport, lookupflags, numa_domain, fib));
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
in6_pcblookup(struct inpcbinfo *pcbinfo, const struct in6_addr *faddr,
    u_int fport, const struct in6_addr *laddr, u_int lport, int lookupflags,
    struct ifnet *ifp)
{
	int fib;

	fib = (lookupflags & INPLOOKUP_FIB) ? if_getfib(ifp) : RT_ALL_FIBS;
	return (in6_pcblookup_hash_smr(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags, M_NODOM, fib));
}

struct inpcb *
in6_pcblookup_mbuf(struct inpcbinfo *pcbinfo, const struct in6_addr *faddr,
    u_int fport, const struct in6_addr *laddr, u_int lport, int lookupflags,
    struct ifnet *ifp __unused, struct mbuf *m)
{
	int fib;

	M_ASSERTPKTHDR(m);
	fib = (lookupflags & INPLOOKUP_FIB) ? M_GETFIB(m) : RT_ALL_FIBS;
	return (in6_pcblookup_hash_smr(pcbinfo, faddr, fport, laddr, lport,
	    lookupflags, m->m_pkthdr.numa_domain, fib));
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

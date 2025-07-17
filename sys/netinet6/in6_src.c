/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 *	$KAME: in6_src.c,v 1.132 2003/08/26 04:42:27 keiichi Exp $
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/rmlock.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_private.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/if_llatbl.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_fib.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>

static struct mtx addrsel_lock;
#define	ADDRSEL_LOCK_INIT()	mtx_init(&addrsel_lock, "addrsel_lock", NULL, MTX_DEF)
#define	ADDRSEL_LOCK()		mtx_lock(&addrsel_lock)
#define	ADDRSEL_UNLOCK()	mtx_unlock(&addrsel_lock)
#define	ADDRSEL_LOCK_ASSERT()	mtx_assert(&addrsel_lock, MA_OWNED)

static struct sx addrsel_sxlock;
#define	ADDRSEL_SXLOCK_INIT()	sx_init(&addrsel_sxlock, "addrsel_sxlock")
#define	ADDRSEL_SLOCK()		sx_slock(&addrsel_sxlock)
#define	ADDRSEL_SUNLOCK()	sx_sunlock(&addrsel_sxlock)
#define	ADDRSEL_XLOCK()		sx_xlock(&addrsel_sxlock)
#define	ADDRSEL_XUNLOCK()	sx_xunlock(&addrsel_sxlock)

#define ADDR_LABEL_NOTAPP (-1)
VNET_DEFINE_STATIC(struct in6_addrpolicy, defaultaddrpolicy);
#define	V_defaultaddrpolicy		VNET(defaultaddrpolicy)

VNET_DEFINE(int, ip6_prefer_tempaddr) = 0;

static int selectroute(struct sockaddr_in6 *, struct ip6_pktopts *,
	struct ip6_moptions *, struct route_in6 *, struct ifnet **,
	struct nhop_object **, int, u_int, uint32_t);
static int in6_selectif(struct sockaddr_in6 *, struct ip6_pktopts *,
	struct ip6_moptions *, struct ifnet **,
	struct ifnet *, u_int);
static int in6_selectsrc(uint32_t, struct sockaddr_in6 *,
	struct ip6_pktopts *, struct inpcb *, struct ucred *,
	struct ifnet **, struct in6_addr *);

static struct in6_addrpolicy *lookup_addrsel_policy(struct sockaddr_in6 *);

static void init_policy_queue(void);
static int add_addrsel_policyent(struct in6_addrpolicy *);
static int delete_addrsel_policyent(struct in6_addrpolicy *);
static int walk_addrsel_policy(int (*)(struct in6_addrpolicy *, void *),
	void *);
static int dump_addrsel_policyent(struct in6_addrpolicy *, void *);
static struct in6_addrpolicy *match_addrsel_policy(struct sockaddr_in6 *);

/*
 * Return an IPv6 address, which is the most appropriate for a given
 * destination and user specified options.
 * If necessary, this function lookups the routing table and returns
 * an entry to the caller for later use.
 */
#define REPLACE(r) do {\
	IP6STAT_INC2(ip6s_sources_rule, (r)); \
	/* { \
	char ip6buf[INET6_ADDRSTRLEN], ip6b[INET6_ADDRSTRLEN]; \
	printf("in6_selectsrc: replace %s with %s by %d\n", ia_best ? ip6_sprintf(ip6buf, &ia_best->ia_addr.sin6_addr) : "none", ip6_sprintf(ip6b, &ia->ia_addr.sin6_addr), (r)); \
	} */ \
	goto replace; \
} while(0)
#define NEXT(r) do {\
	/* { \
	char ip6buf[INET6_ADDRSTRLEN], ip6b[INET6_ADDRSTRLEN]; \
	printf("in6_selectsrc: keep %s against %s by %d\n", ia_best ? ip6_sprintf(ip6buf, &ia_best->ia_addr.sin6_addr) : "none", ip6_sprintf(ip6b, &ia->ia_addr.sin6_addr), (r)); \
	} */ \
	goto next;		/* XXX: we can't use 'continue' here */ \
} while(0)
#define BREAK(r) do { \
	IP6STAT_INC2(ip6s_sources_rule, (r)); \
	goto out;		/* XXX: we can't use 'break' here */ \
} while(0)

static int
in6_selectsrc(uint32_t fibnum, struct sockaddr_in6 *dstsock,
    struct ip6_pktopts *opts, struct inpcb *inp, struct ucred *cred,
    struct ifnet **ifpp, struct in6_addr *srcp)
{
	struct rm_priotracker in6_ifa_tracker;
	struct in6_addr dst, tmp;
	struct ifnet *ifp = NULL, *oifp = NULL;
	struct in6_ifaddr *ia = NULL, *ia_best = NULL;
	struct in6_pktinfo *pi = NULL;
	int dst_scope = -1, best_scope = -1, best_matchlen = -1;
	struct in6_addrpolicy *dst_policy = NULL, *best_policy = NULL;
	u_int32_t odstzone;
	int prefer_tempaddr;
	int error;
	struct ip6_moptions *mopts;

	NET_EPOCH_ASSERT();
	KASSERT(srcp != NULL, ("%s: srcp is NULL", __func__));

	dst = dstsock->sin6_addr; /* make a copy for local operation */
	if (ifpp) {
		/*
		 * Save a possibly passed in ifp for in6_selectsrc. Only
		 * neighbor discovery code should use this feature, where
		 * we may know the interface but not the FIB number holding
		 * the connected subnet in case someone deleted it from the
		 * default FIB and we need to check the interface.
		 */
		if (*ifpp != NULL)
			oifp = *ifpp;
		*ifpp = NULL;
	}

	if (inp != NULL) {
		INP_LOCK_ASSERT(inp);
		mopts = inp->in6p_moptions;
	} else {
		mopts = NULL;
	}

	/*
	 * If the source address is explicitly specified by the caller,
	 * check if the requested source address is indeed a unicast address
	 * assigned to the node, and can be used as the packet's source
	 * address.  If everything is okay, use the address as source.
	 */
	if (opts && (pi = opts->ip6po_pktinfo) &&
	    !IN6_IS_ADDR_UNSPECIFIED(&pi->ipi6_addr)) {
		/* get the outgoing interface */
		if ((error = in6_selectif(dstsock, opts, mopts, &ifp, oifp,
		    fibnum))
		    != 0)
			return (error);

		/*
		 * determine the appropriate zone id of the source based on
		 * the zone of the destination and the outgoing interface.
		 * If the specified address is ambiguous wrt the scope zone,
		 * the interface must be specified; otherwise, ifa_ifwithaddr()
		 * will fail matching the address.
		 */
		tmp = pi->ipi6_addr;
		if (ifp) {
			error = in6_setscope(&tmp, ifp, &odstzone);
			if (error)
				return (error);
		}
		if (cred != NULL && (error = prison_local_ip6(cred,
		    &tmp, (inp->inp_flags & IN6P_IPV6_V6ONLY) != 0)) != 0)
			return (error);

		/*
		 * If IPV6_BINDANY socket option is set, we allow to specify
		 * non local addresses as source address in IPV6_PKTINFO
		 * ancillary data.
		 */
		if ((inp->inp_flags & INP_BINDANY) == 0) {
			ia = in6ifa_ifwithaddr(&tmp, 0 /* XXX */, false);
			if (ia == NULL || (ia->ia6_flags & (IN6_IFF_ANYCAST |
			    IN6_IFF_NOTREADY)))
				return (EADDRNOTAVAIL);
			bcopy(&ia->ia_addr.sin6_addr, srcp, sizeof(*srcp));
		} else
			bcopy(&tmp, srcp, sizeof(*srcp));
		pi->ipi6_addr = tmp; /* XXX: this overrides pi */
		if (ifpp)
			*ifpp = ifp;
		return (0);
	}

	/*
	 * Otherwise, if the socket has already bound the source, just use it.
	 */
	if (inp != NULL && !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		if (cred != NULL &&
		    (error = prison_local_ip6(cred, &inp->in6p_laddr,
		    ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0))) != 0)
			return (error);
		bcopy(&inp->in6p_laddr, srcp, sizeof(*srcp));
		return (0);
	}

	/*
	 * Bypass source address selection and use the primary jail IP
	 * if requested.
	 */
	if (cred != NULL && !prison_saddrsel_ip6(cred, srcp))
		return (0);

	/*
	 * If the address is not specified, choose the best one based on
	 * the outgoing interface and the destination address.
	 */
	/* get the outgoing interface */
	if ((error = in6_selectif(dstsock, opts, mopts, &ifp, oifp,
	    (inp != NULL) ? inp->inp_inc.inc_fibnum : fibnum)) != 0)
		return (error);

#ifdef DIAGNOSTIC
	if (ifp == NULL)	/* this should not happen */
		panic("in6_selectsrc: NULL ifp");
#endif
	error = in6_setscope(&dst, ifp, &odstzone);
	if (error)
		return (error);

	IN6_IFADDR_RLOCK(&in6_ifa_tracker);
	CK_STAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
		int new_scope = -1, new_matchlen = -1;
		struct in6_addrpolicy *new_policy = NULL;
		u_int32_t srczone, osrczone, dstzone;
		struct in6_addr src;
		struct ifnet *ifp1 = ia->ia_ifp;

		/*
		 * We'll never take an address that breaks the scope zone
		 * of the destination.  We also skip an address if its zone
		 * does not contain the outgoing interface.
		 * XXX: we should probably use sin6_scope_id here.
		 */
		if (in6_setscope(&dst, ifp1, &dstzone) ||
		    odstzone != dstzone) {
			continue;
		}
		src = ia->ia_addr.sin6_addr;
		if (in6_setscope(&src, ifp, &osrczone) ||
		    in6_setscope(&src, ifp1, &srczone) ||
		    osrczone != srczone) {
			continue;
		}

		/* avoid unusable addresses */
		if ((ia->ia6_flags &
		     (IN6_IFF_NOTREADY | IN6_IFF_ANYCAST | IN6_IFF_DETACHED))) {
				continue;
		}
		if (!V_ip6_use_deprecated && IFA6_IS_DEPRECATED(ia))
			continue;

		/* If jailed only take addresses of the jail into account. */
		if (cred != NULL &&
		    prison_check_ip6(cred, &ia->ia_addr.sin6_addr) != 0)
			continue;

		/* Rule 1: Prefer same address */
		if (IN6_ARE_ADDR_EQUAL(&dst, &ia->ia_addr.sin6_addr)) {
			ia_best = ia;
			BREAK(1); /* there should be no better candidate */
		}

		if (ia_best == NULL)
			REPLACE(0);

		/* Rule 2: Prefer appropriate scope */
		if (dst_scope < 0)
			dst_scope = in6_addrscope(&dst);
		new_scope = in6_addrscope(&ia->ia_addr.sin6_addr);
		if (IN6_ARE_SCOPE_CMP(best_scope, new_scope) < 0) {
			if (IN6_ARE_SCOPE_CMP(best_scope, dst_scope) < 0)
				REPLACE(2);
			NEXT(2);
		} else if (IN6_ARE_SCOPE_CMP(new_scope, best_scope) < 0) {
			if (IN6_ARE_SCOPE_CMP(new_scope, dst_scope) < 0)
				NEXT(2);
			REPLACE(2);
		}

		/*
		 * Rule 3: Avoid deprecated addresses.  Note that the case of
		 * !ip6_use_deprecated is already rejected above.
		 */
		if (!IFA6_IS_DEPRECATED(ia_best) && IFA6_IS_DEPRECATED(ia))
			NEXT(3);
		if (IFA6_IS_DEPRECATED(ia_best) && !IFA6_IS_DEPRECATED(ia))
			REPLACE(3);

		/* Rule 4: Prefer home addresses */
		/*
		 * XXX: This is a TODO.  We should probably merge the MIP6
		 * case above.
		 */

		/* Rule 5: Prefer outgoing interface */
		if (!(ND_IFINFO(ifp)->flags & ND6_IFF_NO_PREFER_IFACE)) {
			if (ia_best->ia_ifp == ifp && ia->ia_ifp != ifp)
				NEXT(5);
			if (ia_best->ia_ifp != ifp && ia->ia_ifp == ifp)
				REPLACE(5);
		}

		/*
		 * Rule 6: Prefer matching label
		 * Note that best_policy should be non-NULL here.
		 */
		if (dst_policy == NULL)
			dst_policy = lookup_addrsel_policy(dstsock);
		if (dst_policy->label != ADDR_LABEL_NOTAPP) {
			new_policy = lookup_addrsel_policy(&ia->ia_addr);
			if (dst_policy->label == best_policy->label &&
			    dst_policy->label != new_policy->label)
				NEXT(6);
			if (dst_policy->label != best_policy->label &&
			    dst_policy->label == new_policy->label)
				REPLACE(6);
		}

		/*
		 * Rule 7: Prefer public addresses.
		 * We allow users to reverse the logic by configuring
		 * a sysctl variable, so that privacy conscious users can
		 * always prefer temporary addresses.
		 */
		if (opts == NULL ||
		    opts->ip6po_prefer_tempaddr == IP6PO_TEMPADDR_SYSTEM) {
			prefer_tempaddr = V_ip6_prefer_tempaddr;
		} else if (opts->ip6po_prefer_tempaddr ==
		    IP6PO_TEMPADDR_NOTPREFER) {
			prefer_tempaddr = 0;
		} else
			prefer_tempaddr = 1;
		if (!(ia_best->ia6_flags & IN6_IFF_TEMPORARY) &&
		    (ia->ia6_flags & IN6_IFF_TEMPORARY)) {
			if (prefer_tempaddr)
				REPLACE(7);
			else
				NEXT(7);
		}
		if ((ia_best->ia6_flags & IN6_IFF_TEMPORARY) &&
		    !(ia->ia6_flags & IN6_IFF_TEMPORARY)) {
			if (prefer_tempaddr)
				NEXT(7);
			else
				REPLACE(7);
		}

		/*
		 * Rule 8: prefer addresses on alive interfaces.
		 * This is a KAME specific rule.
		 */
		if ((ia_best->ia_ifp->if_flags & IFF_UP) &&
		    !(ia->ia_ifp->if_flags & IFF_UP))
			NEXT(8);
		if (!(ia_best->ia_ifp->if_flags & IFF_UP) &&
		    (ia->ia_ifp->if_flags & IFF_UP))
			REPLACE(8);

		/*
		 * Rule 9: prefer address with better virtual status.
		 */
		if (ifa_preferred(&ia_best->ia_ifa, &ia->ia_ifa))
			REPLACE(9);
		if (ifa_preferred(&ia->ia_ifa, &ia_best->ia_ifa))
			NEXT(9);

		/*
		 * Rule 10: prefer address with `prefer_source' flag.
		 */
		if ((ia_best->ia6_flags & IN6_IFF_PREFER_SOURCE) == 0 &&
		    (ia->ia6_flags & IN6_IFF_PREFER_SOURCE) != 0)
			REPLACE(10);
		if ((ia_best->ia6_flags & IN6_IFF_PREFER_SOURCE) != 0 &&
		    (ia->ia6_flags & IN6_IFF_PREFER_SOURCE) == 0)
			NEXT(10);

		/*
		 * Rule 14: Use longest matching prefix.
		 * Note: in the address selection draft, this rule is
		 * documented as "Rule 8".  However, since it is also
		 * documented that this rule can be overridden, we assign
		 * a large number so that it is easy to assign smaller numbers
		 * to more preferred rules.
		 */
		new_matchlen = in6_matchlen(&ia->ia_addr.sin6_addr, &dst);
		if (best_matchlen < new_matchlen)
			REPLACE(14);
		if (new_matchlen < best_matchlen)
			NEXT(14);

		/* Rule 15 is reserved. */

		/*
		 * Last resort: just keep the current candidate.
		 * Or, do we need more rules?
		 */
		continue;

	  replace:
		ia_best = ia;
		best_scope = (new_scope >= 0 ? new_scope :
			      in6_addrscope(&ia_best->ia_addr.sin6_addr));
		best_policy = (new_policy ? new_policy :
			       lookup_addrsel_policy(&ia_best->ia_addr));
		best_matchlen = (new_matchlen >= 0 ? new_matchlen :
				 in6_matchlen(&ia_best->ia_addr.sin6_addr,
					      &dst));

	  next:
		continue;

	  out:
		break;
	}

	if ((ia = ia_best) == NULL) {
		IN6_IFADDR_RUNLOCK(&in6_ifa_tracker);
		IP6STAT_INC(ip6s_sources_none);
		return (EADDRNOTAVAIL);
	}

	/*
	 * At this point at least one of the addresses belonged to the jail
	 * but it could still be, that we want to further restrict it, e.g.
	 * theoratically IN6_IS_ADDR_LOOPBACK.
	 * It must not be IN6_IS_ADDR_UNSPECIFIED anymore.
	 * prison_local_ip6() will fix an IN6_IS_ADDR_LOOPBACK but should
	 * let all others previously selected pass.
	 * Use tmp to not change ::1 on lo0 to the primary jail address.
	 */
	tmp = ia->ia_addr.sin6_addr;
	if (cred != NULL && prison_local_ip6(cred, &tmp, (inp != NULL &&
	    (inp->inp_flags & IN6P_IPV6_V6ONLY) != 0)) != 0) {
		IN6_IFADDR_RUNLOCK(&in6_ifa_tracker);
		IP6STAT_INC(ip6s_sources_none);
		return (EADDRNOTAVAIL);
	}

	if (ifpp)
		*ifpp = ifp;

	bcopy(&tmp, srcp, sizeof(*srcp));
	if (ia->ia_ifp == ifp)
		IP6STAT_INC2(ip6s_sources_sameif, best_scope);
	else
		IP6STAT_INC2(ip6s_sources_otherif, best_scope);
	if (dst_scope == best_scope)
		IP6STAT_INC2(ip6s_sources_samescope, best_scope);
	else
		IP6STAT_INC2(ip6s_sources_otherscope, best_scope);
	if (IFA6_IS_DEPRECATED(ia))
		IP6STAT_INC2(ip6s_sources_deprecated, best_scope);
	IN6_IFADDR_RUNLOCK(&in6_ifa_tracker);
	return (0);
}

/*
 * Select source address based on @inp, @dstsock and @opts.
 * Stores selected address to @srcp. If @scope_ambiguous is set,
 * embed scope from selected outgoing interface. If @hlim pointer
 * is provided, stores calculated hop limit there.
 * Returns 0 on success.
 */
int
in6_selectsrc_socket(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct inpcb *inp, struct ucred *cred, int scope_ambiguous,
    struct in6_addr *srcp, int *hlim)
{
	struct ifnet *retifp;
	uint32_t fibnum;
	int error;

	fibnum = inp->inp_inc.inc_fibnum;
	retifp = NULL;

	error = in6_selectsrc(fibnum, dstsock, opts, inp, cred, &retifp, srcp);
	if (error != 0)
		return (error);

	if (hlim != NULL)
		*hlim = in6_selecthlim(inp, retifp);

	if (retifp == NULL || scope_ambiguous == 0)
		return (0);

	/*
	 * Application should provide a proper zone ID or the use of
	 * default zone IDs should be enabled.  Unfortunately, some
	 * applications do not behave as it should, so we need a
	 * workaround.  Even if an appropriate ID is not determined
	 * (when it's required), if we can determine the outgoing
	 * interface. determine the zone ID based on the interface.
	 */
	error = in6_setscope(&dstsock->sin6_addr, retifp, NULL);

	return (error);
}

/*
 * Select source address based on @fibnum, @dst and @scopeid.
 * Stores selected address to @srcp.
 * Returns 0 on success.
 *
 * Used by non-socket based consumers (ND code mostly)
 */
int
in6_selectsrc_addr(uint32_t fibnum, const struct in6_addr *dst,
    uint32_t scopeid, struct ifnet *ifp, struct in6_addr *srcp,
    int *hlim)
{
	struct ifnet *retifp;
	struct sockaddr_in6 dst_sa;
	int error;

	retifp = ifp;
	bzero(&dst_sa, sizeof(dst_sa));
	dst_sa.sin6_family = AF_INET6;
	dst_sa.sin6_len = sizeof(dst_sa);
	dst_sa.sin6_addr = *dst;
	dst_sa.sin6_scope_id = scopeid;
	sa6_embedscope(&dst_sa, 0);

	error = in6_selectsrc(fibnum, &dst_sa, NULL, NULL, NULL, &retifp, srcp);
	if (hlim != NULL)
		*hlim = in6_selecthlim(NULL, retifp);

	return (error);
}

static struct nhop_object *
cache_route(uint32_t fibnum, const struct sockaddr_in6 *dst, struct route_in6 *ro,
    uint32_t flowid)
{
	/*
	 * Use a cached route if it exists and is valid, else try to allocate
	 * a new one. Note that we should check the address family of the
	 * cached destination, in case of sharing the cache with IPv4.
	 * Assumes that 'struct route_in6' is exclusively locked.
	 */
	if (ro->ro_nh != NULL && (
	    !NH_IS_VALID(ro->ro_nh) || ro->ro_dst.sin6_family != AF_INET6 ||
	    !IN6_ARE_ADDR_EQUAL(&ro->ro_dst.sin6_addr, &dst->sin6_addr)))
		RO_NHFREE(ro);

	if (ro->ro_nh == NULL) {
		ro->ro_dst = *dst;

		const struct in6_addr *paddr;
		struct in6_addr unscoped_addr;
		uint32_t scopeid = 0;
		if (IN6_IS_SCOPE_LINKLOCAL(&dst->sin6_addr)) {
			in6_splitscope(&dst->sin6_addr, &unscoped_addr, &scopeid);
			paddr = &unscoped_addr;
		} else
			paddr = &dst->sin6_addr;
		ro->ro_nh = fib6_lookup(fibnum, paddr, scopeid, NHR_REF, flowid);
	}
	return (ro->ro_nh);
}

static struct nhop_object *
lookup_route(uint32_t fibnum, struct sockaddr_in6 *dst, struct route_in6 *ro,
    struct ip6_pktopts *opts, uint32_t flowid)
{
	struct nhop_object *nh = NULL;

	/*
	 * If the next hop address for the packet is specified by the caller,
	 * use it as the gateway.
	 */
	if (opts && opts->ip6po_nexthop) {
		struct route_in6 *ron = &opts->ip6po_nextroute;
		struct sockaddr_in6 *sin6_next = satosin6(opts->ip6po_nexthop);

		nh = cache_route(fibnum, sin6_next, ron, flowid);

		/*
		 * The node identified by that address must be a
		 * neighbor of the sending host.
		 */
		if (nh != NULL && (nh->nh_flags & NHF_GATEWAY) != 0)
			nh = NULL;
	} else if (ro != NULL) {
		nh = cache_route(fibnum, dst, ro, flowid);
		if (nh == NULL)
			return (NULL);

		/*
		 * Check if the outgoing interface conflicts with
		 * the interface specified by ipi6_ifindex (if specified).
		 */
		struct in6_pktinfo *pi;
		if (opts && (pi = opts->ip6po_pktinfo) != NULL && pi->ipi6_ifindex) {
			if (nh->nh_aifp->if_index != pi->ipi6_ifindex)
				nh = NULL;
		}
	}

	return (nh);
}

/*
 * Finds outgoing nexthop or the outgoing interface for the
 * @dstsock.
 * Return 0 on success and stores the lookup result in @retnh and @retifp
 */
static int
selectroute(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct route_in6 *ro,
    struct ifnet **retifp, struct nhop_object **retnh, int norouteok,
    u_int fibnum, uint32_t flowid)
{
	int error = 0;
	struct ifnet *ifp = NULL;
	struct in6_pktinfo *pi = NULL;
	struct in6_addr *dst = &dstsock->sin6_addr;

	/* If the caller specify the outgoing interface explicitly, use it. */
	if (opts && (pi = opts->ip6po_pktinfo) != NULL && pi->ipi6_ifindex) {
		/* XXX boundary check is assumed to be already done. */
		ifp = ifnet_byindex(pi->ipi6_ifindex);
		if (ifp != NULL && (norouteok || IN6_IS_ADDR_MULTICAST(dst))) {
			/*
			 * we do not have to check or get the route for
			 * multicast.
			 */
			goto done;
		} else
			goto getroute;
	}
	/*
	 * If the destination address is a multicast address and the outgoing
	 * interface for the address is specified by the caller, use it.
	 */
	if (IN6_IS_ADDR_MULTICAST(dst) &&
	    mopts != NULL && (ifp = mopts->im6o_multicast_ifp) != NULL) {
		goto done; /* we do not need a route for multicast. */
	}
	/*
	 * If destination address is LLA or link- or node-local multicast,
	 * use it's embedded scope zone id to determine outgoing interface.
	 */
	if (IN6_IS_ADDR_MC_LINKLOCAL(dst) ||
	    IN6_IS_ADDR_MC_NODELOCAL(dst)) {
		uint32_t zoneid = ntohs(in6_getscope(dst));
		if (zoneid > 0) {
			ifp = in6_getlinkifnet(zoneid);
			goto done;
		}
	}

  getroute:;
	struct nhop_object *nh = lookup_route(fibnum, dstsock, ro, opts, flowid);
	if (nh != NULL) {
		*retifp = nh->nh_aifp;
		error = 0;
	} else {
		*retifp = NULL;
		IP6STAT_INC(ip6s_noroute);
		error = EHOSTUNREACH;
	}
	*retnh = nh;
	return (error);

  done:
	if (ifp == NULL) {
		/*
		 * This can happen if the caller did not pass a cached route
		 * nor any other hints.  We treat this case an error.
		 */
		error = EHOSTUNREACH;
	}
	if (error == EHOSTUNREACH)
		IP6STAT_INC(ip6s_noroute);

	*retifp = ifp;
	*retnh = NULL;

	return (error);
}

static int
in6_selectif(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct ifnet **retifp,
    struct ifnet *oifp, u_int fibnum)
{
	int error;
	struct route_in6 sro;
	struct nhop_object *nh = NULL;
	uint16_t nh_flags;

	KASSERT(retifp != NULL, ("%s: retifp is NULL", __func__));

	bzero(&sro, sizeof(sro));
	nh_flags = 0;

	error = selectroute(dstsock, opts, mopts, &sro, retifp, &nh, 1, fibnum, 0);

	if (nh != NULL)
		nh_flags = nh->nh_flags;
	if (nh != NULL && nh == sro.ro_nh)
		NH_FREE(nh);

	if (error != 0) {
		/* Help ND. See oifp comment in in6_selectsrc(). */
		if (oifp != NULL && fibnum == RT_DEFAULT_FIB) {
			*retifp = oifp;
			error = 0;
		}
		return (error);
	}

	/*
	 * do not use a rejected or black hole route.
	 * XXX: this check should be done in the L2 output routine.
	 * However, if we skipped this check here, we'd see the following
	 * scenario:
	 * - install a rejected route for a scoped address prefix
	 *   (like fe80::/10)
	 * - send a packet to a destination that matches the scoped prefix,
	 *   with ambiguity about the scope zone.
	 * - pick the outgoing interface from the route, and disambiguate the
	 *   scope zone with the interface.
	 * - ip6_output() would try to get another route with the "new"
	 *   destination, which may be valid.
	 * - we'd see no error on output.
	 * Although this may not be very harmful, it should still be confusing.
	 * We thus reject the case here.
	 */

	if (nh_flags & (NHF_REJECT | NHF_BLACKHOLE)) {
		error = (nh_flags & NHF_HOST ? EHOSTUNREACH : ENETUNREACH);
		return (error);
	}

	return (0);
}

/* Public wrapper function to selectroute(). */
int
in6_selectroute(struct sockaddr_in6 *dstsock, struct ip6_pktopts *opts,
    struct ip6_moptions *mopts, struct route_in6 *ro,
    struct ifnet **retifp, struct nhop_object **retnh, u_int fibnum, uint32_t flowid)
{
	MPASS(retifp != NULL);
	MPASS(retnh != NULL);

	return (selectroute(dstsock, opts, mopts, ro, retifp,
	    retnh, 0, fibnum, flowid));
}

/*
 * Default hop limit selection. The precedence is as follows:
 * 1. Hoplimit value specified via ioctl.
 * 2. (If the outgoing interface is detected) the current
 *     hop limit of the interface specified by router advertisement.
 * 3. The system default hoplimit.
 */
int
in6_selecthlim(struct inpcb *inp, struct ifnet *ifp)
{

	if (inp && inp->in6p_hops >= 0)
		return (inp->in6p_hops);
	else if (ifp)
		return (ND_IFINFO(ifp)->chlim);
	else if (inp && !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_faddr)) {
		struct nhop_object *nh;
		struct in6_addr dst;
		uint32_t fibnum, scopeid;
		int hlim;

		fibnum = inp->inp_inc.inc_fibnum;
		in6_splitscope(&inp->in6p_faddr, &dst, &scopeid);
		nh = fib6_lookup(fibnum, &dst, scopeid, 0, 0);
		if (nh != NULL) {
			hlim = ND_IFINFO(nh->nh_ifp)->chlim;
			return (hlim);
		}
	}
	return (V_ip6_defhlim);
}

void
addrsel_policy_init(void)
{

	init_policy_queue();

	/* initialize the "last resort" policy */
	bzero(&V_defaultaddrpolicy, sizeof(V_defaultaddrpolicy));
	V_defaultaddrpolicy.label = ADDR_LABEL_NOTAPP;

	if (!IS_DEFAULT_VNET(curvnet))
		return;

	ADDRSEL_LOCK_INIT();
	ADDRSEL_SXLOCK_INIT();
}

static struct in6_addrpolicy *
lookup_addrsel_policy(struct sockaddr_in6 *key)
{
	struct in6_addrpolicy *match = NULL;

	ADDRSEL_LOCK();
	match = match_addrsel_policy(key);

	if (match == NULL)
		match = &V_defaultaddrpolicy;
	else
		match->use++;
	ADDRSEL_UNLOCK();

	return (match);
}

/*
 * Subroutines to manage the address selection policy table via sysctl.
 */
struct walkarg {
	struct sysctl_req *w_req;
};

static int in6_src_sysctl(SYSCTL_HANDLER_ARGS);
SYSCTL_DECL(_net_inet6_ip6);
static SYSCTL_NODE(_net_inet6_ip6, IPV6CTL_ADDRCTLPOLICY, addrctlpolicy,
    CTLFLAG_RD | CTLFLAG_MPSAFE, in6_src_sysctl,
    "");

static int
in6_src_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct walkarg w;

	if (req->newptr)
		return EPERM;

	bzero(&w, sizeof(w));
	w.w_req = req;

	return (walk_addrsel_policy(dump_addrsel_policyent, &w));
}

int
in6_src_ioctl(u_long cmd, caddr_t data)
{
	struct in6_addrpolicy ent0;

	if (cmd != SIOCAADDRCTL_POLICY && cmd != SIOCDADDRCTL_POLICY)
		return (EOPNOTSUPP); /* check for safety */

	ent0 = *(struct in6_addrpolicy *)data;

	if (ent0.label == ADDR_LABEL_NOTAPP)
		return (EINVAL);
	/* check if the prefix mask is consecutive. */
	if (in6_mask2len(&ent0.addrmask.sin6_addr, NULL) < 0)
		return (EINVAL);
	/* clear trailing garbages (if any) of the prefix address. */
	IN6_MASK_ADDR(&ent0.addr.sin6_addr, &ent0.addrmask.sin6_addr);
	ent0.use = 0;

	switch (cmd) {
	case SIOCAADDRCTL_POLICY:
		return (add_addrsel_policyent(&ent0));
	case SIOCDADDRCTL_POLICY:
		return (delete_addrsel_policyent(&ent0));
	}

	return (0);		/* XXX: compromise compilers */
}

/*
 * The followings are implementation of the policy table using a
 * simple tail queue.
 * XXX such details should be hidden.
 * XXX implementation using binary tree should be more efficient.
 */
struct addrsel_policyent {
	TAILQ_ENTRY(addrsel_policyent) ape_entry;
	struct in6_addrpolicy ape_policy;
};

TAILQ_HEAD(addrsel_policyhead, addrsel_policyent);

VNET_DEFINE_STATIC(struct addrsel_policyhead, addrsel_policytab);
#define	V_addrsel_policytab		VNET(addrsel_policytab)

static void
init_policy_queue(void)
{

	TAILQ_INIT(&V_addrsel_policytab);
}

static int
add_addrsel_policyent(struct in6_addrpolicy *newpolicy)
{
	struct addrsel_policyent *new, *pol;

	new = malloc(sizeof(*new), M_IFADDR,
	       M_WAITOK);
	ADDRSEL_XLOCK();
	ADDRSEL_LOCK();

	/* duplication check */
	TAILQ_FOREACH(pol, &V_addrsel_policytab, ape_entry) {
		if (IN6_ARE_ADDR_EQUAL(&newpolicy->addr.sin6_addr,
				       &pol->ape_policy.addr.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&newpolicy->addrmask.sin6_addr,
				       &pol->ape_policy.addrmask.sin6_addr)) {
			ADDRSEL_UNLOCK();
			ADDRSEL_XUNLOCK();
			free(new, M_IFADDR);
			return (EEXIST);	/* or override it? */
		}
	}

	bzero(new, sizeof(*new));

	/* XXX: should validate entry */
	new->ape_policy = *newpolicy;

	TAILQ_INSERT_TAIL(&V_addrsel_policytab, new, ape_entry);
	ADDRSEL_UNLOCK();
	ADDRSEL_XUNLOCK();

	return (0);
}

static int
delete_addrsel_policyent(struct in6_addrpolicy *key)
{
	struct addrsel_policyent *pol;

	ADDRSEL_XLOCK();
	ADDRSEL_LOCK();

	/* search for the entry in the table */
	TAILQ_FOREACH(pol, &V_addrsel_policytab, ape_entry) {
		if (IN6_ARE_ADDR_EQUAL(&key->addr.sin6_addr,
		    &pol->ape_policy.addr.sin6_addr) &&
		    IN6_ARE_ADDR_EQUAL(&key->addrmask.sin6_addr,
		    &pol->ape_policy.addrmask.sin6_addr)) {
			break;
		}
	}
	if (pol == NULL) {
		ADDRSEL_UNLOCK();
		ADDRSEL_XUNLOCK();
		return (ESRCH);
	}

	TAILQ_REMOVE(&V_addrsel_policytab, pol, ape_entry);
	ADDRSEL_UNLOCK();
	ADDRSEL_XUNLOCK();
	free(pol, M_IFADDR);

	return (0);
}

static int
walk_addrsel_policy(int (*callback)(struct in6_addrpolicy *, void *), void *w)
{
	struct addrsel_policyent *pol;
	int error = 0;

	ADDRSEL_SLOCK();
	TAILQ_FOREACH(pol, &V_addrsel_policytab, ape_entry) {
		if ((error = (*callback)(&pol->ape_policy, w)) != 0) {
			ADDRSEL_SUNLOCK();
			return (error);
		}
	}
	ADDRSEL_SUNLOCK();
	return (error);
}

static int
dump_addrsel_policyent(struct in6_addrpolicy *pol, void *arg)
{
	int error = 0;
	struct walkarg *w = arg;

	error = SYSCTL_OUT(w->w_req, pol, sizeof(*pol));

	return (error);
}

static struct in6_addrpolicy *
match_addrsel_policy(struct sockaddr_in6 *key)
{
	struct addrsel_policyent *pent;
	struct in6_addrpolicy *bestpol = NULL, *pol;
	int matchlen, bestmatchlen = -1;
	u_char *mp, *ep, *k, *p, m;

	TAILQ_FOREACH(pent, &V_addrsel_policytab, ape_entry) {
		matchlen = 0;

		pol = &pent->ape_policy;
		mp = (u_char *)&pol->addrmask.sin6_addr;
		ep = mp + 16;	/* XXX: scope field? */
		k = (u_char *)&key->sin6_addr;
		p = (u_char *)&pol->addr.sin6_addr;
		for (; mp < ep && *mp; mp++, k++, p++) {
			m = *mp;
			if ((*k & m) != *p)
				goto next; /* not match */
			if (m == 0xff) /* short cut for a typical case */
				matchlen += 8;
			else {
				while (m >= 0x80) {
					matchlen++;
					m <<= 1;
				}
			}
		}

		/* matched.  check if this is better than the current best. */
		if (bestpol == NULL ||
		    matchlen > bestmatchlen) {
			bestpol = pol;
			bestmatchlen = matchlen;
		}

	  next:
		continue;
	}

	return (bestpol);
}

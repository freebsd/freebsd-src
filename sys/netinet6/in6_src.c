/*-
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_mpath.h"

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
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/if_llatbl.h>
#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

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
static VNET_DEFINE(struct in6_addrpolicy, defaultaddrpolicy);
#define	V_defaultaddrpolicy		VNET(defaultaddrpolicy)

VNET_DEFINE(int, ip6_prefer_tempaddr) = 0;

static int cached_rtlookup(const struct sockaddr_in6 *dst,
    struct route_in6 *ro, u_int fibnum);
static int handle_nexthop(struct ip6po_nhinfo *nh, u_int fibnum,
    struct ifnet **ifpp);
static int handle_pktinfo(const struct in6_pktinfo* pi, struct ifnet **ifpp,
    struct in6_addr *srcp);

static int lookup_policy_label(const struct in6_addr *, uint32_t);

static void init_policy_queue(void);
static int add_addrsel_policyent(struct in6_addrpolicy *);
static int delete_addrsel_policyent(struct in6_addrpolicy *);
static int walk_addrsel_policy(int (*)(struct in6_addrpolicy *, void *),
	void *);
static int dump_addrsel_policyent(struct in6_addrpolicy *, void *);
static struct in6_addrpolicy *match_addrsel_policy(struct sockaddr_in6 *);
static int in6_srcaddrscope(const struct in6_addr *);

/*
 * Return an IPv6 address, which is the most appropriate for a given
 * destination and user specified options.
 * If necessary, this function lookups the routing table and returns
 * an entry to the caller for later use.
 */
struct srcaddr_choice {
	struct in6_ifaddr *ia;
	int scope;
	int label;
	int prefixlen;
	int rule;
};
struct dstaddr_props {
	struct ifnet *ifp;
	struct in6_addr *addr;
	int scope;
	int label;
	int prefixlen;
};

#define	REPLACE(r)	{ rule = r; goto replace; }
#define	NEXT(r)		{ rule = r; goto next; }

#ifndef IPV6_SASDEBUG
#define	IPV6SASDEBUG(fmt, ...)
#else
#define	IPV6SASDEBUG(fmt, ...)	printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
static char *srcrule_str[IP6S_RULESMAX] = {
	"Rule 0: first candidate",
	"Rule 1: prefer same address",
	"Rule 2: prefer appropriate scope",
	"Rule 3: avoid deprecated addresses",
	"Rule 4: prefer home address",
	"Rule 5: prefer outgoing interface",
	"Rule 6: prefer matching label",
	"Rule 7: prefer temporary addresses",
	"Rule 8: prefer address with better virtual status",
	"Rule 9: prefer address with `prefer_source' flag",
	"Rule 10",
	"Rule 11",
	"Rule 12",
	"Rule 13",
	"Rule 14: use longest matching prefix",
	"Rule 15"
};
#endif

static int
srcaddrcmp(struct srcaddr_choice *c, struct in6_ifaddr *ia,
    struct dstaddr_props *dst, struct ucred *cred,
    struct ip6_pktopts *opts)
{
#ifdef IPV6_SASDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif
	int srcscope, rule, label, prefer_tempaddr, prefixlen;

	IPV6SASDEBUG("candidate address %s from interface %s",
	    ip6_sprintf(buf, &ia->ia_addr.sin6_addr), ia->ia_ifp->if_xname);
	/* Avoid unusable addresses */
	if ((ia->ia6_flags & (IN6_IFF_NOTREADY | IN6_IFF_DETACHED)) ||
	    (ia->ia_ifp->if_flags & IFF_UP) == 0) {
		IPV6SASDEBUG("address %s is unusable",
		    ip6_sprintf(buf, &ia->ia_addr.sin6_addr));
		return (-1);
	}
	/*
	 * In any case, multicast addresses and the unspecified address
	 * MUST NOT be included in a candidate set.
	 */
	if (IN6_IS_ADDR_MULTICAST(IA6_IN6(ia)) ||
	    IN6_IS_ADDR_UNSPECIFIED(IA6_IN6(ia))) {
		IPV6SASDEBUG("skip multicast and unspecified addresses");
		return (-1);
	}
	if (!V_ip6_use_deprecated && IFA6_IS_DEPRECATED(ia)) {
		IPV6SASDEBUG("skip deprecated addresses");
		return (-1);
	}
	/* If jailed, only take addresses of the jail into account. */
	if (cred != NULL && prison_check_ip6(cred, IA6_SIN6(ia)) != 0) {
		IPV6SASDEBUG("address is unusable from the jail");
		return (-1);
	}
	/* Source address can not break the destination zone */
	srcscope = in6_srcaddrscope(IA6_IN6(ia));
	if (ia->ia_ifp != dst->ifp &&
	    in6_getscopezone(ia->ia_ifp, srcscope) !=
	    in6_getscopezone(dst->ifp, dst->scope)) {
		IPV6SASDEBUG("scope zone mismatch");
		return (-1);
	}
	label = ADDR_LABEL_NOTAPP;
	prefixlen = -1;
	/* Rule 1: Prefer same address. */
	if (IN6_ARE_ADDR_EQUAL(IA6_IN6(ia), dst->addr))
		REPLACE(1);
	/* Rule 2: Prefer appropriate scope. */
	if (c->ia == NULL) {
		dst->label = lookup_policy_label(dst->addr,
		    in6_getscopezone(dst->ifp, dst->scope));
		REPLACE(0);
	}
	if (IN6_ARE_SCOPE_CMP(c->scope, srcscope) < 0) {
		if (IN6_ARE_SCOPE_CMP(c->scope, dst->scope) < 0)
			REPLACE(2);
		NEXT(2);
	} else if (IN6_ARE_SCOPE_CMP(srcscope, c->scope) < 0) {
		if (IN6_ARE_SCOPE_CMP(srcscope, dst->scope) < 0)
			NEXT(2);
		REPLACE(2);
	}
	/* Rule 3: Avoid deprecated addresses. */
	if (!IFA6_IS_DEPRECATED(c->ia) && IFA6_IS_DEPRECATED(ia))
		NEXT(3);
	if (IFA6_IS_DEPRECATED(c->ia) && !IFA6_IS_DEPRECATED(ia))
		REPLACE(3);
	/*
	 * Rule 4: Prefer home addresses.
	 * XXX: This is a TODO.
	 */
	/* Rule 5: Prefer outgoing interface. */
	if (!(ND_IFINFO(dst->ifp)->flags & ND6_IFF_NO_PREFER_IFACE)) {
		if (c->ia->ia_ifp == dst->ifp && ia->ia_ifp != dst->ifp)
			NEXT(5);
		if (c->ia->ia_ifp != dst->ifp && ia->ia_ifp == dst->ifp)
			REPLACE(5);
	}
	/*
	 * Rule 5.5: Prefer addresses in a prefix advertised by
	 * the next-hop.
	 * XXX: not yet.
	 */
	/* Rule 6: Prefer matching label. */
	if (dst->label != ADDR_LABEL_NOTAPP) {
		c->label = lookup_policy_label(IA6_IN6(c->ia),
		    in6_getscopezone(c->ia->ia_ifp, c->scope));
		label = lookup_policy_label(IA6_IN6(ia),
		    in6_getscopezone(ia->ia_ifp, srcscope));
		if (c->label == dst->label && label != dst->label)
			NEXT(6);
		if (label == dst->label && c->label != dst->label)
			REPLACE(6);
	}
	/* Rule 7: Prefer temporary addresses. */
	if (opts == NULL ||
	    opts->ip6po_prefer_tempaddr == IP6PO_TEMPADDR_SYSTEM)
		prefer_tempaddr = V_ip6_prefer_tempaddr;
	else if (opts->ip6po_prefer_tempaddr == IP6PO_TEMPADDR_NOTPREFER)
		prefer_tempaddr = 0;
	else
		prefer_tempaddr = 1;
	if ((c->ia->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
	    (ia->ia6_flags & IN6_IFF_TEMPORARY) == 0) {
		if (prefer_tempaddr)
			NEXT(7);
		REPLACE(7);
	}
	if ((c->ia->ia6_flags & IN6_IFF_TEMPORARY) == 0 &&
	    (ia->ia6_flags & IN6_IFF_TEMPORARY) != 0) {
		if (prefer_tempaddr)
			REPLACE(7);
		NEXT(7);
	}
	/*
	 * Rule 8: prefer address with better virtual status.
	 */
	if (ifa_preferred(&c->ia->ia_ifa, &ia->ia_ifa))
		REPLACE(8);
	if (ifa_preferred(&ia->ia_ifa, &c->ia->ia_ifa))
		NEXT(8);
	/*
	 * Rule 9: prefer address with `prefer_source' flag.
	 */
	if ((c->ia->ia6_flags & IN6_IFF_PREFER_SOURCE) == 0 &&
	    (ia->ia6_flags & IN6_IFF_PREFER_SOURCE) != 0)
		REPLACE(9);
	if ((c->ia->ia6_flags & IN6_IFF_PREFER_SOURCE) != 0 &&
	    (ia->ia6_flags & IN6_IFF_PREFER_SOURCE) == 0)
		NEXT(9);
	/* Rule 14: Use longest matching prefix. */
	if (c->prefixlen < 0)
		c->prefixlen = in6_matchlen(IA6_IN6(c->ia), dst->addr);
	prefixlen = in6_matchlen(IA6_IN6(ia), dst->addr);
	if (c->prefixlen > prefixlen)
		NEXT(14);
	if (prefixlen > c->prefixlen)
		REPLACE(14);
	IPV6SASDEBUG("the algorithm failed to select an address");
	return (-1);
replace:
	IPV6SASDEBUG("address %s was selected according to the \"%s\"",
	    ip6_sprintf(buf, &ia->ia_addr.sin6_addr), srcrule_str[rule]);
	c->ia = ia;
	c->label = label;
	c->scope = srcscope;
	c->rule = rule;
	c->prefixlen = prefixlen;
	/* Update statistic */
	IP6STAT_INC(ip6s_sources_rule[rule]);
	return (rule);
next:
	IPV6SASDEBUG("address %s was selected according to the \"%s\"",
	    ip6_sprintf(buf, &c->ia->ia_addr.sin6_addr), srcrule_str[rule]);
	return (rule);
}
#undef	REPLACE
#undef	NEXT

static int
cached_rtlookup(const struct sockaddr_in6 *dst, struct route_in6 *ro,
    u_int fibnum)
{

	/*
	 * Use a cached route if it exists and is valid, else try to allocate
	 * a new one. Note that we should check the address family of the
	 * cached destination, in case of sharing the cache with IPv4.
	 */
	KASSERT(ro != NULL, ("%s: ro is NULL", __func__));
	if (ro->ro_rt != NULL && (
	    (ro->ro_rt->rt_flags & RTF_UP) == 0 ||
	    ro->ro_dst.sin6_family != AF_INET6 ||
	    !IN6_ARE_ADDR_EQUAL(&ro->ro_dst.sin6_addr, &dst->sin6_addr))) {
		RO_RTFREE(ro);
	}
	if (ro->ro_rt == NULL) {
		/* No route yet, so try to acquire one */
		memcpy(&ro->ro_dst, dst, sizeof(*dst));
		in6_rtalloc(ro, fibnum);
	}
	if (ro->ro_rt == NULL)
		return (EHOSTUNREACH);
	return (0);
}

/*
 * pi - options configured via IPV6_PKTINFO;
 *
 * These parameters are returned back to caller:
 * ifpp - determined outgoing interface;
 * srcp - determined source address;
 */
static int
handle_pktinfo(const struct in6_pktinfo* pi, struct ifnet **ifpp,
    struct in6_addr *srcp)
{
	struct ifnet *ifp;

	ifp = NULL;
	if (pi->ipi6_ifindex != 0) {
		ifp = ifnet_byindex(pi->ipi6_ifindex);
		if (ifp == NULL)
			return (ENXIO);
		if (ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED)
			return (ENETDOWN);
	}
	if (ifp != NULL)
		*ifpp = ifp;
	if (IN6_IS_ADDR_UNSPECIFIED(&pi->ipi6_addr))
		return (0);
	*srcp = pi->ipi6_addr;
	return (0);
}

/*
 * nh - next hop destination and route;
 * fibnum - FIB number.
 * ifpp - pointer to outgoing interface.
 *
 * NOTE: we can keep this route, it will be freed in the socket
 * option handling code (see ip6_output.c).
 */
static int
handle_nexthop(struct ip6po_nhinfo *nh, u_int fibnum, struct ifnet **ifpp)
{
	struct sockaddr_in6 *sa;
	struct route_in6 *ro;
	struct ifnet *ifp, *oifp;

	sa = (struct sockaddr_in6 *)nh->ip6po_nhi_nexthop;
	ro = &nh->ip6po_nhi_route;
	if (sa->sin6_family != AF_INET6)
		return (EAFNOSUPPORT);
	/*
	 * If *ifpp is not NULL, this means that outgoing interface
	 * was determined in the PKTINFO handling code.
	 */
	oifp = *ifpp;
	if (IN6_IS_ADDR_LINKLOCAL(&sa->sin6_addr))
		/*
		 * Next hop is LLA, thus it should be neighbor.
		 * Determine outgoing interface by zone index.
		 */
		ifp = in6_getlinkifnet(sa->sin6_scope_id);
	else {
		if (cached_rtlookup(sa, ro, fibnum) != 0)
			return (EHOSTUNREACH);
		/*
		 * The node identified by that address must be a
		 * neighbor of the sending host.
		 */
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			return (EHOSTUNREACH);
		ifp = ro->ro_rt->rt_ifp;
	}
	/*
	 * When the outgoing interface is specified by IPV6_PKTINFO
	 * as well, the next hop specified by this option must be
	 * reachable via the specified interface.
	 */
	if (ifp == NULL || (oifp != NULL && oifp != ifp))
		return (EHOSTUNREACH);

	*ifpp = ifp;
	return (0);
}

static int
check_addrs(const struct sockaddr_in6 *src, const struct sockaddr_in6 *dst,
    struct ifnet *ifp)
{
	struct in6_ifaddr *ia;

	/*
	 * Check that source address is available.
	 */
	ia = in6ifa_ifwithaddr(&src->sin6_addr, src->sin6_scope_id);
	if (ia == NULL || (
	    ia->ia6_flags & (IN6_IFF_ANYCAST | IN6_IFF_NOTREADY))) {
		if (ia != NULL)
			ifa_free(&ia->ia_ifa);
		return (EADDRNOTAVAIL);
	}
	ifa_free(&ia->ia_ifa);
	/*
	 * Check that source address does not break the destination
	 * zone.
	 */
	if (dst->sin6_scope_id != 0 &&
	    dst->sin6_scope_id != in6_getscopezone(ifp,
	    in6_srcaddrscope(&dst->sin6_addr)))
		return (EHOSTUNREACH);
	return (0);
}

int
in6_selectsrc(struct sockaddr_in6 *dst, struct ip6_pktopts *opts,
    struct inpcb *inp, struct route_in6 *ro, struct ucred *cred,
    struct ifnet **ifpp, struct in6_addr *srcp)
{
#ifdef IPV6_SASDEBUG
	char buf[INET6_ADDRSTRLEN];
#endif
	struct route_in6 ro6;
	struct dstaddr_props dstprops;
	struct srcaddr_choice best;
	struct sockaddr_in6 srcsock;
	struct ip6_moptions *mopts;
	struct in6_ifaddr *ia;
	struct ifaddr *ifa;
	struct ifnet *ifp, *oifp;
	u_int fibnum;
	int error;

	KASSERT(srcp != NULL, ("%s: srcp is NULL", __func__));
	KASSERT(ifpp != NULL, ("%s: ifpp is NULL", __func__));
	KASSERT(sa6_checkzone(dst) == 0, ("%s: invalid zone information",
	    __func__));

	ifp = NULL;
	/*
	 * XXX: Save a possibly passed in ifp for in6_selectsrc. Only
	 * neighbor discovery code should use this feature, where
	 * we may know the interface but not the FIB number holding
	 * the connected subnet in case someone deleted it from the
	 * default FIB and we need to check the interface.
	 */
	oifp = *ifpp;
	if (inp != NULL) {
		INP_LOCK_ASSERT(inp);
		mopts = inp->in6p_moptions;
		fibnum = inp->inp_inc.inc_fibnum;
		/* Use "sticky" options if opts isn't specified. */
		if (opts == NULL)
			opts = inp->in6p_outputopts;
	} else {
		mopts = NULL;
		fibnum = RT_DEFAULT_FIB;
	}
	if (ro == NULL) {
		ro = &ro6;
		bzero(ro, sizeof(*ro));
	}
	srcsock = sa6_any;
	if (opts != NULL && opts->ip6po_pktinfo != NULL) {
		error = handle_pktinfo(opts->ip6po_pktinfo, &ifp,
		    &srcsock.sin6_addr);
		if (error != 0)
			return (error);
		if (ifp != NULL) {
			/*
			 * When the outgoing interface is specified by
			 * IPV6_PKTINFO as well, the next hop specified by
			 * this option must be reachable via the specified
			 * interface.
			 * We ignore next hop for multicast destinations.
			 */
			if (!IN6_IS_ADDR_MULTICAST(&dst->sin6_addr) &&
			    opts->ip6po_nexthop != NULL) {
				error = handle_nexthop(&opts->ip6po_nhinfo,
				    fibnum, &ifp);
				if (error != 0)
					return (error);
			}
		}
	}
	if (ifp != NULL)
		goto oif_found;
	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr)) {
		/*
		 * If the destination address is a multicast address and
		 * the IPV6_MULTICAST_IF socket option is specified for the
		 * socket, the interface is used.
		 */
		if (mopts && mopts->im6o_multicast_ifp) {
			ifp = mopts->im6o_multicast_ifp;
		} else if (IN6_IS_ADDR_MC_LINKLOCAL(&dst->sin6_addr) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL(&dst->sin6_addr)) {
			/*
			 * Destination multicast address is in the link-local
			 * or interface-local scope. Use its sin6_scope_id to
			 * determine outgoing interface.
			 */
			if (dst->sin6_scope_id != 0)
				ifp = in6_getlinkifnet(dst->sin6_scope_id);
		} else {
			/*
			 * Try to lookup route for this multicast
			 * destination address.
			 */
			if (cached_rtlookup(dst, ro, fibnum) != 0)
				return (EHOSTUNREACH);
			ifp = ro->ro_rt->rt_ifp;
		}
	} else if (opts != NULL && opts->ip6po_nexthop != NULL) {
		error = handle_nexthop(&opts->ip6po_nhinfo, fibnum, &ifp);
		if (error != 0)
			return (error);
	} else {
		/*
		 * We don't have any options and destination isn't multicast.
		 * Use sin6_scope_id for link-local addresses.
		 * Do a route lookup for global addresses.
		 */
		if (IN6_IS_ADDR_LINKLOCAL(&dst->sin6_addr)) {
			if (dst->sin6_scope_id != 0)
				ifp = in6_getlinkifnet(dst->sin6_scope_id);
		} else {
			if (cached_rtlookup(dst, ro, fibnum) != 0)
				return (EHOSTUNREACH);
			ifp = ro->ro_rt->rt_ifp;
		}
	}
	if (ifp == NULL) {
		if (oifp == NULL)
			return (EHOSTUNREACH);
		/* Use outgoing interface specified by caller. */
		ifp = oifp;
	}
oif_found:
	if (!IN6_IS_ADDR_UNSPECIFIED(&srcsock.sin6_addr)) {
		if (ro == &ro6)
			RO_RTFREE(ro);
		if (cred != NULL) {
			srcsock.sin6_scope_id = in6_getscopezone(ifp,
			    in6_srcaddrscope(&srcsock.sin6_addr));
			error = prison_local_ip6(cred, &srcsock,
			    (inp != NULL &&
			    (inp->inp_flags & IN6P_IPV6_V6ONLY) != 0));
			if (error != 0)
				return (error);
		}
		error = check_addrs(&srcsock, dst, ifp);
		if (error != 0)
			return (error);
		*ifpp = ifp;
		*srcp = srcsock.sin6_addr;
		return (0);
	}
	/*
	 * Otherwise, if the socket has already bound the source, just use it.
	 */
	if (inp != NULL && !IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr)) {
		if (ro == &ro6)
			RO_RTFREE(ro);
		*srcp = inp->in6p_laddr;
		*ifpp = ifp;
		return (0);
	}
	/*
	 * Bypass source address selection and use the primary jail IP
	 * if requested.
	 */
	if (cred != NULL && !prison_saddrsel_ip6(cred, &srcsock)) {
		if (ro == &ro6)
			RO_RTFREE(ro);
		error = check_addrs(&srcsock, dst, ifp);
		if (error != 0)
			return (error);
		*ifpp = ifp;
		*srcp = srcsock.sin6_addr;
		return (0);
	}

	/*
	 * If the address is not specified, choose the best one based on
	 * the outgoing interface and the destination address.
	 */
	IPV6SASDEBUG("dst %s, oif %s", ip6_sprintf(buf, &dst->sin6_addr),
	    ifp->if_xname);
	dstprops.ifp = ifp;
	dstprops.addr = &dst->sin6_addr;
	dstprops.scope = in6_srcaddrscope(&dst->sin6_addr);
	best.rule = -1;
	best.ia = NULL;
	/*
	 * RFC 6724 (section 4):
	 * For all multicast and link-local destination addresses, the set of
	 * candidate source addresses MUST only include addresses assigned to
	 * interfaces belonging to the same link as the outgoing interface.
	 */
	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr) ||
	    dstprops.scope == IPV6_ADDR_SCOPE_LINKLOCAL) {
		IPV6SASDEBUG("use the addresses only from interface %s",
		    ifp->if_xname);
		IF_ADDR_RLOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			error = srcaddrcmp(&best, (struct in6_ifaddr*)ifa,
			    &dstprops, cred, opts);
			if (error == 1)
				break;
		}
		if (best.rule >= 0)
			ifa_ref(&best.ia->ia_ifa);
		IF_ADDR_RUNLOCK(ifp);
	} else {
		IN6_IFADDR_RLOCK();
		TAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
			error = srcaddrcmp(&best, ia, &dstprops, cred, opts);
			if (error == 1)
				break;
		}
		if (best.rule >= 0)
			ifa_ref(&best.ia->ia_ifa);
		IN6_IFADDR_RUNLOCK();
	}
	if (best.rule < 0) {
		IPV6SASDEBUG("Failed to choose an address");
		IP6STAT_INC(ip6s_sources_none);
		if (ro == &ro6)
			RO_RTFREE(ro);
		return (EADDRNOTAVAIL);
	}
	*ifpp = ifp;
	*srcp = best.ia->ia_addr.sin6_addr;

	/* Update statistic */
	if (best.ia->ia_ifp == ifp)
		IP6STAT_INC(ip6s_sources_sameif[best.scope]);
	else
		IP6STAT_INC(ip6s_sources_otherif[best.scope]);
	if (dstprops.scope == best.scope)
		IP6STAT_INC(ip6s_sources_samescope[best.scope]);
	else
		IP6STAT_INC(ip6s_sources_otherscope[best.scope]);
	if (IFA6_IS_DEPRECATED(best.ia))
		IP6STAT_INC(ip6s_sources_deprecated[best.scope]);
	ifa_free(&best.ia->ia_ifa);
	if (ro == &ro6)
		RO_RTFREE(ro);
	return (0);
}

/*
 * Default hop limit selection. The precedence is as follows:
 * 1. Hoplimit value specified via ioctl.
 * 2. (If the outgoing interface is detected) the current
 *     hop limit of the interface specified by router advertisement.
 * 3. If destination address is from link-local scope, use its zoneid
 *    to determine outgoing interface and use its hop limit.
 * 4. The system default hoplimit.
 */
int
in6_selecthlim(struct inpcb *in6p, struct ifnet *ifp)
{
	struct route_in6 ro6;

	if (in6p && in6p->in6p_hops >= 0)
		return (in6p->in6p_hops);

	if (ifp != NULL)
		return (ND_IFINFO(ifp)->chlim);

	/* XXX: should we check for multicast here?*/
	if (in6p && IN6_IS_ADDR_LINKLOCAL(&in6p->in6p_faddr)) {
		if (in6p->in6p_zoneid != 0 &&
		    (ifp = in6_getlinkifnet(in6p->in6p_zoneid)))
			return (ND_IFINFO(ifp)->chlim);
	} else if (in6p && !IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
		bzero(&ro6, sizeof(ro6));
		ro6.ro_dst.sin6_family = AF_INET6;
		ro6.ro_dst.sin6_len = sizeof(struct sockaddr_in6);
		ro6.ro_dst.sin6_addr = in6p->in6p_faddr;
		in6_rtalloc(&ro6, in6p->inp_inc.inc_fibnum);
		if (ro6.ro_rt) {
			ifp = ro6.ro_rt->rt_ifp;
			RTFREE(ro6.ro_rt);
			if (ifp)
				return (ND_IFINFO(ifp)->chlim);
		}
	}
	return (V_ip6_defhlim);
}
/*
 * XXX: this is borrowed from in6_pcbbind(). If possible, we should
 * share this function by all *bsd*...
 */
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

#if 0
	error = prison_local_ip6(cred, laddr,
	    ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0));
	if (error)
		return(error);
#endif

	/* XXX: this is redundant when called from in6_pcbbind */
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0)
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

static int
lookup_policy_label(const struct in6_addr *addr, uint32_t zoneid)
{
	struct sockaddr_in6 sa6;
	struct in6_addrpolicy *match = NULL;

	sa6.sin6_addr = *addr;
	sa6.sin6_scope_id = zoneid;

	ADDRSEL_LOCK();
	match = match_addrsel_policy(&sa6);
	if (match == NULL)
		match = &V_defaultaddrpolicy;
	else
		match->use++;
	ADDRSEL_UNLOCK();

	return (match->label);
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
	CTLFLAG_RD, in6_src_sysctl, "");

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

static VNET_DEFINE(struct addrsel_policyhead, addrsel_policytab);
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

/*
 * This function is similar to in6_addrscope, but has some difference,
 * specific for the source address selection algorithm (RFC 6724).
 */
static int
in6_srcaddrscope(const struct in6_addr *addr)
{

	/* 169.254/16 and 127/8 have link-local scope */
	if (IN6_IS_ADDR_V4MAPPED(addr)) {
		if (addr->s6_addr[12] == 127 || (
		    addr->s6_addr[12] == 169 && addr->s6_addr[13] == 254))
			return (IPV6_ADDR_SCOPE_LINKLOCAL);
	}
	return (in6_addrscope(addr));
}

/*
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
 * $FreeBSD: src/sys/netinet6/nd6.c,v 1.2 1999/12/07 17:39:15 shin Exp $
 */

/*
 * XXX
 * KAME 970409 note:
 * BSD/OS version heavily modifies this code, related to llinfo.
 * Since we don't have BSD/OS version of net/route.c in our hand,
 * I left the code mostly as it was in 970310.  -- itojun
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_atm.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/if_fddi.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_prefix.h>
#include <netinet6/icmp6.h>

#include "loop.h"

#include <net/net_osdep.h>

#define	ND6_SLOWTIMER_INTERVAL (60 * 60) /* 1 hour */
#define	ND6_RECALC_REACHTM_INTERVAL (60 * 120) /* 2 hours */

#define	SIN6(s) ((struct sockaddr_in6 *)s)
#define	SDL(s) ((struct sockaddr_dl *)s)

/* timer values */
int	nd6_prune	= 1;	/* walk list every 1 seconds */
int	nd6_delay	= 5;	/* delay first probe time 5 second */
int	nd6_umaxtries	= 3;	/* maximum unicast query */
int	nd6_mmaxtries	= 3;	/* maximum multicast query */
int	nd6_useloopback = 1;	/* use loopback interface for local traffic */
int	nd6_proxyall	= 0;	/* enable Proxy Neighbor Advertisement */

/* for debugging? */
static int	nd6_inuse, nd6_allocated;

struct	llinfo_nd6 llinfo_nd6 = {&llinfo_nd6, &llinfo_nd6};
struct	nd_ifinfo *nd_ifinfo = NULL;
struct	nd_drhead nd_defrouter = { 0 };
struct	nd_prhead nd_prefix = { 0 };

int	nd6_recalc_reachtm_interval = ND6_RECALC_REACHTM_INTERVAL;
static struct	sockaddr_in6 all1_sa;

static void	nd6_slowtimo __P((void *));

void
nd6_init()
{
	static int nd6_init_done = 0;
	int i;

	if (nd6_init_done) {
		log(LOG_NOTICE, "nd6_init called more than once(ignored)\n");
		return;
	}

	all1_sa.sin6_family = AF_INET6;
	all1_sa.sin6_len = sizeof(struct sockaddr_in6);
	for (i = 0; i < sizeof(all1_sa.sin6_addr); i++)
		all1_sa.sin6_addr.s6_addr[i] = 0xff;

	nd6_init_done = 1;

	/* start timer */
	timeout(nd6_slowtimo, (caddr_t)0, ND6_SLOWTIMER_INTERVAL * hz);
}

void
nd6_ifattach(ifp)
	struct ifnet *ifp;
{
	static size_t if_indexlim = 8;

	/*
	 * We have some arrays that should be indexed by if_index.
	 * since if_index will grow dynamically, they should grow too.
	 */
	if (nd_ifinfo == NULL || if_index >= if_indexlim) {
		size_t n;
		caddr_t q;

		while (if_index >= if_indexlim)
			if_indexlim <<= 1;

		/* grow nd_ifinfo */
		n = if_indexlim * sizeof(struct nd_ifinfo);
		q = (caddr_t)malloc(n, M_IP6NDP, M_WAITOK);
		bzero(q, n);
		if (nd_ifinfo) {
			bcopy((caddr_t)nd_ifinfo, q, n/2);
			free((caddr_t)nd_ifinfo, M_IP6NDP);
		}
		nd_ifinfo = (struct nd_ifinfo *)q;
	}

#define ND nd_ifinfo[ifp->if_index]
	ND.linkmtu = ifindex2ifnet[ifp->if_index]->if_mtu;
	ND.chlim = IPV6_DEFHLIM;
	ND.basereachable = REACHABLE_TIME;
	ND.reachable = ND_COMPUTE_RTIME(ND.basereachable);
	ND.retrans = RETRANS_TIMER;
	ND.receivedra = 0;
	nd6_setmtu(ifp);
#undef ND
}

/*
 * Reset ND level link MTU. This function is called when the physical MTU
 * changes, which means we might have to adjust the ND level MTU.
 */
void
nd6_setmtu(ifp)
	struct ifnet *ifp;
{
#define MIN(a,b) ((a) < (b) ? (a) : (b))
	struct nd_ifinfo *ndi = &nd_ifinfo[ifp->if_index];
	u_long oldmaxmtu = ndi->maxmtu;
	u_long oldlinkmtu = ndi->linkmtu;

	switch(ifp->if_type) {
	 case IFT_ARCNET:	/* XXX MTU handling needs more work */
		 ndi->maxmtu = MIN(60480, ifp->if_mtu);
		 break;
	 case IFT_ETHER:
		 ndi->maxmtu = MIN(ETHERMTU, ifp->if_mtu);
		 break;
	 case IFT_FDDI:
		 ndi->maxmtu = MIN(FDDIIPMTU, ifp->if_mtu);
		 break;
	 case IFT_ATM:
		 ndi->maxmtu = MIN(ATMMTU, ifp->if_mtu);
		 break;
	 default:
		 ndi->maxmtu = ifp->if_mtu;
		 break;
	}

	if (oldmaxmtu != ndi->maxmtu) {
		/*
		 * If the ND level MTU is not set yet, or if the maxmtu
		 * is reset to a smaller value than the ND level MTU,
		 * also reset the ND level MTU.
		 */
		if (ndi->linkmtu == 0 ||
		    ndi->maxmtu < ndi->linkmtu) {
			ndi->linkmtu = ndi->maxmtu;
			/* also adjust in6_maxmtu if necessary. */
			if (oldlinkmtu == 0) {
				/*
				 * XXX: the case analysis is grotty, but
				 * it is not efficient to call in6_setmaxmtu()
				 * here when we are during the initialization
				 * procedure.
				 */
				if (in6_maxmtu < ndi->linkmtu)
					in6_maxmtu = ndi->linkmtu;
			} else
				in6_setmaxmtu();
		}
	}
#undef MIN
}

void
nd6_option_init(opt, icmp6len, ndopts)
	void *opt;
	int icmp6len;
	union nd_opts *ndopts;
{
	bzero(ndopts, sizeof(*ndopts));
	ndopts->nd_opts_search = (struct nd_opt_hdr *)opt;
	ndopts->nd_opts_last
		= (struct nd_opt_hdr *)(((u_char *)opt) + icmp6len);

	if (icmp6len == 0) {
		ndopts->nd_opts_done = 1;
		ndopts->nd_opts_search = NULL;
	}
}

/*
 * Take one ND option.
 */
struct nd_opt_hdr *
nd6_option(ndopts)
	union nd_opts *ndopts;
{
	struct nd_opt_hdr *nd_opt;
	int olen;

	if (!ndopts)
		panic("ndopts == NULL in nd6_option\n");
	if (!ndopts->nd_opts_last)
		panic("uninitialized ndopts in nd6_option\n");
	if (!ndopts->nd_opts_search)
		return NULL;
	if (ndopts->nd_opts_done)
		return NULL;

	nd_opt = ndopts->nd_opts_search;

	olen = nd_opt->nd_opt_len << 3;
	if (olen == 0) {
		/*
		 * Message validation requires that all included
		 * options have a length that is greater than zero.
		 */
		bzero(ndopts, sizeof(*ndopts));
		return NULL;
	}

	ndopts->nd_opts_search = (struct nd_opt_hdr *)((caddr_t)nd_opt + olen);
	if (!(ndopts->nd_opts_search < ndopts->nd_opts_last)) {
		ndopts->nd_opts_done = 1;
		ndopts->nd_opts_search = NULL;
	}
	return nd_opt;
}

/*
 * Parse multiple ND options.
 * This function is much easier to use, for ND routines that do not need
 * multiple options of the same type.
 */
int
nd6_options(ndopts)
	union nd_opts *ndopts;
{
	struct nd_opt_hdr *nd_opt;
	int i = 0;

	if (!ndopts)
		panic("ndopts == NULL in nd6_options\n");
	if (!ndopts->nd_opts_last)
		panic("uninitialized ndopts in nd6_options\n");
	if (!ndopts->nd_opts_search)
		return 0;

	while (1) {
		nd_opt = nd6_option(ndopts);
		if (!nd_opt && !ndopts->nd_opts_last) {
			/*
			 * Message validation requires that all included
			 * options have a length that is greater than zero.
			 */
			bzero(ndopts, sizeof(*ndopts));
			return -1;
		}

		if (!nd_opt)
			goto skip1;

		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECTED_HEADER:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				printf("duplicated ND6 option found "
					"(type=%d)\n", nd_opt->nd_opt_type);
				/* XXX bark? */
			} else {
				ndopts->nd_opt_array[nd_opt->nd_opt_type]
					= nd_opt;
			}
			break;
		case ND_OPT_PREFIX_INFORMATION:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type] == 0) {
				ndopts->nd_opt_array[nd_opt->nd_opt_type]
					= nd_opt;
			}
			ndopts->nd_opts_pi_end =
				(struct nd_opt_prefix_info *)nd_opt;
			break;
		default:
			/*
			 * Unknown options must be silently ignored,
			 * to accomodate future extension to the protocol.
			 */
			log(LOG_INFO,
			    "nd6_options: unsupported option %d - "
			    "option ignored\n", nd_opt->nd_opt_type);
		}

skip1:
		i++;
		if (i > 10) {
			printf("too many loop in nd opt\n");
			break;
		}

		if (ndopts->nd_opts_done)
			break;
	}

	return 0;
}

/*
 * ND6 timer routine to expire default route list and prefix list
 */
void
nd6_timer(ignored_arg)
	void	*ignored_arg;
{
	int s;
	register struct llinfo_nd6 *ln;
	register struct nd_defrouter *dr;
	register struct nd_prefix *pr;
	
	s = splnet();
	timeout(nd6_timer, (caddr_t)0, nd6_prune * hz);

	ln = llinfo_nd6.ln_next;
	/* XXX BSD/OS separates this code -- itojun */
	while (ln && ln != &llinfo_nd6) {
		struct rtentry *rt;
		struct ifnet *ifp;
		struct sockaddr_in6 *dst;
		struct llinfo_nd6 *next = ln->ln_next;

		if ((rt = ln->ln_rt) == NULL) {
			ln = next;
			continue;
		}
		if ((ifp = rt->rt_ifp) == NULL) {
			ln = next;
			continue;
		}
		dst = (struct sockaddr_in6 *)rt_key(rt);

		if (ln->ln_expire > time_second) {
			ln = next;
			continue;
		}
		
		/* sanity check */
		if (!rt)
			panic("rt=0 in nd6_timer(ln=%p)\n", ln);
		if (!dst)
			panic("dst=0 in nd6_timer(ln=%p)\n", ln);

		switch (ln->ln_state) {
		case ND6_LLINFO_INCOMPLETE:
			if (ln->ln_asked < nd6_mmaxtries) {
				ln->ln_asked++;
				ln->ln_expire = time_second +
					nd_ifinfo[ifp->if_index].retrans / 1000;
				nd6_ns_output(ifp, NULL, &dst->sin6_addr,
					ln, 0);
			} else {
				struct mbuf *m = ln->ln_hold;
				if (m) {
					if (rt->rt_ifp) {
						/*
						 * Fake rcvif to make ICMP error
						 * more helpful in diagnosing
						 * for the receiver.
						 * XXX: should we consider
						 * older rcvif?
						 */
						m->m_pkthdr.rcvif = rt->rt_ifp;
					}
					icmp6_error(m, ICMP6_DST_UNREACH,
						    ICMP6_DST_UNREACH_ADDR, 0);
					ln->ln_hold = NULL;
				}
				nd6_free(rt);
			}
			break;
		case ND6_LLINFO_REACHABLE:
			if (ln->ln_expire) {
				ln->ln_state = ND6_LLINFO_STALE;
			}
			break;
		/*
		 * ND6_LLINFO_STALE state requires nothing for timer
		 * routine.
		 */
		case ND6_LLINFO_DELAY:
			ln->ln_asked = 1;
			ln->ln_state = ND6_LLINFO_PROBE;
			ln->ln_expire = time_second +
				nd_ifinfo[ifp->if_index].retrans / 1000;
			nd6_ns_output(ifp, &dst->sin6_addr, &dst->sin6_addr,
				ln, 0);
			break;

		case ND6_LLINFO_PROBE:
			if (ln->ln_asked < nd6_umaxtries) {
				ln->ln_asked++;
				ln->ln_expire = time_second +
					nd_ifinfo[ifp->if_index].retrans / 1000;
				nd6_ns_output(ifp, &dst->sin6_addr,
					       &dst->sin6_addr, ln, 0);
			} else {
				nd6_free(rt);
			}
			break;
		case ND6_LLINFO_WAITDELETE:
			nd6_free(rt);
			break;
		}
		ln = next;
	}
	
	/* expire */
	dr = LIST_FIRST(&nd_defrouter);
	while (dr) {
		if (dr->expire && dr->expire < time_second) {
			struct nd_defrouter *t;
			t = LIST_NEXT(dr, dr_entry);
			defrtrlist_del(dr);
			dr = t;
		} else
			dr = LIST_NEXT(dr, dr_entry);
	}
	pr = LIST_FIRST(&nd_prefix);
	while (pr) {
		struct in6_ifaddr *ia6;
		struct in6_addrlifetime *lt6;

		if (IN6_IS_ADDR_UNSPECIFIED(&pr->ndpr_addr))
			ia6 = NULL;
		else
			ia6 = in6ifa_ifpwithaddr(pr->ndpr_ifp, &pr->ndpr_addr);

		if (ia6) {
			/* check address lifetime */
			lt6 = &ia6->ia6_lifetime;
			if (lt6->ia6t_preferred && lt6->ia6t_preferred < time_second)
				ia6->ia6_flags |= IN6_IFF_DEPRECATED;
			if (lt6->ia6t_expire && lt6->ia6t_expire < time_second) {
				if (!IN6_IS_ADDR_UNSPECIFIED(&pr->ndpr_addr))
					in6_ifdel(pr->ndpr_ifp, &pr->ndpr_addr);
				/* xxx ND_OPT_PI_FLAG_ONLINK processing */
			}
		}

		/*
		 * check prefix lifetime.
		 * since pltime is just for autoconf, pltime processing for
		 * prefix is not necessary.
		 *
		 * we offset expire time by NDPR_KEEP_EXPIRE, so that we
		 * can use the old prefix information to validate the
		 * next prefix information to come.  See prelist_update()
		 * for actual validation.
		 */
		if (pr->ndpr_expire
		 && pr->ndpr_expire + NDPR_KEEP_EXPIRED < time_second) {
			struct nd_prefix *t;
			t = LIST_NEXT(pr, ndpr_entry);

			/*
			 * address expiration and prefix expiration are
			 * separate.  NEVER perform in6_ifdel here.
			 */

			prelist_remove(pr);
			pr = t;
		} else
			pr = LIST_NEXT(pr, ndpr_entry);
	}
	splx(s);
}

struct rtentry *
nd6_lookup(addr6, create, ifp)
	struct in6_addr *addr6;
	int create;
	struct ifnet *ifp;
{
	struct rtentry *rt;
	struct sockaddr_in6 sin6;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *addr6;
	rt = rtalloc1((struct sockaddr *)&sin6, create, 0UL);
	if (rt && (rt->rt_flags & RTF_LLINFO) == 0) {
		/*
		 * This is the case for the default route.
		 * If we want to create a neighbor cache for the address, we
		 * should free the route for the destination and allocate an
		 * interface route.
		 */
		if (create) {
			RTFREE(rt);
			rt = 0;
		}
	}
	if (!rt) {
		if (create && ifp) {
			/*
			 * If no route is available and create is set,
			 * we allocate a host route for the destination
			 * and treat it like an interface route.
			 * This hack is necessary for a neighbor which can't
			 * be covered by our own prefix.
			 */
			struct ifaddr *ifa =
				ifaof_ifpforaddr((struct sockaddr *)&sin6, ifp);
			if (ifa == NULL)
				return(NULL);

			/*
			 * Create a new route. RTF_LLINFO is necessary
			 * to create a Neighbor Cache entry for the
			 * destination in nd6_rtrequest which will be
			 * called in rtequest via ifa->ifa_rtrequest.
			 */
			if (rtrequest(RTM_ADD, (struct sockaddr *)&sin6,
				      ifa->ifa_addr,
				      (struct sockaddr *)&all1_sa,
				      (ifa->ifa_flags |
				       RTF_HOST | RTF_LLINFO) & ~RTF_CLONING,
				      &rt))
				log(LOG_ERR,
				    "nd6_lookup: failed to add route for a "
				    "neighbor(%s)\n", ip6_sprintf(addr6));
			if (rt == NULL)
				return(NULL);
			if (rt->rt_llinfo) {
				struct llinfo_nd6 *ln =
					(struct llinfo_nd6 *)rt->rt_llinfo;
				ln->ln_state = ND6_LLINFO_NOSTATE;
			}
		} else
			return(NULL);
	}
	rt->rt_refcnt--;
	/*
	 * Validation for the entry.
	 * XXX: we can't use rt->rt_ifp to check for the interface, since
	 *      it might be the loopback interface if the entry is for our
	 *      own address on a non-loopback interface. Instead, we should
	 *      use rt->rt_ifa->ifa_ifp, which would specify the REAL interface.
	 */
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK ||
	    (ifp && rt->rt_ifa->ifa_ifp != ifp)) {
		if (create) {
			log(LOG_DEBUG, "nd6_lookup: failed to lookup %s (if = %s)\n",
			    ip6_sprintf(addr6), ifp ? if_name(ifp) : "unspec");
			/* xxx more logs... kazu */
		}
		return(0);
	}
	return(rt);
}

/*
 * Detect if a given IPv6 address identifies a neighbor on a given link.
 * XXX: should take care of the destination of a p2p link?
 */
int
nd6_is_addr_neighbor(addr, ifp)
	struct in6_addr *addr;
	struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	int i;

#define IFADDR6(a) ((((struct in6_ifaddr *)(a))->ia_addr).sin6_addr)
#define IFMASK6(a) ((((struct in6_ifaddr *)(a))->ia_prefixmask).sin6_addr)

	/* A link-local address is always a neighbor. */
	if (IN6_IS_ADDR_LINKLOCAL(addr))
		return(1);

	/*
	 * If the address matches one of our addresses,
	 * it should be a neighbor.
	 */
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list)
	{
		if (ifa->ifa_addr->sa_family != AF_INET6)
			next: continue;

		for (i = 0; i < 4; i++) {
			if ((IFADDR6(ifa).s6_addr32[i] ^ addr->s6_addr32[i]) &
			    IFMASK6(ifa).s6_addr32[i])
				goto next;
		}
		return(1);
	}

	/*
	 * Even if the address matches none of our addresses, it might be
	 * in the neighbor cache.
	 */
	if (nd6_lookup(addr, 0, ifp))
		return(1);

	return(0);
#undef IFADDR6
#undef IFMASK6
}

/*
 * Free an nd6 llinfo entry.
 */
void
nd6_free(rt)
	struct rtentry *rt;
{
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	struct sockaddr_dl *sdl;

	if (ln->ln_router) {
		/* remove from default router list */
		struct nd_defrouter *dr;
		struct in6_addr *in6;
		int s;
		in6 = &((struct sockaddr_in6 *)rt_key(rt))->sin6_addr;

		s = splnet();
		dr = defrouter_lookup(&((struct sockaddr_in6 *)rt_key(rt))->
				      sin6_addr,
				      rt->rt_ifp);
		if (dr)
			defrtrlist_del(dr);
		else if (!ip6_forwarding && ip6_accept_rtadv) {
			/*
			 * rt6_flush must be called in any case.
			 * see the comment in nd6_na_input().
			 */
			rt6_flush(in6, rt->rt_ifp);
		}
		splx(s);
	}
	
	if (rt->rt_refcnt > 0 && (sdl = SDL(rt->rt_gateway)) &&
	   sdl->sdl_family == AF_LINK) {
		sdl->sdl_alen = 0;
		ln->ln_state = ND6_LLINFO_WAITDELETE;
		ln->ln_asked = 0;
		rt->rt_flags &= ~RTF_REJECT;
		return;
	}
	rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0, rt_mask(rt),
		  0, (struct rtentry **)0);
}

/*
 * Upper-layer reachability hint for Neighbor Unreachability Detection.
 *
 * XXX cost-effective metods?
 */
void
nd6_nud_hint(rt, dst6)
	struct rtentry *rt;
	struct in6_addr *dst6;
{
	struct llinfo_nd6 *ln;

	/*
	 * If the caller specified "rt", use that.  Otherwise, resolve the
	 * routing table by supplied "dst6".
	 */
	if (!rt) {
		if (!dst6)
			return;
		if (!(rt = nd6_lookup(dst6, 0, NULL)))
			return;
	}

	if ((rt->rt_flags & RTF_GATEWAY)
	 || (rt->rt_flags & RTF_LLINFO) == 0
	 || !rt->rt_llinfo
	 || !rt->rt_gateway
	 || rt->rt_gateway->sa_family != AF_LINK) {
		/* This is not a host route. */
		return;
	}

	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	if (ln->ln_state == ND6_LLINFO_INCOMPLETE)
		return;

	ln->ln_state = ND6_LLINFO_REACHABLE;
	if (ln->ln_expire)
		ln->ln_expire = time_second +
			nd_ifinfo[rt->rt_ifp->if_index].reachable;
}

void
nd6_rtrequest(req, rt, sa)
	int	req;
	struct rtentry *rt;
	struct sockaddr *sa; /* xxx unused */
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
	struct ifnet *ifp = rt->rt_ifp;
	struct ifaddr *ifa;

	if (rt->rt_flags & RTF_GATEWAY)
		return;

	switch (req) {
	case RTM_ADD:
		/*
		 * There is no backward compatibility :)
		 *
		 * if ((rt->rt_flags & RTF_HOST) == 0 &&
		 *     SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
		 *	   rt->rt_flags |= RTF_CLONING;
		 */
		if (rt->rt_flags & RTF_CLONING || rt->rt_flags & RTF_LLINFO) {
			/*
			 * Case 1: This route should come from
			 * a route to interface. RTF_LLINFO flag is set
			 * for a host route whose destination should be
			 * treated as on-link.
			 */
			rt_setgate(rt, rt_key(rt),
				   (struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = ifp->if_type;
			SDL(gate)->sdl_index = ifp->if_index;
			if (ln)
				ln->ln_expire = time_second;
			if (ln && ln->ln_expire == 0) {
				/* cludge for desktops */
				ln->ln_expire = 1;
			}
			if (rt->rt_flags & RTF_CLONING)
				break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			nd6_na_output(ifp,
				      &SIN6(rt_key(rt))->sin6_addr,
				      &SIN6(rt_key(rt))->sin6_addr,
				      ip6_forwarding ? ND_NA_FLAG_ROUTER : 0,
				      1);
		/* FALLTHROUGH */
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		   gate->sa_len < sizeof(null_sdl)) {
			log(LOG_DEBUG, "nd6_rtrequest: bad gateway value\n");
			break;
		}
		SDL(gate)->sdl_type = ifp->if_type;
		SDL(gate)->sdl_index = ifp->if_index;
		if (ln != 0)
			break;	/* This happens on a route change */
		/*
		 * Case 2: This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(ln, struct llinfo_nd6 *, sizeof(*ln));
		rt->rt_llinfo = (caddr_t)ln;
		if (!ln) {
			log(LOG_DEBUG, "nd6_rtrequest: malloc failed\n");
			break;
		}
		nd6_inuse++;
		nd6_allocated++;
		Bzero(ln, sizeof(*ln));
		ln->ln_rt = rt;
		/* this is required for "ndp" command. - shin */
		if (req == RTM_ADD) {
		        /*
			 * gate should have some valid AF_LINK entry,
			 * and ln->ln_expire should have some lifetime
			 * which is specified by ndp command.
			 */
			ln->ln_state = ND6_LLINFO_REACHABLE;
		} else {
		        /*
			 * When req == RTM_RESOLVE, rt is created and
			 * initialized in rtrequest(), so rt_expire is 0.
			 */
			ln->ln_state = ND6_LLINFO_INCOMPLETE;
			ln->ln_expire = time_second;
		}
		rt->rt_flags |= RTF_LLINFO;
		ln->ln_next = llinfo_nd6.ln_next;
		llinfo_nd6.ln_next = ln;
		ln->ln_prev = &llinfo_nd6;
		ln->ln_next->ln_prev = ln;

		/*
		 * check if rt_key(rt) is one of my address assigned
		 * to the interface.
		 */
		ifa = (struct ifaddr *)in6ifa_ifpwithaddr(rt->rt_ifp,
					  &SIN6(rt_key(rt))->sin6_addr);
		if (ifa) {
			caddr_t macp = nd6_ifptomac(ifp);
			ln->ln_expire = 0;
			ln->ln_state = ND6_LLINFO_REACHABLE;
			if (macp) {
				Bcopy(macp, LLADDR(SDL(gate)), ifp->if_addrlen);
				SDL(gate)->sdl_alen = ifp->if_addrlen;
			}
			if (nd6_useloopback) {
				rt->rt_ifp = &loif[0];	/*XXX*/
				/*
				 * Make sure rt_ifa be equal to the ifaddr
				 * corresponding to the address.
				 * We need this because when we refer
				 * rt_ifa->ia6_flags in ip6_input, we assume
				 * that the rt_ifa points to the address instead
				 * of the loopback address.
				 */
				if (ifa != rt->rt_ifa) {
					rt->rt_ifa->ifa_refcnt--;
					ifa->ifa_refcnt++;
					rt->rt_ifa = ifa;
				}
			}
		}
		break;

	case RTM_DELETE:
		if (!ln)
			break;
		nd6_inuse--;
		ln->ln_next->ln_prev = ln->ln_prev;
		ln->ln_prev->ln_next = ln->ln_next;
		ln->ln_prev = NULL;
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (ln->ln_hold)
			m_freem(ln->ln_hold);
		Free((caddr_t)ln);
	}
}

void
nd6_p2p_rtrequest(req, rt, sa)
	int	req;
	struct rtentry *rt;
	struct sockaddr *sa; /* xxx unused */
{
	struct sockaddr *gate = rt->rt_gateway;
	static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};
	struct ifnet *ifp = rt->rt_ifp;
	struct ifaddr *ifa;

	if (rt->rt_flags & RTF_GATEWAY)
		return;

	switch (req) {
	case RTM_ADD:
		/*
		 * There is no backward compatibility :)
		 *
		 * if ((rt->rt_flags & RTF_HOST) == 0 &&
		 *     SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
		 *	   rt->rt_flags |= RTF_CLONING;
		 */
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1: This route should come from
			 * a route to interface.
			 */
			rt_setgate(rt, rt_key(rt),
				   (struct sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = ifp->if_type;
			SDL(gate)->sdl_index = ifp->if_index;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			nd6_na_output(ifp,
				      &SIN6(rt_key(rt))->sin6_addr,
				      &SIN6(rt_key(rt))->sin6_addr,
				      ip6_forwarding ? ND_NA_FLAG_ROUTER : 0,
				      1);
		/* FALLTHROUGH */
	case RTM_RESOLVE:
		/*
		 * check if rt_key(rt) is one of my address assigned
		 * to the interface.
		 */
 		ifa = (struct ifaddr *)in6ifa_ifpwithaddr(rt->rt_ifp,
					  &SIN6(rt_key(rt))->sin6_addr);
		if (ifa) {
			if (nd6_useloopback) {
				rt->rt_ifp = &loif[0];	/*XXX*/
			}
		}
		break;
	}
}

int
nd6_ioctl(cmd, data, ifp)
	u_long cmd;
	caddr_t	data;
	struct ifnet *ifp;
{
	struct in6_drlist *drl = (struct in6_drlist *)data;
	struct in6_prlist *prl = (struct in6_prlist *)data;
	struct in6_ndireq *ndi = (struct in6_ndireq *)data;
	struct in6_nbrinfo *nbi = (struct in6_nbrinfo *)data;
	struct nd_defrouter *dr, any;
	struct nd_prefix *pr;
	struct rtentry *rt;
	int i = 0, error = 0;
	int s;

	switch (cmd) {
	case SIOCGDRLST_IN6:
		bzero(drl, sizeof(*drl));
		s = splnet();
		dr = LIST_FIRST(&nd_defrouter);
		while (dr && i < DRLSTSIZ) {
			drl->defrouter[i].rtaddr = dr->rtaddr;
			if (IN6_IS_ADDR_LINKLOCAL(&drl->defrouter[i].rtaddr)) {
				/* XXX: need to this hack for KAME stack */
				drl->defrouter[i].rtaddr.s6_addr16[1] = 0;
			} else
				log(LOG_ERR,
				    "default router list contains a "
				    "non-linklocal address(%s)\n",
				    ip6_sprintf(&drl->defrouter[i].rtaddr));

			drl->defrouter[i].flags = dr->flags;
			drl->defrouter[i].rtlifetime = dr->rtlifetime;
			drl->defrouter[i].expire = dr->expire;
			drl->defrouter[i].if_index = dr->ifp->if_index;
			i++;
			dr = LIST_NEXT(dr, dr_entry);
		}
		splx(s);
		break;
	case SIOCGPRLST_IN6:
		bzero(prl, sizeof(*prl));
		s = splnet();
		pr = LIST_FIRST(&nd_prefix);
		while (pr && i < PRLSTSIZ) {
			struct nd_pfxrouter *pfr;
			int j;

			prl->prefix[i].prefix = pr->ndpr_prefix.sin6_addr;
			prl->prefix[i].raflags = pr->ndpr_raf;
			prl->prefix[i].prefixlen = pr->ndpr_plen;
			prl->prefix[i].vltime = pr->ndpr_vltime;
			prl->prefix[i].pltime = pr->ndpr_pltime;
			prl->prefix[i].if_index = pr->ndpr_ifp->if_index;
			prl->prefix[i].expire = pr->ndpr_expire;

			pfr = LIST_FIRST(&pr->ndpr_advrtrs);
			j = 0;
			while(pfr) {
				if (j < DRLSTSIZ) {
#define RTRADDR prl->prefix[i].advrtr[j]
					RTRADDR = pfr->router->rtaddr;
					if (IN6_IS_ADDR_LINKLOCAL(&RTRADDR)) {
						/* XXX: hack for KAME */
						RTRADDR.s6_addr16[1] = 0;
					} else
						log(LOG_ERR,
						    "a router(%s) advertises "
						    "a prefix with "
						    "non-link local address\n",
						    ip6_sprintf(&RTRADDR));
#undef RTRADDR
				}
				j++;
				pfr = LIST_NEXT(pfr, pfr_entry);
			}
			prl->prefix[i].advrtrs = j;

			i++;
			pr = LIST_NEXT(pr, ndpr_entry);
		}
		splx(s);
	      {
		struct rr_prefix *rpp;

		for (rpp = LIST_FIRST(&rr_prefix); rpp;
		     rpp = LIST_NEXT(rpp, rp_entry)) {
			if (i >= PRLSTSIZ)
				break;
			prl->prefix[i].prefix = rpp->rp_prefix.sin6_addr;
			prl->prefix[i].raflags = rpp->rp_raf;
			prl->prefix[i].prefixlen = rpp->rp_plen;
			prl->prefix[i].vltime = rpp->rp_vltime;
			prl->prefix[i].pltime = rpp->rp_pltime;
			prl->prefix[i].if_index = rpp->rp_ifp->if_index;
			prl->prefix[i].expire = rpp->rp_expire;
			prl->prefix[i].advrtrs = 0;
			i++;
		}
	      }

		break;
	case SIOCGIFINFO_IN6:
		ndi->ndi = nd_ifinfo[ifp->if_index];
		break;
	case SIOCSNDFLUSH_IN6:
		/* flush default router list */
		/*
		 * xxx sumikawa: should not delete route if default
		 * route equals to the top of default router list
		 */
		bzero(&any, sizeof(any));
		defrouter_delreq(&any, 0);
		/* xxx sumikawa: flush prefix list */
		break;
	case SIOCSPFXFLUSH_IN6:
	    {
		/* flush all the prefix advertised by routers */
		struct nd_prefix *pr, *next;

		s = splnet();
		for (pr = LIST_FIRST(&nd_prefix); pr; pr = next) {
			next = LIST_NEXT(pr, ndpr_entry);
			if (!IN6_IS_ADDR_UNSPECIFIED(&pr->ndpr_addr))
				in6_ifdel(pr->ndpr_ifp, &pr->ndpr_addr);
			prelist_remove(pr);
		}
		splx(s);
		break;
	    }
	case SIOCSRTRFLUSH_IN6:
	    {
		/* flush all the default routers */
		struct nd_defrouter *dr, *next;

		s = splnet();
		if ((dr = LIST_FIRST(&nd_defrouter)) != NULL) {
			/*
			 * The first entry of the list may be stored in
			 * the routing table, so we'll delete it later.
			 */
			for (dr = LIST_NEXT(dr, dr_entry); dr; dr = next) {
				next = LIST_NEXT(dr, dr_entry);
				defrtrlist_del(dr);
			}
			defrtrlist_del(LIST_FIRST(&nd_defrouter));
		}
		splx(s);
		break;
	    }
	case SIOCGNBRINFO_IN6:
	    {
		struct llinfo_nd6 *ln;
		struct in6_addr nb_addr = nbi->addr; /* make local for safety */

		/*
		 * XXX: KAME specific hack for scoped addresses
		 *      XXXX: for other scopes than link-local?
		 */
		if (IN6_IS_ADDR_LINKLOCAL(&nbi->addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&nbi->addr)) {
			u_int16_t *idp = (u_int16_t *)&nb_addr.s6_addr[2];

			if (*idp == 0)
				*idp = htons(ifp->if_index);
		}

		s = splnet();
		if ((rt = nd6_lookup(&nb_addr, 0, ifp)) == NULL) {
			error = EINVAL;
			break;
		}
		ln = (struct llinfo_nd6 *)rt->rt_llinfo;
		nbi->state = ln->ln_state;
		nbi->asked = ln->ln_asked;
		nbi->isrouter = ln->ln_router;
		nbi->expire = ln->ln_expire;
		splx(s);
		
		break;
	    }
	}
	return(error);
}

/*
 * Create neighbor cache entry and cache link-layer address,
 * on reception of inbound ND6 packets. (RS/RA/NS/redirect)
 */
struct rtentry *
nd6_cache_lladdr(ifp, from, lladdr, lladdrlen, type, code)
	struct ifnet *ifp;
	struct in6_addr *from;
	char *lladdr;
	int lladdrlen;
	int type;	/* ICMP6 type */
	int code;	/* type dependent information */
{
	struct rtentry *rt = NULL;
	struct llinfo_nd6 *ln = NULL;
	int is_newentry;
	struct sockaddr_dl *sdl = NULL;
	int do_update;
	int olladdr;
	int llchange;
	int newstate = 0;

	if (!ifp)
		panic("ifp == NULL in nd6_cache_lladdr");
	if (!from)
		panic("from == NULL in nd6_cache_lladdr");

	/* nothing must be updated for unspecified address */
	if (IN6_IS_ADDR_UNSPECIFIED(from))
		return NULL;

	/*
	 * Validation about ifp->if_addrlen and lladdrlen must be done in
	 * the caller.
	 *
	 * XXX If the link does not have link-layer adderss, what should
	 * we do? (ifp->if_addrlen == 0)
	 * Spec says nothing in sections for RA, RS and NA.  There's small
	 * description on it in NS section (RFC 2461 7.2.3).
	 */

	rt = nd6_lookup(from, 0, ifp);
	if (!rt) {
		rt = nd6_lookup(from, 1, ifp);
		is_newentry = 1;
	} else
		is_newentry = 0;

	if (!rt)
		return NULL;
	if ((rt->rt_flags & (RTF_GATEWAY | RTF_LLINFO)) != RTF_LLINFO) {
fail:
		nd6_free(rt);
		return NULL;
	}
	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	if (!ln)
		goto fail;
	if (!rt->rt_gateway)
		goto fail;
	if (rt->rt_gateway->sa_family != AF_LINK)
		goto fail;
	sdl = SDL(rt->rt_gateway);

	olladdr = (sdl->sdl_alen) ? 1 : 0;
	if (olladdr && lladdr) {
		if (bcmp(lladdr, LLADDR(sdl), ifp->if_addrlen))
			llchange = 1;
		else
			llchange = 0;
	} else
		llchange = 0;

	/*
	 * newentry olladdr  lladdr  llchange	(*=record)
	 *	0	n	n	--	(1)
	 *	0	y	n	--	(2)
	 *	0	n	y	--	(3) * STALE
	 *	0	y	y	n	(4) *
	 *	0	y	y	y	(5) * STALE
	 *	1	--	n	--	(6)   NOSTATE(= PASSIVE)
	 *	1	--	y	--	(7) * STALE
	 */

	if (lladdr) {		/*(3-5) and (7)*/
		/*
		 * Record source link-layer address
		 * XXX is it dependent to ifp->if_type?
		 */
		sdl->sdl_alen = ifp->if_addrlen;
		bcopy(lladdr, LLADDR(sdl), ifp->if_addrlen);
	}

	if (!is_newentry) {
		if ((!olladdr && lladdr)		/*(3)*/
		 || (olladdr && lladdr && llchange)) {	/*(5)*/
			do_update = 1;
			newstate = ND6_LLINFO_STALE;
		} else					/*(1-2,4)*/
			do_update = 0;
	} else {
		do_update = 1;
		if (!lladdr)				/*(6)*/
			newstate = ND6_LLINFO_NOSTATE;
		else					/*(7)*/
			newstate = ND6_LLINFO_STALE;
	}

	if (do_update) {
		/*
		 * Update the state of the neighbor cache.
		 */
		ln->ln_state = newstate;

		if (ln->ln_state == ND6_LLINFO_STALE) {
			rt->rt_flags &= ~RTF_REJECT;
			if (ln->ln_hold) {
				nd6_output(ifp, ln->ln_hold,
					   (struct sockaddr_in6 *)rt_key(rt),
					   rt);
				ln->ln_hold = 0;
			}
		} else if (ln->ln_state == ND6_LLINFO_INCOMPLETE) {
			/* probe right away */
			ln->ln_expire = time_second;
		}
	}

	/*
	 * ICMP6 type dependent behavior.
	 *
	 * NS: clear IsRouter if new entry
	 * RS: clear IsRouter
	 * RA: set IsRouter if there's lladdr
	 * redir: clear IsRouter if new entry
	 *
	 * RA case, (1):
	 * The spec says that we must set IsRouter in the following cases:
	 * - If lladdr exist, set IsRouter.  This means (1-5).
	 * - If it is old entry (!newentry), set IsRouter.  This means (7).
	 * So, based on the spec, in (1-5) and (7) cases we must set IsRouter.
	 * A quetion arises for (1) case.  (1) case has no lladdr in the
	 * neighbor cache, this is similar to (6).
	 * This case is rare but we figured that we MUST NOT set IsRouter.
	 *
	 * newentry olladdr  lladdr  llchange	    NS  RS  RA	redir
	 *							D R
	 *	0	n	n	--	(1)	c   ?     s
	 *	0	y	n	--	(2)	c   s     s
	 *	0	n	y	--	(3)	c   s     s
	 *	0	y	y	n	(4)	c   s     s
	 *	0	y	y	y	(5)	c   s     s
	 *	1	--	n	--	(6) c	c 	c s
	 *	1	--	y	--	(7) c	c   s	c s
	 *
	 *					(c=clear s=set)
	 */
	switch (type & 0xff) {
	case ND_NEIGHBOR_SOLICIT:
		/*
		 * New entry must have is_router flag cleared.
		 */
		if (is_newentry)	/*(6-7)*/
			ln->ln_router = 0;
		break;
	case ND_REDIRECT:
		/*
		 * If the icmp is a redirect to a better router, always set the
		 * is_router flag. Otherwise, if the entry is newly created,
		 * clear the flag. [RFC 2461, sec 8.3]
		 *
		 */
		if (code == ND_REDIRECT_ROUTER)
			ln->ln_router = 1;
		else if (is_newentry) /*(6-7)*/
			ln->ln_router = 0;
		break;
	case ND_ROUTER_SOLICIT:
		/*
		 * is_router flag must always be cleared.
		 */
		ln->ln_router = 0;
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Mark an entry with lladdr as a router.
		 */
		if ((!is_newentry && (olladdr || lladdr))	/*(2-5)*/
		 || (is_newentry && lladdr)) {			/*(7)*/
			ln->ln_router = 1;
		}
		break;
	}

	return rt;
}

static void
nd6_slowtimo(ignored_arg)
    void *ignored_arg;
{
	int s = splnet();
	register int i;
	register struct nd_ifinfo *nd6if;

	timeout(nd6_slowtimo, (caddr_t)0, ND6_SLOWTIMER_INTERVAL * hz);
	for (i = 1; i < if_index + 1; i++) {
		nd6if = &nd_ifinfo[i];
		if (nd6if->basereachable && /* already initialized */
		    (nd6if->recalctm -= ND6_SLOWTIMER_INTERVAL) <= 0) {
			/*
			 * Since reachable time rarely changes by router
			 * advertisements, we SHOULD insure that a new random
			 * value gets recomputed at least once every few hours.
			 * (RFC 2461, 6.3.4)
			 */
			nd6if->recalctm = nd6_recalc_reachtm_interval;
			nd6if->reachable = ND_COMPUTE_RTIME(nd6if->basereachable);
		}
	}
	splx(s);
}

#define senderr(e) { error = (e); goto bad;}
int
nd6_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr_in6 *dst;
	struct rtentry *rt0;
{
	register struct mbuf *m = m0;
	register struct rtentry *rt = rt0;
	struct llinfo_nd6 *ln = NULL;
	int error = 0;

	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
		goto sendpkt;

	/*
	 * XXX: we currently do not make neighbor cache on any interface
	 * other than ARCnet, Ethernet and FDDI.
	 */
	switch (ifp->if_type) {
	case IFT_ARCNET:
	case IFT_ETHER:
	case IFT_FDDI:
		break;
	default:
		goto sendpkt;
	}

	/*
	 * next hop determination. This routine is derived from ether_outpout.
	 */
	if (rt) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc1((struct sockaddr *)dst, 1, 0UL)) !=
				NULL)
			{
				rt->rt_refcnt--;
				if (rt->rt_ifp != ifp)
					return nd6_output(ifp, m0, dst, rt); /* XXX: loop care? */
			} else
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1, 0UL);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

	/*
	 * Address resolution or Neighbor Unreachability Detection
	 * for the next hop.
	 * At this point, the destination of the packet must be a unicast
	 * or an anycast address(i.e. not a multicast).
	 */

	/* Look up the neighbor cache for the nexthop */
	if (rt && (rt->rt_flags & RTF_LLINFO) != 0)
		ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	else {
		if ((rt = nd6_lookup(&dst->sin6_addr, 1, ifp)) != NULL)
			ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	}
	if (!ln || !rt) {
		log(LOG_DEBUG, "nd6_output: can't allocate llinfo for %s "
		    "(ln=%p, rt=%p)\n",
		    ip6_sprintf(&dst->sin6_addr), ln, rt);
		senderr(EIO);	/* XXX: good error? */
	}


	/*
	 * The first time we send a packet to a neighbor whose entry is
	 * STALE, we have to change the state to DELAY and a sets a timer to
	 * expire in DELAY_FIRST_PROBE_TIME seconds to ensure do
	 * neighbor unreachability detection on expiration.
	 * (RFC 2461 7.3.3)
	 */
	if (ln->ln_state == ND6_LLINFO_STALE) {
		ln->ln_asked = 0;
		ln->ln_state = ND6_LLINFO_DELAY;
		ln->ln_expire = time_second + nd6_delay;
	}

	/*
	 * If the neighbor cache entry has a state other than INCOMPLETE
	 * (i.e. its link-layer address is already reloved), just
	 * send the packet.
	 */
	if (ln->ln_state > ND6_LLINFO_INCOMPLETE)
		goto sendpkt;

	/*
	 * There is a neighbor cache entry, but no ethernet address
	 * response yet. Replace the held mbuf (if any) with this
	 * latest one.
	 *
	 * XXX Does the code conform to rate-limiting rule?
	 * (RFC 2461 7.2.2)
	 */
	if (ln->ln_state == ND6_LLINFO_WAITDELETE ||
	    ln->ln_state == ND6_LLINFO_NOSTATE)
		ln->ln_state = ND6_LLINFO_INCOMPLETE;
	if (ln->ln_hold)
		m_freem(ln->ln_hold);
	ln->ln_hold = m;
	if (ln->ln_expire) {
		rt->rt_flags &= ~RTF_REJECT;
		if (ln->ln_asked < nd6_mmaxtries &&
		    ln->ln_expire < time_second) {
			ln->ln_asked++;
			ln->ln_expire = time_second +
				nd_ifinfo[ifp->if_index].retrans / 1000;
			nd6_ns_output(ifp, NULL, &dst->sin6_addr, ln, 0);
		}
	}
	return(0);
	
  sendpkt:
	return((*ifp->if_output)(ifp, m, (struct sockaddr *)dst, rt));
	
  bad:
	if (m)
		m_freem(m);
	return (error);
}	
#undef senderr

int
nd6_storelladdr(ifp, rt, m, dst, desten)
	struct ifnet *ifp;
	struct rtentry *rt;
	struct mbuf *m;
	struct sockaddr *dst;
	u_char *desten;
{
	struct sockaddr_dl *sdl;

	if (m->m_flags & M_MCAST) {
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_FDDI:			
			ETHER_MAP_IPV6_MULTICAST(&SIN6(dst)->sin6_addr,
						 desten);
			return(1);
			break;
		case IFT_ARCNET:
			*desten = 0;
			return(1);
		default:
			return(0);
		}
	}

	if (rt == NULL ||
	    rt->rt_gateway->sa_family != AF_LINK) {
		printf("nd6_storelladdr: something odd happens\n");
		return(0);
	}
	sdl = SDL(rt->rt_gateway);
	if (sdl->sdl_alen != 0)
		bcopy(LLADDR(sdl), desten, sdl->sdl_alen);

	return(1);
}

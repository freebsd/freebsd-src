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
 *	$KAME: nd6.c,v 1.144 2001/05/24 07:44:00 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/iso88025.h>
#include <net/fddi.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <net/if_llatbl.h>
#include <netinet/if_ether.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet/icmp6.h>
#include <netinet6/send.h>

#include <sys/limits.h>

#include <security/mac/mac_framework.h>

#define ND6_SLOWTIMER_INTERVAL (60 * 60) /* 1 hour */
#define ND6_RECALC_REACHTM_INTERVAL (60 * 120) /* 2 hours */

#define SIN6(s) ((const struct sockaddr_in6 *)(s))

/* timer values */
VNET_DEFINE(int, nd6_prune)	= 1;	/* walk list every 1 seconds */
VNET_DEFINE(int, nd6_delay)	= 5;	/* delay first probe time 5 second */
VNET_DEFINE(int, nd6_umaxtries)	= 3;	/* maximum unicast query */
VNET_DEFINE(int, nd6_mmaxtries)	= 3;	/* maximum multicast query */
VNET_DEFINE(int, nd6_useloopback) = 1;	/* use loopback interface for
					 * local traffic */
VNET_DEFINE(int, nd6_gctimer)	= (60 * 60 * 24); /* 1 day: garbage
					 * collection timer */

/* preventing too many loops in ND option parsing */
static VNET_DEFINE(int, nd6_maxndopt) = 10; /* max # of ND options allowed */

VNET_DEFINE(int, nd6_maxnudhint) = 0;	/* max # of subsequent upper
					 * layer hints */
static VNET_DEFINE(int, nd6_maxqueuelen) = 1; /* max pkts cached in unresolved
					 * ND entries */
#define	V_nd6_maxndopt			VNET(nd6_maxndopt)
#define	V_nd6_maxqueuelen		VNET(nd6_maxqueuelen)

#ifdef ND6_DEBUG
VNET_DEFINE(int, nd6_debug) = 1;
#else
VNET_DEFINE(int, nd6_debug) = 0;
#endif

static eventhandler_tag lle_event_eh;

/* for debugging? */
#if 0
static int nd6_inuse, nd6_allocated;
#endif

VNET_DEFINE(struct nd_drhead, nd_defrouter);
VNET_DEFINE(struct nd_prhead, nd_prefix);

VNET_DEFINE(int, nd6_recalc_reachtm_interval) = ND6_RECALC_REACHTM_INTERVAL;
#define	V_nd6_recalc_reachtm_interval	VNET(nd6_recalc_reachtm_interval)

int	(*send_sendso_input_hook)(struct mbuf *, struct ifnet *, int, int);

static int nd6_is_new_addr_neighbor(const struct sockaddr_in6 *,
	struct ifnet *);
static void nd6_setmtu0(struct ifnet *, struct nd_ifinfo *);
static void nd6_slowtimo(void *);
static int regen_tmpaddr(struct in6_ifaddr *);
static void nd6_free(struct llentry *, int);
static void nd6_free_redirect(const struct llentry *);
static void nd6_llinfo_timer(void *);
static void clear_llinfo_pqueue(struct llentry *);
static void nd6_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
static int nd6_resolve_slow(struct ifnet *, struct mbuf *,
    const struct sockaddr_in6 *, u_char *, uint32_t *);
static int nd6_need_cache(struct ifnet *);
 

static VNET_DEFINE(struct callout, nd6_slowtimo_ch);
#define	V_nd6_slowtimo_ch		VNET(nd6_slowtimo_ch)

VNET_DEFINE(struct callout, nd6_timer_ch);

static void
nd6_lle_event(void *arg __unused, struct llentry *lle, int evt)
{
	struct rt_addrinfo rtinfo;
	struct sockaddr_in6 dst;
	struct sockaddr_dl gw;
	struct ifnet *ifp;
	int type;

	LLE_WLOCK_ASSERT(lle);

	if (lltable_get_af(lle->lle_tbl) != AF_INET6)
		return;

	switch (evt) {
	case LLENTRY_RESOLVED:
		type = RTM_ADD;
		KASSERT(lle->la_flags & LLE_VALID,
		    ("%s: %p resolved but not valid?", __func__, lle));
		break;
	case LLENTRY_EXPIRED:
		type = RTM_DELETE;
		break;
	default:
		return;
	}

	ifp = lltable_get_ifp(lle->lle_tbl);

	bzero(&dst, sizeof(dst));
	bzero(&gw, sizeof(gw));
	bzero(&rtinfo, sizeof(rtinfo));
	lltable_fill_sa_entry(lle, (struct sockaddr *)&dst);
	dst.sin6_scope_id = in6_getscopezone(ifp,
	    in6_addrscope(&dst.sin6_addr));
	gw.sdl_len = sizeof(struct sockaddr_dl);
	gw.sdl_family = AF_LINK;
	gw.sdl_alen = ifp->if_addrlen;
	gw.sdl_index = ifp->if_index;
	gw.sdl_type = ifp->if_type;
	if (evt == LLENTRY_RESOLVED)
		bcopy(&lle->ll_addr, gw.sdl_data, ifp->if_addrlen);
	rtinfo.rti_info[RTAX_DST] = (struct sockaddr *)&dst;
	rtinfo.rti_info[RTAX_GATEWAY] = (struct sockaddr *)&gw;
	rtinfo.rti_addrs = RTA_DST | RTA_GATEWAY;
	rt_missmsg_fib(type, &rtinfo, RTF_HOST | RTF_LLDATA | (
	    type == RTM_ADD ? RTF_UP: 0), 0, RT_DEFAULT_FIB);
}

void
nd6_init(void)
{

	LIST_INIT(&V_nd_prefix);

	/* initialization of the default router list */
	TAILQ_INIT(&V_nd_defrouter);

	/* start timer */
	callout_init(&V_nd6_slowtimo_ch, 0);
	callout_reset(&V_nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL * hz,
	    nd6_slowtimo, curvnet);

	nd6_dad_init();
	if (IS_DEFAULT_VNET(curvnet))
		lle_event_eh = EVENTHANDLER_REGISTER(lle_event, nd6_lle_event,
		    NULL, EVENTHANDLER_PRI_ANY);
}

#ifdef VIMAGE
void
nd6_destroy()
{

	callout_drain(&V_nd6_slowtimo_ch);
	callout_drain(&V_nd6_timer_ch);
	if (IS_DEFAULT_VNET(curvnet))
		EVENTHANDLER_DEREGISTER(lle_event, lle_event_eh);
}
#endif

struct nd_ifinfo *
nd6_ifattach(struct ifnet *ifp)
{
	struct nd_ifinfo *nd;

	nd = (struct nd_ifinfo *)malloc(sizeof(*nd), M_IP6NDP, M_WAITOK|M_ZERO);
	nd->initialized = 1;

	nd->chlim = IPV6_DEFHLIM;
	nd->basereachable = REACHABLE_TIME;
	nd->reachable = ND_COMPUTE_RTIME(nd->basereachable);
	nd->retrans = RETRANS_TIMER;

	nd->flags = ND6_IFF_PERFORMNUD;

	/* A loopback interface always has ND6_IFF_AUTO_LINKLOCAL.
	 * XXXHRS: Clear ND6_IFF_AUTO_LINKLOCAL on an IFT_BRIDGE interface by
	 * default regardless of the V_ip6_auto_linklocal configuration to
	 * give a reasonable default behavior.
	 */
	if ((V_ip6_auto_linklocal && ifp->if_type != IFT_BRIDGE) ||
	    (ifp->if_flags & IFF_LOOPBACK))
		nd->flags |= ND6_IFF_AUTO_LINKLOCAL;
	/*
	 * A loopback interface does not need to accept RTADV.
	 * XXXHRS: Clear ND6_IFF_ACCEPT_RTADV on an IFT_BRIDGE interface by
	 * default regardless of the V_ip6_accept_rtadv configuration to
	 * prevent the interface from accepting RA messages arrived
	 * on one of the member interfaces with ND6_IFF_ACCEPT_RTADV.
	 */
	if (V_ip6_accept_rtadv &&
	    !(ifp->if_flags & IFF_LOOPBACK) &&
	    (ifp->if_type != IFT_BRIDGE))
			nd->flags |= ND6_IFF_ACCEPT_RTADV;
	if (V_ip6_no_radr && !(ifp->if_flags & IFF_LOOPBACK))
		nd->flags |= ND6_IFF_NO_RADR;

	/* XXX: we cannot call nd6_setmtu since ifp is not fully initialized */
	nd6_setmtu0(ifp, nd);

	return nd;
}

void
nd6_ifdetach(struct nd_ifinfo *nd)
{

	free(nd, M_IP6NDP);
}

/*
 * Reset ND level link MTU. This function is called when the physical MTU
 * changes, which means we might have to adjust the ND level MTU.
 */
void
nd6_setmtu(struct ifnet *ifp)
{

	nd6_setmtu0(ifp, ND_IFINFO(ifp));
}

/* XXX todo: do not maintain copy of ifp->if_mtu in ndi->maxmtu */
void
nd6_setmtu0(struct ifnet *ifp, struct nd_ifinfo *ndi)
{
	u_int32_t omaxmtu;

	omaxmtu = ndi->maxmtu;

	switch (ifp->if_type) {
	case IFT_ARCNET:
		ndi->maxmtu = MIN(ARC_PHDS_MAXMTU, ifp->if_mtu); /* RFC2497 */
		break;
	case IFT_FDDI:
		ndi->maxmtu = MIN(FDDIIPMTU, ifp->if_mtu); /* RFC2467 */
		break;
	case IFT_ISO88025:
		 ndi->maxmtu = MIN(ISO88025_MAX_MTU, ifp->if_mtu);
		 break;
	default:
		ndi->maxmtu = ifp->if_mtu;
		break;
	}

	/*
	 * Decreasing the interface MTU under IPV6 minimum MTU may cause
	 * undesirable situation.  We thus notify the operator of the change
	 * explicitly.  The check for omaxmtu is necessary to restrict the
	 * log to the case of changing the MTU, not initializing it.
	 */
	if (omaxmtu >= IPV6_MMTU && ndi->maxmtu < IPV6_MMTU) {
		log(LOG_NOTICE, "nd6_setmtu0: "
		    "new link MTU on %s (%lu) is too small for IPv6\n",
		    if_name(ifp), (unsigned long)ndi->maxmtu);
	}

	if (ndi->maxmtu > V_in6_maxmtu)
		in6_setmaxmtu(); /* check all interfaces just in case */

}

void
nd6_option_init(void *opt, int icmp6len, union nd_opts *ndopts)
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
nd6_option(union nd_opts *ndopts)
{
	struct nd_opt_hdr *nd_opt;
	int olen;

	KASSERT(ndopts != NULL, ("%s: ndopts == NULL", __func__));
	KASSERT(ndopts->nd_opts_last != NULL, ("%s: uninitialized ndopts",
	    __func__));
	if (ndopts->nd_opts_search == NULL)
		return NULL;
	if (ndopts->nd_opts_done)
		return NULL;

	nd_opt = ndopts->nd_opts_search;

	/* make sure nd_opt_len is inside the buffer */
	if ((caddr_t)&nd_opt->nd_opt_len >= (caddr_t)ndopts->nd_opts_last) {
		bzero(ndopts, sizeof(*ndopts));
		return NULL;
	}

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
	if (ndopts->nd_opts_search > ndopts->nd_opts_last) {
		/* option overruns the end of buffer, invalid */
		bzero(ndopts, sizeof(*ndopts));
		return NULL;
	} else if (ndopts->nd_opts_search == ndopts->nd_opts_last) {
		/* reached the end of options chain */
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
nd6_options(union nd_opts *ndopts)
{
	struct nd_opt_hdr *nd_opt;
	int i = 0;

	KASSERT(ndopts != NULL, ("%s: ndopts == NULL", __func__));
	KASSERT(ndopts->nd_opts_last != NULL, ("%s: uninitialized ndopts",
	    __func__));
	if (ndopts->nd_opts_search == NULL)
		return 0;

	while (1) {
		nd_opt = nd6_option(ndopts);
		if (nd_opt == NULL && ndopts->nd_opts_last == NULL) {
			/*
			 * Message validation requires that all included
			 * options have a length that is greater than zero.
			 */
			ICMP6STAT_INC(icp6s_nd_badopt);
			bzero(ndopts, sizeof(*ndopts));
			return -1;
		}

		if (nd_opt == NULL)
			goto skip1;

		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECTED_HEADER:
		case ND_OPT_NONCE:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				nd6log((LOG_INFO,
				    "duplicated ND6 option found (type=%d)\n",
				    nd_opt->nd_opt_type));
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
		/* What about ND_OPT_ROUTE_INFO? RFC 4191 */
		case ND_OPT_RDNSS:	/* RFC 6106 */
		case ND_OPT_DNSSL:	/* RFC 6106 */
			/*
			 * Silently ignore options we know and do not care about
			 * in the kernel.
			 */
			break;
		default:
			/*
			 * Unknown options must be silently ignored,
			 * to accomodate future extension to the protocol.
			 */
			nd6log((LOG_DEBUG,
			    "nd6_options: unsupported option %d - "
			    "option ignored\n", nd_opt->nd_opt_type));
		}

skip1:
		i++;
		if (i > V_nd6_maxndopt) {
			ICMP6STAT_INC(icp6s_nd_toomanyopt);
			nd6log((LOG_INFO, "too many loop in nd opt\n"));
			break;
		}

		if (ndopts->nd_opts_done)
			break;
	}

	return 0;
}

/*
 * ND6 timer routine to handle ND6 entries
 */
void
nd6_llinfo_settimer_locked(struct llentry *ln, long tick)
{
	int canceled;

	LLE_WLOCK_ASSERT(ln);

	if (tick < 0) {
		ln->la_expire = 0;
		ln->ln_ntick = 0;
		canceled = callout_stop(&ln->lle_timer);
	} else {
		ln->la_expire = time_uptime + tick / hz;
		LLE_ADDREF(ln);
		if (tick > INT_MAX) {
			ln->ln_ntick = tick - INT_MAX;
			canceled = callout_reset(&ln->lle_timer, INT_MAX,
			    nd6_llinfo_timer, ln);
		} else {
			ln->ln_ntick = 0;
			canceled = callout_reset(&ln->lle_timer, tick,
			    nd6_llinfo_timer, ln);
		}
	}
	if (canceled)
		LLE_REMREF(ln);
}

/*
* Gets source address of the first packet in hold queue
* and stores it in @src.
* Returns pointer to @src (if hold queue is not empty) or NULL.
*
*/
static struct in6_addr *
nd6_llinfo_get_holdsrc(struct llentry *ln, struct in6_addr *src)
{
	struct ip6_hdr hdr;
	struct mbuf *m;

	if (ln->la_hold == NULL)
		return (NULL);

	/*
	 * assume every packet in la_hold has the same IP header
	 */
	m = ln->la_hold;
	if (sizeof(hdr) < m->m_len)
		return (NULL);

	m_copydata(m, 0, sizeof(hdr), (caddr_t)&hdr);
	*src = hdr.ip6_src;

	return (src);
}

void
nd6_llinfo_settimer(struct llentry *ln, long tick)
{

	LLE_WLOCK(ln);
	nd6_llinfo_settimer_locked(ln, tick);
	LLE_WUNLOCK(ln);
}

static void
nd6_llinfo_timer(void *arg)
{
	struct llentry *ln;
	struct in6_addr *dst, *pdst, *psrc, src;
	struct ifnet *ifp;
	struct nd_ifinfo *ndi = NULL;
	int send_ns;

	KASSERT(arg != NULL, ("%s: arg NULL", __func__));
	ln = (struct llentry *)arg;
	LLE_WLOCK(ln);
	if (callout_pending(&ln->lle_timer)) {
		/*
		 * Here we are a bit odd here in the treatment of 
		 * active/pending. If the pending bit is set, it got
		 * rescheduled before I ran. The active
		 * bit we ignore, since if it was stopped
		 * in ll_tablefree() and was currently running
		 * it would have return 0 so the code would
		 * not have deleted it since the callout could
		 * not be stopped so we want to go through
		 * with the delete here now. If the callout
		 * was restarted, the pending bit will be back on and
		 * we just want to bail since the callout_reset would
		 * return 1 and our reference would have been removed
		 * by nd6_llinfo_settimer_locked above since canceled
		 * would have been 1.
		 */
		LLE_WUNLOCK(ln);
		return;
	}
	ifp = ln->lle_tbl->llt_ifp;
	CURVNET_SET(ifp->if_vnet);
	ndi = ND_IFINFO(ifp);
	send_ns = 0;
	dst = &ln->r_l3addr.addr6;
	pdst = dst;

	if (ln->ln_ntick > 0) {
		if (ln->ln_ntick > INT_MAX) {
			ln->ln_ntick -= INT_MAX;
			nd6_llinfo_settimer_locked(ln, INT_MAX);
		} else {
			ln->ln_ntick = 0;
			nd6_llinfo_settimer_locked(ln, ln->ln_ntick);
		}
		goto done;
	}

	if (ln->la_flags & LLE_STATIC) {
		goto done;
	}

	if (ln->la_flags & LLE_DELETED) {
		nd6_free(ln, 0);
		ln = NULL;
		goto done;
	}

	switch (ln->ln_state) {
	case ND6_LLINFO_INCOMPLETE:
		if (ln->la_asked < V_nd6_mmaxtries) {
			ln->la_asked++;
			send_ns = 1;
			/* Send NS to multicast address */
			pdst = NULL;
		} else {
			struct mbuf *m = ln->la_hold;
			if (m) {
				struct mbuf *m0;

				/*
				 * assuming every packet in la_hold has the
				 * same IP header.  Send error after unlock.
				 */
				m0 = m->m_nextpkt;
				m->m_nextpkt = NULL;
				ln->la_hold = m0;
				clear_llinfo_pqueue(ln);
			}
			EVENTHANDLER_INVOKE(lle_event, ln, LLENTRY_TIMEDOUT);
			nd6_free(ln, 0);
			ln = NULL;
			if (m != NULL)
				icmp6_error2(m, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0, ifp);
		}
		break;
	case ND6_LLINFO_REACHABLE:
		if (!ND6_LLINFO_PERMANENT(ln)) {
			ln->ln_state = ND6_LLINFO_STALE;
			nd6_llinfo_settimer_locked(ln, (long)V_nd6_gctimer * hz);
		}
		break;

	case ND6_LLINFO_STALE:
		/* Garbage Collection(RFC 2461 5.3) */
		if (!ND6_LLINFO_PERMANENT(ln)) {
			EVENTHANDLER_INVOKE(lle_event, ln, LLENTRY_EXPIRED);
			nd6_free(ln, 1);
			ln = NULL;
		}
		break;

	case ND6_LLINFO_DELAY:
		if (ndi && (ndi->flags & ND6_IFF_PERFORMNUD) != 0) {
			/* We need NUD */
			ln->la_asked = 1;
			ln->ln_state = ND6_LLINFO_PROBE;
			send_ns = 1;
		} else {
			ln->ln_state = ND6_LLINFO_STALE; /* XXX */
			nd6_llinfo_settimer_locked(ln, (long)V_nd6_gctimer * hz);
		}
		break;
	case ND6_LLINFO_PROBE:
		if (ln->la_asked < V_nd6_umaxtries) {
			ln->la_asked++;
			send_ns = 1;
		} else {
			EVENTHANDLER_INVOKE(lle_event, ln, LLENTRY_EXPIRED);
			nd6_free(ln, 0);
			ln = NULL;
		}
		break;
	default:
		panic("%s: paths in a dark night can be confusing: %d",
		    __func__, ln->ln_state);
	}
done:
	if (send_ns != 0) {
		nd6_llinfo_settimer_locked(ln, (long)ndi->retrans * hz / 1000);
		psrc = nd6_llinfo_get_holdsrc(ln, &src);
		LLE_FREE_LOCKED(ln);
		ln = NULL;
		nd6_ns_output(ifp, psrc, pdst, dst, NULL);
	}

	if (ln != NULL)
		LLE_FREE_LOCKED(ln);
	CURVNET_RESTORE();
}


/*
 * ND6 timer routine to expire default route list and prefix list
 */
void
nd6_timer(void *arg)
{
	CURVNET_SET((struct vnet *) arg);
	struct nd_defrouter *dr, *ndr;
	struct nd_prefix *pr, *npr;
	struct in6_ifaddr *ia6, *nia6;

	callout_reset(&V_nd6_timer_ch, V_nd6_prune * hz,
	    nd6_timer, curvnet);

	/* expire default router list */
	TAILQ_FOREACH_SAFE(dr, &V_nd_defrouter, dr_entry, ndr) {
		if (dr->expire && dr->expire < time_uptime)
			defrtrlist_del(dr);
	}

	/*
	 * expire interface addresses.
	 * in the past the loop was inside prefix expiry processing.
	 * However, from a stricter speci-confrmance standpoint, we should
	 * rather separate address lifetimes and prefix lifetimes.
	 *
	 * XXXRW: in6_ifaddrhead locking.
	 */
  addrloop:
	TAILQ_FOREACH_SAFE(ia6, &V_in6_ifaddrhead, ia_link, nia6) {
		/* check address lifetime */
		if (IFA6_IS_INVALID(ia6)) {
			int regen = 0;

			/*
			 * If the expiring address is temporary, try
			 * regenerating a new one.  This would be useful when
			 * we suspended a laptop PC, then turned it on after a
			 * period that could invalidate all temporary
			 * addresses.  Although we may have to restart the
			 * loop (see below), it must be after purging the
			 * address.  Otherwise, we'd see an infinite loop of
			 * regeneration.
			 */
			if (V_ip6_use_tempaddr &&
			    (ia6->ia6_flags & IN6_IFF_TEMPORARY) != 0) {
				if (regen_tmpaddr(ia6) == 0)
					regen = 1;
			}

			in6_purgeaddr(&ia6->ia_ifa);

			if (regen)
				goto addrloop; /* XXX: see below */
		} else if (IFA6_IS_DEPRECATED(ia6)) {
			int oldflags = ia6->ia6_flags;

			ia6->ia6_flags |= IN6_IFF_DEPRECATED;

			/*
			 * If a temporary address has just become deprecated,
			 * regenerate a new one if possible.
			 */
			if (V_ip6_use_tempaddr &&
			    (ia6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
			    (oldflags & IN6_IFF_DEPRECATED) == 0) {

				if (regen_tmpaddr(ia6) == 0) {
					/*
					 * A new temporary address is
					 * generated.
					 * XXX: this means the address chain
					 * has changed while we are still in
					 * the loop.  Although the change
					 * would not cause disaster (because
					 * it's not a deletion, but an
					 * addition,) we'd rather restart the
					 * loop just for safety.  Or does this
					 * significantly reduce performance??
					 */
					goto addrloop;
				}
			}
		} else {
			/*
			 * A new RA might have made a deprecated address
			 * preferred.
			 */
			ia6->ia6_flags &= ~IN6_IFF_DEPRECATED;
		}
	}

	/* expire prefix list */
	LIST_FOREACH_SAFE(pr, &V_nd_prefix, ndpr_entry, npr) {
		/*
		 * check prefix lifetime.
		 * since pltime is just for autoconf, pltime processing for
		 * prefix is not necessary.
		 */
		if (pr->ndpr_vltime != ND6_INFINITE_LIFETIME &&
		    time_uptime - pr->ndpr_lastupdate > pr->ndpr_vltime) {

			/*
			 * address expiration and prefix expiration are
			 * separate.  NEVER perform in6_purgeaddr here.
			 */
			prelist_remove(pr);
		}
	}
	CURVNET_RESTORE();
}

/*
 * ia6 - deprecated/invalidated temporary address
 */
static int
regen_tmpaddr(struct in6_ifaddr *ia6)
{
	struct ifaddr *ifa;
	struct ifnet *ifp;
	struct in6_ifaddr *public_ifa6 = NULL;

	ifp = ia6->ia_ifa.ifa_ifp;
	IF_ADDR_RLOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in6_ifaddr *it6;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		it6 = (struct in6_ifaddr *)ifa;

		/* ignore no autoconf addresses. */
		if ((it6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
			continue;

		/* ignore autoconf addresses with different prefixes. */
		if (it6->ia6_ndpr == NULL || it6->ia6_ndpr != ia6->ia6_ndpr)
			continue;

		/*
		 * Now we are looking at an autoconf address with the same
		 * prefix as ours.  If the address is temporary and is still
		 * preferred, do not create another one.  It would be rare, but
		 * could happen, for example, when we resume a laptop PC after
		 * a long period.
		 */
		if ((it6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
		    !IFA6_IS_DEPRECATED(it6)) {
			public_ifa6 = NULL;
			break;
		}

		/*
		 * This is a public autoconf address that has the same prefix
		 * as ours.  If it is preferred, keep it.  We can't break the
		 * loop here, because there may be a still-preferred temporary
		 * address with the prefix.
		 */
		if (!IFA6_IS_DEPRECATED(it6))
			public_ifa6 = it6;
	}
	if (public_ifa6 != NULL)
		ifa_ref(&public_ifa6->ia_ifa);
	IF_ADDR_RUNLOCK(ifp);

	if (public_ifa6 != NULL) {
		int e;

		if ((e = in6_tmpifadd(public_ifa6, 0, 0)) != 0) {
			ifa_free(&public_ifa6->ia_ifa);
			log(LOG_NOTICE, "regen_tmpaddr: failed to create a new"
			    " tmp addr,errno=%d\n", e);
			return (-1);
		}
		ifa_free(&public_ifa6->ia_ifa);
		return (0);
	}

	return (-1);
}

/*
 * Nuke neighbor cache/prefix/default router management table, right before
 * ifp goes away.
 */
void
nd6_purge(struct ifnet *ifp)
{
	struct nd_defrouter *dr, *ndr;
	struct nd_prefix *pr, *npr;

	/*
	 * Nuke default router list entries toward ifp.
	 * We defer removal of default router list entries that is installed
	 * in the routing table, in order to keep additional side effects as
	 * small as possible.
	 */
	TAILQ_FOREACH_SAFE(dr, &V_nd_defrouter, dr_entry, ndr) {
		if (dr->installed)
			continue;

		if (dr->ifp == ifp)
			defrtrlist_del(dr);
	}

	TAILQ_FOREACH_SAFE(dr, &V_nd_defrouter, dr_entry, ndr) {
		if (!dr->installed)
			continue;

		if (dr->ifp == ifp)
			defrtrlist_del(dr);
	}

	/* Nuke prefix list entries toward ifp */
	LIST_FOREACH_SAFE(pr, &V_nd_prefix, ndpr_entry, npr) {
		if (pr->ndpr_ifp == ifp) {
			/*
			 * Because if_detach() does *not* release prefixes
			 * while purging addresses the reference count will
			 * still be above zero. We therefore reset it to
			 * make sure that the prefix really gets purged.
			 */
			pr->ndpr_refcnt = 0;

			/*
			 * Previously, pr->ndpr_addr is removed as well,
			 * but I strongly believe we don't have to do it.
			 * nd6_purge() is only called from in6_ifdetach(),
			 * which removes all the associated interface addresses
			 * by itself.
			 * (jinmei@kame.net 20010129)
			 */
			prelist_remove(pr);
		}
	}

	/* cancel default outgoing interface setting */
	if (V_nd6_defifindex == ifp->if_index)
		nd6_setdefaultiface(0);

	if (ND_IFINFO(ifp)->flags & ND6_IFF_ACCEPT_RTADV) {
		/* Refresh default router list. */
		defrouter_select();
	}

	/* XXXXX
	 * We do not nuke the neighbor cache entries here any more
	 * because the neighbor cache is kept in if_afdata[AF_INET6].
	 * nd6_purge() is invoked by in6_ifdetach() which is called
	 * from if_detach() where everything gets purged. So let
	 * in6_domifdetach() do the actual L2 table purging work.
	 */
}

/* 
 * the caller acquires and releases the lock on the lltbls
 * Returns the llentry locked
 */
struct llentry *
nd6_lookup(const struct in6_addr *addr6, int flags, struct ifnet *ifp)
{
	struct sockaddr_in6 sin6;
	struct llentry *ln;
	int llflags;
	
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *addr6;

	IF_AFDATA_LOCK_ASSERT(ifp);

	llflags = (flags & ND6_EXCLUSIVE) ? LLE_EXCLUSIVE : 0;
	ln = lla_lookup(LLTABLE6(ifp), llflags, (struct sockaddr *)&sin6);

	return (ln);
}

struct llentry *
nd6_alloc(const struct in6_addr *addr6, int flags, struct ifnet *ifp)
{
	struct sockaddr_in6 sin6;
	struct llentry *ln;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *addr6;

	ln = lltable_alloc_entry(LLTABLE6(ifp), 0, (struct sockaddr *)&sin6);
	if (ln != NULL)
		ln->ln_state = ND6_LLINFO_NOSTATE;

	return (ln);
}

/*
 * Test whether a given IPv6 address is a neighbor or not, ignoring
 * the actual neighbor cache.  The neighbor cache is ignored in order
 * to not reenter the routing code from within itself.
 */
static int
nd6_is_new_addr_neighbor(const struct sockaddr_in6 *addr, struct ifnet *ifp)
{
	struct nd_prefix *pr;
	struct ifaddr *dstaddr;

	/*
	 * A link-local address is always a neighbor.
	 * XXX: a link does not necessarily specify a single interface.
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr)) {
		struct sockaddr_in6 sin6_copy;
		u_int32_t zone;

		/*
		 * We need sin6_copy since sa6_recoverscope() may modify the
		 * content (XXX).
		 */
		sin6_copy = *addr;
		if (sa6_recoverscope(&sin6_copy))
			return (0); /* XXX: should be impossible */
		if (in6_setscope(&sin6_copy.sin6_addr, ifp, &zone))
			return (0);
		if (sin6_copy.sin6_scope_id == zone)
			return (1);
		else
			return (0);
	}

	/*
	 * If the address matches one of our addresses,
	 * it should be a neighbor.
	 * If the address matches one of our on-link prefixes, it should be a
	 * neighbor.
	 */
	LIST_FOREACH(pr, &V_nd_prefix, ndpr_entry) {
		if (pr->ndpr_ifp != ifp)
			continue;

		if (!(pr->ndpr_stateflags & NDPRF_ONLINK)) {
			struct rtentry *rt;

			/* Always use the default FIB here. */
			rt = in6_rtalloc1((struct sockaddr *)&pr->ndpr_prefix,
			    0, 0, RT_DEFAULT_FIB);
			if (rt == NULL)
				continue;
			/*
			 * This is the case where multiple interfaces
			 * have the same prefix, but only one is installed 
			 * into the routing table and that prefix entry
			 * is not the one being examined here. In the case
			 * where RADIX_MPATH is enabled, multiple route
			 * entries (of the same rt_key value) will be 
			 * installed because the interface addresses all
			 * differ.
			 */
			if (!IN6_ARE_ADDR_EQUAL(&pr->ndpr_prefix.sin6_addr,
			       &((struct sockaddr_in6 *)rt_key(rt))->sin6_addr)) {
				RTFREE_LOCKED(rt);
				continue;
			}
			RTFREE_LOCKED(rt);
		}

		if (IN6_ARE_MASKED_ADDR_EQUAL(&pr->ndpr_prefix.sin6_addr,
		    &addr->sin6_addr, &pr->ndpr_mask))
			return (1);
	}

	/*
	 * If the address is assigned on the node of the other side of
	 * a p2p interface, the address should be a neighbor.
	 */
	dstaddr = ifa_ifwithdstaddr((const struct sockaddr *)addr, RT_ALL_FIBS);
	if (dstaddr != NULL) {
		if (dstaddr->ifa_ifp == ifp) {
			ifa_free(dstaddr);
			return (1);
		}
		ifa_free(dstaddr);
	}

	/*
	 * If the default router list is empty, all addresses are regarded
	 * as on-link, and thus, as a neighbor.
	 */
	if (ND_IFINFO(ifp)->flags & ND6_IFF_ACCEPT_RTADV &&
	    TAILQ_EMPTY(&V_nd_defrouter) &&
	    V_nd6_defifindex == ifp->if_index) {
		return (1);
	}

	return (0);
}


/*
 * Detect if a given IPv6 address identifies a neighbor on a given link.
 * XXX: should take care of the destination of a p2p link?
 */
int
nd6_is_addr_neighbor(const struct sockaddr_in6 *addr, struct ifnet *ifp)
{
	struct llentry *lle;
	int rc = 0;

	IF_AFDATA_UNLOCK_ASSERT(ifp);
	if (nd6_is_new_addr_neighbor(addr, ifp))
		return (1);

	/*
	 * Even if the address matches none of our addresses, it might be
	 * in the neighbor cache.
	 */
	IF_AFDATA_RLOCK(ifp);
	if ((lle = nd6_lookup(&addr->sin6_addr, 0, ifp)) != NULL) {
		LLE_RUNLOCK(lle);
		rc = 1;
	}
	IF_AFDATA_RUNLOCK(ifp);
	return (rc);
}

/*
 * Free an nd6 llinfo entry.
 * Since the function would cause significant changes in the kernel, DO NOT
 * make it global, unless you have a strong reason for the change, and are sure
 * that the change is safe.
 */
static void
nd6_free(struct llentry *ln, int gc)
{
	struct nd_defrouter *dr;
	struct ifnet *ifp;

	LLE_WLOCK_ASSERT(ln);

	/*
	 * we used to have pfctlinput(PRC_HOSTDEAD) here.
	 * even though it is not harmful, it was not really necessary.
	 */

	/* cancel timer */
	nd6_llinfo_settimer_locked(ln, -1);

	ifp = ln->lle_tbl->llt_ifp;

	if (ND_IFINFO(ifp)->flags & ND6_IFF_ACCEPT_RTADV) {
		dr = defrouter_lookup(&ln->r_l3addr.addr6, ifp);

		if (dr != NULL && dr->expire &&
		    ln->ln_state == ND6_LLINFO_STALE && gc) {
			/*
			 * If the reason for the deletion is just garbage
			 * collection, and the neighbor is an active default
			 * router, do not delete it.  Instead, reset the GC
			 * timer using the router's lifetime.
			 * Simply deleting the entry would affect default
			 * router selection, which is not necessarily a good
			 * thing, especially when we're using router preference
			 * values.
			 * XXX: the check for ln_state would be redundant,
			 *      but we intentionally keep it just in case.
			 */
			if (dr->expire > time_uptime)
				nd6_llinfo_settimer_locked(ln,
				    (dr->expire - time_uptime) * hz);
			else
				nd6_llinfo_settimer_locked(ln,
				    (long)V_nd6_gctimer * hz);

			LLE_REMREF(ln);
			LLE_WUNLOCK(ln);
			return;
		}

		if (dr) {
			/*
			 * Unreachablity of a router might affect the default
			 * router selection and on-link detection of advertised
			 * prefixes.
			 */

			/*
			 * Temporarily fake the state to choose a new default
			 * router and to perform on-link determination of
			 * prefixes correctly.
			 * Below the state will be set correctly,
			 * or the entry itself will be deleted.
			 */
			ln->ln_state = ND6_LLINFO_INCOMPLETE;
		}

		if (ln->ln_router || dr) {

			/*
			 * We need to unlock to avoid a LOR with rt6_flush() with the
			 * rnh and for the calls to pfxlist_onlink_check() and
			 * defrouter_select() in the block further down for calls
			 * into nd6_lookup().  We still hold a ref.
			 */
			LLE_WUNLOCK(ln);

			/*
			 * rt6_flush must be called whether or not the neighbor
			 * is in the Default Router List.
			 * See a corresponding comment in nd6_na_input().
			 */
			rt6_flush(&ln->r_l3addr.addr6, ifp);
		}

		if (dr) {
			/*
			 * Since defrouter_select() does not affect the
			 * on-link determination and MIP6 needs the check
			 * before the default router selection, we perform
			 * the check now.
			 */
			pfxlist_onlink_check();

			/*
			 * Refresh default router list.
			 */
			defrouter_select();
		}

		/*
		 * If this entry was added by an on-link redirect, remove the
		 * corresponding host route.
		 */
		if (ln->la_flags & LLE_REDIRECT)
			nd6_free_redirect(ln);

		if (ln->ln_router || dr)
			LLE_WLOCK(ln);
	}

	/*
	 * Save to unlock. We still hold an extra reference and will not
	 * free(9) in llentry_free() if someone else holds one as well.
	 */
	LLE_WUNLOCK(ln);
	IF_AFDATA_LOCK(ifp);
	LLE_WLOCK(ln);
	/* Guard against race with other llentry_free(). */
	if (ln->la_flags & LLE_LINKED) {
		/* Remove callout reference */
		LLE_REMREF(ln);
		lltable_unlink_entry(ln->lle_tbl, ln);
	}
	IF_AFDATA_UNLOCK(ifp);

	llentry_free(ln);
}

/*
 * Remove the rtentry for the given llentry,
 * both of which were installed by a redirect.
 */
static void
nd6_free_redirect(const struct llentry *ln)
{
	int fibnum;
	struct rtentry *rt;
	struct radix_node_head *rnh;
	struct sockaddr_in6 sin6;

	lltable_fill_sa_entry(ln, (struct sockaddr *)&sin6);
	for (fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		rnh = rt_tables_get_rnh(fibnum, AF_INET6);
		if (rnh == NULL)
			continue;

		RADIX_NODE_HEAD_LOCK(rnh);
		rt = in6_rtalloc1((struct sockaddr *)&sin6, 0,
		    RTF_RNH_LOCKED, fibnum);
		if (rt) {
			if (rt->rt_flags == (RTF_UP | RTF_HOST | RTF_DYNAMIC))
				rt_expunge(rnh, rt);
			RTFREE_LOCKED(rt);
		}
		RADIX_NODE_HEAD_UNLOCK(rnh);
	}
}

/*
 * Upper-layer reachability hint for Neighbor Unreachability Detection.
 *
 * XXX cost-effective methods?
 */
void
nd6_nud_hint(struct rtentry *rt, struct in6_addr *dst6, int force)
{
	struct llentry *ln;
	struct ifnet *ifp;

	if ((dst6 == NULL) || (rt == NULL))
		return;

	ifp = rt->rt_ifp;
	IF_AFDATA_RLOCK(ifp);
	ln = nd6_lookup(dst6, ND6_EXCLUSIVE, NULL);
	IF_AFDATA_RUNLOCK(ifp);
	if (ln == NULL)
		return;

	if (ln->ln_state < ND6_LLINFO_REACHABLE)
		goto done;

	/*
	 * if we get upper-layer reachability confirmation many times,
	 * it is possible we have false information.
	 */
	if (!force) {
		ln->ln_byhint++;
		if (ln->ln_byhint > V_nd6_maxnudhint) {
			goto done;
		}
	}

 	ln->ln_state = ND6_LLINFO_REACHABLE;
	if (!ND6_LLINFO_PERMANENT(ln)) {
		nd6_llinfo_settimer_locked(ln,
		    (long)ND_IFINFO(rt->rt_ifp)->reachable * hz);
	}
done:
	LLE_WUNLOCK(ln);
}


/*
 * Rejuvenate this function for routing operations related
 * processing.
 */
void
nd6_rtrequest(int req, struct rtentry *rt, struct rt_addrinfo *info)
{
	struct sockaddr_in6 *gateway;
	struct nd_defrouter *dr;
	struct ifnet *ifp;

	gateway = (struct sockaddr_in6 *)rt->rt_gateway;
	ifp = rt->rt_ifp;

	switch (req) {
	case RTM_ADD:
		break;

	case RTM_DELETE:
		if (!ifp)
			return;
		/*
		 * Only indirect routes are interesting.
		 */
		if ((rt->rt_flags & RTF_GATEWAY) == 0)
			return;
		/*
		 * check for default route
		 */
		if (IN6_ARE_ADDR_EQUAL(&in6addr_any, 
				       &SIN6(rt_key(rt))->sin6_addr)) {

			dr = defrouter_lookup(&gateway->sin6_addr, ifp);
			if (dr != NULL)
				dr->installed = 0;
		}
		break;
	}
}


int
nd6_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp)
{
	struct in6_ndireq *ndi = (struct in6_ndireq *)data;
	struct in6_nbrinfo *nbi = (struct in6_nbrinfo *)data;
	struct in6_ndifreq *ndif = (struct in6_ndifreq *)data;
	int error = 0;

	if (ifp->if_afdata[AF_INET6] == NULL)
		return (EPFNOSUPPORT);
	switch (cmd) {
	case OSIOCGIFINFO_IN6:
#define ND	ndi->ndi
		/* XXX: old ndp(8) assumes a positive value for linkmtu. */
		bzero(&ND, sizeof(ND));
		ND.linkmtu = IN6_LINKMTU(ifp);
		ND.maxmtu = ND_IFINFO(ifp)->maxmtu;
		ND.basereachable = ND_IFINFO(ifp)->basereachable;
		ND.reachable = ND_IFINFO(ifp)->reachable;
		ND.retrans = ND_IFINFO(ifp)->retrans;
		ND.flags = ND_IFINFO(ifp)->flags;
		ND.recalctm = ND_IFINFO(ifp)->recalctm;
		ND.chlim = ND_IFINFO(ifp)->chlim;
		break;
	case SIOCGIFINFO_IN6:
		ND = *ND_IFINFO(ifp);
		break;
	case SIOCSIFINFO_IN6:
		/*
		 * used to change host variables from userland.
		 * intented for a use on router to reflect RA configurations.
		 */
		/* 0 means 'unspecified' */
		if (ND.linkmtu != 0) {
			if (ND.linkmtu < IPV6_MMTU ||
			    ND.linkmtu > IN6_LINKMTU(ifp)) {
				error = EINVAL;
				break;
			}
			ND_IFINFO(ifp)->linkmtu = ND.linkmtu;
		}

		if (ND.basereachable != 0) {
			int obasereachable = ND_IFINFO(ifp)->basereachable;

			ND_IFINFO(ifp)->basereachable = ND.basereachable;
			if (ND.basereachable != obasereachable)
				ND_IFINFO(ifp)->reachable =
				    ND_COMPUTE_RTIME(ND.basereachable);
		}
		if (ND.retrans != 0)
			ND_IFINFO(ifp)->retrans = ND.retrans;
		if (ND.chlim != 0)
			ND_IFINFO(ifp)->chlim = ND.chlim;
		/* FALLTHROUGH */
	case SIOCSIFINFO_FLAGS:
	{
		struct ifaddr *ifa;
		struct in6_ifaddr *ia;

		if ((ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) &&
		    !(ND.flags & ND6_IFF_IFDISABLED)) {
			/* ifdisabled 1->0 transision */

			/*
			 * If the interface is marked as ND6_IFF_IFDISABLED and
			 * has an link-local address with IN6_IFF_DUPLICATED,
			 * do not clear ND6_IFF_IFDISABLED.
			 * See RFC 4862, Section 5.4.5.
			 */
			IF_ADDR_RLOCK(ifp);
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				if (ifa->ifa_addr->sa_family != AF_INET6)
					continue;
				ia = (struct in6_ifaddr *)ifa;
				if ((ia->ia6_flags & IN6_IFF_DUPLICATED) &&
				    IN6_IS_ADDR_LINKLOCAL(IA6_IN6(ia)))
					break;
			}
			IF_ADDR_RUNLOCK(ifp);

			if (ifa != NULL) {
				/* LLA is duplicated. */
				ND.flags |= ND6_IFF_IFDISABLED;
				log(LOG_ERR, "Cannot enable an interface"
				    " with a link-local address marked"
				    " duplicate.\n");
			} else {
				ND_IFINFO(ifp)->flags &= ~ND6_IFF_IFDISABLED;
				if (ifp->if_flags & IFF_UP)
					in6_if_up(ifp);
			}
		} else if (!(ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) &&
			    (ND.flags & ND6_IFF_IFDISABLED)) {
			/* ifdisabled 0->1 transision */
			/* Mark all IPv6 address as tentative. */

			ND_IFINFO(ifp)->flags |= ND6_IFF_IFDISABLED;
			if ((ND_IFINFO(ifp)->flags & ND6_IFF_NO_DAD) == 0) {
				IF_ADDR_RLOCK(ifp);
				TAILQ_FOREACH(ifa, &ifp->if_addrhead,
				    ifa_link) {
					if (ifa->ifa_addr->sa_family !=
					    AF_INET6)
						continue;
					ia = (struct in6_ifaddr *)ifa;
					ia->ia6_flags |= IN6_IFF_TENTATIVE;
				}
				IF_ADDR_RUNLOCK(ifp);
			}
		}

		if (ND.flags & ND6_IFF_AUTO_LINKLOCAL) {
			if (!(ND_IFINFO(ifp)->flags & ND6_IFF_AUTO_LINKLOCAL)) {
				/* auto_linklocal 0->1 transision */

				/* If no link-local address on ifp, configure */
				ND_IFINFO(ifp)->flags |= ND6_IFF_AUTO_LINKLOCAL;
				in6_ifattach(ifp, NULL);
			} else if (!(ND.flags & ND6_IFF_IFDISABLED) &&
			    ifp->if_flags & IFF_UP) {
				/*
				 * When the IF already has
				 * ND6_IFF_AUTO_LINKLOCAL, no link-local
				 * address is assigned, and IFF_UP, try to
				 * assign one.
				 */
				IF_ADDR_RLOCK(ifp);
				TAILQ_FOREACH(ifa, &ifp->if_addrhead,
				    ifa_link) {
					if (ifa->ifa_addr->sa_family !=
					    AF_INET6)
						continue;
					ia = (struct in6_ifaddr *)ifa;
					if (IN6_IS_ADDR_LINKLOCAL(IA6_IN6(ia)))
						break;
				}
				IF_ADDR_RUNLOCK(ifp);
				if (ifa != NULL)
					/* No LLA is configured. */
					in6_ifattach(ifp, NULL);
			}
		}
	}
		ND_IFINFO(ifp)->flags = ND.flags;
		break;
#undef ND
	case SIOCSNDFLUSH_IN6:	/* XXX: the ioctl name is confusing... */
		/* sync kernel routing table with the default router list */
		defrouter_reset();
		defrouter_select();
		break;
	case SIOCSPFXFLUSH_IN6:
	{
		/* flush all the prefix advertised by routers */
		struct nd_prefix *pr, *next;

		LIST_FOREACH_SAFE(pr, &V_nd_prefix, ndpr_entry, next) {
			struct in6_ifaddr *ia, *ia_next;

			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue; /* XXX */

			/* do we really have to remove addresses as well? */
			/* XXXRW: in6_ifaddrhead locking. */
			TAILQ_FOREACH_SAFE(ia, &V_in6_ifaddrhead, ia_link,
			    ia_next) {
				if ((ia->ia6_flags & IN6_IFF_AUTOCONF) == 0)
					continue;

				if (ia->ia6_ndpr == pr)
					in6_purgeaddr(&ia->ia_ifa);
			}
			prelist_remove(pr);
		}
		break;
	}
	case SIOCSRTRFLUSH_IN6:
	{
		/* flush all the default routers */
		struct nd_defrouter *dr, *next;

		defrouter_reset();
		TAILQ_FOREACH_SAFE(dr, &V_nd_defrouter, dr_entry, next) {
			defrtrlist_del(dr);
		}
		defrouter_select();
		break;
	}
	case SIOCGNBRINFO_IN6:
	{
		struct llentry *ln;
		struct in6_addr nb_addr = nbi->addr; /* make local for safety */

		if ((error = in6_setscope(&nb_addr, ifp, NULL)) != 0)
			return (error);

		IF_AFDATA_RLOCK(ifp);
		ln = nd6_lookup(&nb_addr, 0, ifp);
		IF_AFDATA_RUNLOCK(ifp);

		if (ln == NULL) {
			error = EINVAL;
			break;
		}
		nbi->state = ln->ln_state;
		nbi->asked = ln->la_asked;
		nbi->isrouter = ln->ln_router;
		if (ln->la_expire == 0)
			nbi->expire = 0;
		else
			nbi->expire = ln->la_expire +
			    (time_second - time_uptime);
		LLE_RUNLOCK(ln);
		break;
	}
	case SIOCGDEFIFACE_IN6:	/* XXX: should be implemented as a sysctl? */
		ndif->ifindex = V_nd6_defifindex;
		break;
	case SIOCSDEFIFACE_IN6:	/* XXX: should be implemented as a sysctl? */
		return (nd6_setdefaultiface(ndif->ifindex));
	}
	return (error);
}

/*
 * Calculates new isRouter value based on provided parameters and
 * returns it.
 */
static int
nd6_is_router(int type, int code, int is_new, int old_addr, int new_addr,
    int ln_router)
{

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
	 *	1	--	n	--	(6) c	c	c s
	 *	1	--	y	--	(7) c	c   s	c s
	 *
	 *					(c=clear s=set)
	 */
	switch (type & 0xff) {
	case ND_NEIGHBOR_SOLICIT:
		/*
		 * New entry must have is_router flag cleared.
		 */
		if (is_new)					/* (6-7) */
			ln_router = 0;
		break;
	case ND_REDIRECT:
		/*
		 * If the icmp is a redirect to a better router, always set the
		 * is_router flag.  Otherwise, if the entry is newly created,
		 * clear the flag.  [RFC 2461, sec 8.3]
		 */
		if (code == ND_REDIRECT_ROUTER)
			ln_router = 1;
		else {
			if (is_new)				/* (6-7) */
				ln_router = 0;
		}
		break;
	case ND_ROUTER_SOLICIT:
		/*
		 * is_router flag must always be cleared.
		 */
		ln_router = 0;
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Mark an entry with lladdr as a router.
		 */
		if ((!is_new && (old_addr || new_addr)) ||	/* (2-5) */
		    (is_new && new_addr)) {			/* (7) */
			ln_router = 1;
		}
		break;
	}

	return (ln_router);
}

/*
 * Create neighbor cache entry and cache link-layer address,
 * on reception of inbound ND6 packets.  (RS/RA/NS/redirect)
 *
 * type - ICMP6 type
 * code - type dependent information
 *
 */
void
nd6_cache_lladdr(struct ifnet *ifp, struct in6_addr *from, char *lladdr,
    int lladdrlen, int type, int code)
{
	struct llentry *ln = NULL, *ln_tmp;
	int is_newentry;
	int do_update;
	int olladdr;
	int llchange;
	int flags;
	int newstate = 0;
	uint16_t router = 0;
	struct sockaddr_in6 sin6;
	struct mbuf *chain = NULL;

	IF_AFDATA_UNLOCK_ASSERT(ifp);

	KASSERT(ifp != NULL, ("%s: ifp == NULL", __func__));
	KASSERT(from != NULL, ("%s: from == NULL", __func__));

	/* nothing must be updated for unspecified address */
	if (IN6_IS_ADDR_UNSPECIFIED(from))
		return;

	/*
	 * Validation about ifp->if_addrlen and lladdrlen must be done in
	 * the caller.
	 *
	 * XXX If the link does not have link-layer adderss, what should
	 * we do? (ifp->if_addrlen == 0)
	 * Spec says nothing in sections for RA, RS and NA.  There's small
	 * description on it in NS section (RFC 2461 7.2.3).
	 */
	flags = lladdr ? ND6_EXCLUSIVE : 0;
	IF_AFDATA_RLOCK(ifp);
	ln = nd6_lookup(from, flags, ifp);
	IF_AFDATA_RUNLOCK(ifp);
	is_newentry = 0;
	if (ln == NULL) {
		flags |= ND6_EXCLUSIVE;
		ln = nd6_alloc(from, 0, ifp);
		if (ln == NULL)
			return;
		IF_AFDATA_WLOCK(ifp);
		LLE_WLOCK(ln);
		/* Prefer any existing lle over newly-created one */
		ln_tmp = nd6_lookup(from, ND6_EXCLUSIVE, ifp);
		if (ln_tmp == NULL)
			lltable_link_entry(LLTABLE6(ifp), ln);
		IF_AFDATA_WUNLOCK(ifp);
		if (ln_tmp == NULL)
			/* No existing lle, mark as new entry */
			is_newentry = 1;
		else {
			lltable_free_entry(LLTABLE6(ifp), ln);
			ln = ln_tmp;
			ln_tmp = NULL;
		}
	} 
	/* do nothing if static ndp is set */
	if ((ln->la_flags & LLE_STATIC)) {
		if (flags & ND6_EXCLUSIVE)
			LLE_WUNLOCK(ln);
		else
			LLE_RUNLOCK(ln);
		return;
	}

	olladdr = (ln->la_flags & LLE_VALID) ? 1 : 0;
	if (olladdr && lladdr) {
		llchange = bcmp(lladdr, &ln->ll_addr,
		    ifp->if_addrlen);
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

	if (lladdr) {		/* (3-5) and (7) */
		/*
		 * Record source link-layer address
		 * XXX is it dependent to ifp->if_type?
		 */
		bcopy(lladdr, &ln->ll_addr, ifp->if_addrlen);
		ln->la_flags |= LLE_VALID;
		EVENTHANDLER_INVOKE(lle_event, ln, LLENTRY_RESOLVED);
	}

	if (!is_newentry) {
		if ((!olladdr && lladdr != NULL) ||	/* (3) */
		    (olladdr && lladdr != NULL && llchange)) {	/* (5) */
			do_update = 1;
			newstate = ND6_LLINFO_STALE;
		} else					/* (1-2,4) */
			do_update = 0;
	} else {
		do_update = 1;
		if (lladdr == NULL)			/* (6) */
			newstate = ND6_LLINFO_NOSTATE;
		else					/* (7) */
			newstate = ND6_LLINFO_STALE;
	}

	if (do_update) {
		/*
		 * Update the state of the neighbor cache.
		 */
		ln->ln_state = newstate;

		if (ln->ln_state == ND6_LLINFO_STALE) {
			if (ln->la_hold != NULL)
				nd6_grab_holdchain(ln, &chain, &sin6);
		} else if (ln->ln_state == ND6_LLINFO_INCOMPLETE) {
			/* probe right away */
			nd6_llinfo_settimer_locked((void *)ln, 0);
		}
	}

	/* Calculates new router status */
	router = nd6_is_router(type, code, is_newentry, olladdr,
	    lladdr != NULL ? 1 : 0, ln->ln_router);

	ln->ln_router = router;
	/* Mark non-router redirects with special flag */
	if ((type & 0xFF) == ND_REDIRECT && code != ND_REDIRECT_ROUTER)
		ln->la_flags |= LLE_REDIRECT;

	if (flags & ND6_EXCLUSIVE)
		LLE_WUNLOCK(ln);
	else
		LLE_RUNLOCK(ln);

	if (chain != NULL)
		nd6_flush_holdchain(ifp, ifp, chain, &sin6);
	
	/*
	 * When the link-layer address of a router changes, select the
	 * best router again.  In particular, when the neighbor entry is newly
	 * created, it might affect the selection policy.
	 * Question: can we restrict the first condition to the "is_newentry"
	 * case?
	 * XXX: when we hear an RA from a new router with the link-layer
	 * address option, defrouter_select() is called twice, since
	 * defrtrlist_update called the function as well.  However, I believe
	 * we can compromise the overhead, since it only happens the first
	 * time.
	 * XXX: although defrouter_select() should not have a bad effect
	 * for those are not autoconfigured hosts, we explicitly avoid such
	 * cases for safety.
	 */
	if (do_update && router &&
	    ND_IFINFO(ifp)->flags & ND6_IFF_ACCEPT_RTADV) {
		/*
		 * guaranteed recursion
		 */
		defrouter_select();
	}
}

static void
nd6_slowtimo(void *arg)
{
	CURVNET_SET((struct vnet *) arg);
	struct nd_ifinfo *nd6if;
	struct ifnet *ifp;

	callout_reset(&V_nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL * hz,
	    nd6_slowtimo, curvnet);
	IFNET_RLOCK_NOSLEEP();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (ifp->if_afdata[AF_INET6] == NULL)
			continue;
		nd6if = ND_IFINFO(ifp);
		if (nd6if->basereachable && /* already initialized */
		    (nd6if->recalctm -= ND6_SLOWTIMER_INTERVAL) <= 0) {
			/*
			 * Since reachable time rarely changes by router
			 * advertisements, we SHOULD insure that a new random
			 * value gets recomputed at least once every few hours.
			 * (RFC 2461, 6.3.4)
			 */
			nd6if->recalctm = V_nd6_recalc_reachtm_interval;
			nd6if->reachable = ND_COMPUTE_RTIME(nd6if->basereachable);
		}
	}
	IFNET_RUNLOCK_NOSLEEP();
	CURVNET_RESTORE();
}

void
nd6_grab_holdchain(struct llentry *ln, struct mbuf **chain,
    struct sockaddr_in6 *sin6)
{

	LLE_WLOCK_ASSERT(ln);

	*chain = ln->la_hold;
	ln->la_hold = NULL;
	lltable_fill_sa_entry(ln, (struct sockaddr *)sin6);

	if (ln->ln_state == ND6_LLINFO_STALE) {

		/*
		 * The first time we send a packet to a
		 * neighbor whose entry is STALE, we have
		 * to change the state to DELAY and a sets
		 * a timer to expire in DELAY_FIRST_PROBE_TIME
		 * seconds to ensure do neighbor unreachability
		 * detection on expiration.
		 * (RFC 2461 7.3.3)
		 */
		ln->la_asked = 0;
		ln->ln_state = ND6_LLINFO_DELAY;
		nd6_llinfo_settimer_locked(ln, (long)V_nd6_delay * hz);
	}
}

int
nd6_output_ifp(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m,
    struct sockaddr_in6 *dst)
{
	int error;
	int ip6len;
	struct ip6_hdr *ip6;
	struct m_tag *mtag;

#ifdef MAC
	mac_netinet6_nd6_send(ifp, m);
#endif

	/*
	 * If called from nd6_ns_output() (NS), nd6_na_output() (NA),
	 * icmp6_redirect_output() (REDIRECT) or from rip6_output() (RS, RA
	 * as handled by rtsol and rtadvd), mbufs will be tagged for SeND
	 * to be diverted to user space.  When re-injected into the kernel,
	 * send_output() will directly dispatch them to the outgoing interface.
	 */
	if (send_sendso_input_hook != NULL) {
		mtag = m_tag_find(m, PACKET_TAG_ND_OUTGOING, NULL);
		if (mtag != NULL) {
			ip6 = mtod(m, struct ip6_hdr *);
			ip6len = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen);
			/* Use the SEND socket */
			error = send_sendso_input_hook(m, ifp, SND_OUT,
			    ip6len);
			/* -1 == no app on SEND socket */
			if (error == 0 || error != -1)
			    return (error);
		}
	}

	m_clrprotoflags(m);	/* Avoid confusing lower layers. */
	IP_PROBE(send, NULL, NULL, mtod(m, struct ip6_hdr *), ifp, NULL,
	    mtod(m, struct ip6_hdr *));

	if ((ifp->if_flags & IFF_LOOPBACK) == 0)
		origifp = ifp;

	error = (*ifp->if_output)(origifp, m, (struct sockaddr *)dst, NULL);
	return (error);
}

/*
 * Do L2 address resolution for @sa_dst address. Stores found
 * address in @desten buffer. Copy of lle ln_flags can be also
 * saved in @pflags if @pflags is non-NULL.
 *
 * If destination LLE does not exists or lle state modification
 * is required, call "slow" version.
 *
 * Return values:
 * - 0 on success (address copied to buffer).
 * - EWOULDBLOCK (no local error, but address is still unresolved)
 * - other errors (alloc failure, etc)
 */
int
nd6_resolve(struct ifnet *ifp, int is_gw, struct mbuf *m,
    const struct sockaddr *sa_dst, u_char *desten, uint32_t *pflags)
{
	struct llentry *ln = NULL;
	const struct sockaddr_in6 *dst6;

	if (pflags != NULL)
		*pflags = 0;

	dst6 = (const struct sockaddr_in6 *)sa_dst;

	/* discard the packet if IPv6 operation is disabled on the interface */
	if ((ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED)) {
		m_freem(m);
		return (ENETDOWN); /* better error? */
	}

	if (m != NULL && m->m_flags & M_MCAST) {
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_FDDI:
		case IFT_L2VLAN:
		case IFT_IEEE80211:
		case IFT_BRIDGE:
		case IFT_ISO88025:
			ETHER_MAP_IPV6_MULTICAST(&dst6->sin6_addr,
						 desten);
			return (0);
		default:
			m_freem(m);
			return (EAFNOSUPPORT);
		}
	}

	IF_AFDATA_RLOCK(ifp);
	ln = nd6_lookup(&dst6->sin6_addr, 0, ifp);
	IF_AFDATA_RUNLOCK(ifp);

	/*
	 * Perform fast path for the following cases:
	 * 1) lle state is REACHABLE
	 * 2) lle state is DELAY (NS message sentNS message sent)
	 *
	 * Every other case involves lle modification, so we handle
	 * them separately.
	 */
	if (ln == NULL || (ln->ln_state != ND6_LLINFO_REACHABLE &&
	    ln->ln_state != ND6_LLINFO_DELAY)) {
		/* Fall back to slow processing path */
		if (ln != NULL)
			LLE_RUNLOCK(ln);
		return (nd6_resolve_slow(ifp, m, dst6, desten, pflags));
	}


	bcopy(&ln->ll_addr, desten, ifp->if_addrlen);
	if (pflags != NULL)
		*pflags = ln->la_flags;
	LLE_RUNLOCK(ln);
	return (0);
}


/*
 * Do L2 address resolution for @sa_dst address. Stores found
 * address in @desten buffer. Copy of lle ln_flags can be also
 * saved in @pflags if @pflags is non-NULL.
 *
 * Heavy version.
 * Function assume that destination LLE does not exist,
 * is invalid or stale, so ND6_EXCLUSIVE lock needs to be acquired.
 */
static int
nd6_resolve_slow(struct ifnet *ifp, struct mbuf *m,
    const struct sockaddr_in6 *dst, u_char *desten, uint32_t *pflags)
{
	struct llentry *lle = NULL, *lle_tmp;

	/*
	 * Address resolution or Neighbor Unreachability Detection
	 * for the next hop.
	 * At this point, the destination of the packet must be a unicast
	 * or an anycast address(i.e. not a multicast).
	 */
	if (lle == NULL) {
		IF_AFDATA_RLOCK(ifp);
		lle = nd6_lookup(&dst->sin6_addr, ND6_EXCLUSIVE, ifp);
		IF_AFDATA_RUNLOCK(ifp);
		if ((lle == NULL) && nd6_is_addr_neighbor(dst, ifp))  {
			/*
			 * Since nd6_is_addr_neighbor() internally calls nd6_lookup(),
			 * the condition below is not very efficient.  But we believe
			 * it is tolerable, because this should be a rare case.
			 */
			lle = nd6_alloc(&dst->sin6_addr, 0, ifp);
			if (lle == NULL) {
				char ip6buf[INET6_ADDRSTRLEN];
				log(LOG_DEBUG,
				    "nd6_output: can't allocate llinfo for %s "
				    "(ln=%p)\n",
				    ip6_sprintf(ip6buf, &dst->sin6_addr), lle);
				m_freem(m);
				return (ENOBUFS);
			}
			lle->ln_state = ND6_LLINFO_NOSTATE;

			IF_AFDATA_WLOCK(ifp);
			LLE_WLOCK(lle);
			/* Prefer any existing entry over newly-created one */
			lle_tmp = nd6_lookup(&dst->sin6_addr, ND6_EXCLUSIVE, ifp);
			if (lle_tmp == NULL)
				lltable_link_entry(LLTABLE6(ifp), lle);
			IF_AFDATA_WUNLOCK(ifp);
			if (lle_tmp != NULL) {
				lltable_free_entry(LLTABLE6(ifp), lle);
				lle = lle_tmp;
				lle_tmp = NULL;
			}
		}
	} 
	if (lle == NULL) {
		if (!(ND_IFINFO(ifp)->flags & ND6_IFF_PERFORMNUD)) {
			m_freem(m);
			return (ENOBUFS);
		}

		if (m != NULL)
			m_freem(m);
		return (ENOBUFS);
	}

	LLE_WLOCK_ASSERT(lle);

	/*
	 * The first time we send a packet to a neighbor whose entry is
	 * STALE, we have to change the state to DELAY and a sets a timer to
	 * expire in DELAY_FIRST_PROBE_TIME seconds to ensure do
	 * neighbor unreachability detection on expiration.
	 * (RFC 2461 7.3.3)
	 */
	if (lle->ln_state == ND6_LLINFO_STALE) {
		lle->la_asked = 0;
		lle->ln_state = ND6_LLINFO_DELAY;
		nd6_llinfo_settimer_locked(lle, (long)V_nd6_delay * hz);
	}

	/*
	 * If the neighbor cache entry has a state other than INCOMPLETE
	 * (i.e. its link-layer address is already resolved), just
	 * send the packet.
	 */
	if (lle->ln_state > ND6_LLINFO_INCOMPLETE) {
		bcopy(&lle->ll_addr, desten, ifp->if_addrlen);
		if (pflags != NULL)
			*pflags = lle->la_flags;
		LLE_WUNLOCK(lle);
		return (0);
	}

	/*
	 * There is a neighbor cache entry, but no ethernet address
	 * response yet.  Append this latest packet to the end of the
	 * packet queue in the mbuf, unless the number of the packet
	 * does not exceed nd6_maxqueuelen.  When it exceeds nd6_maxqueuelen,
	 * the oldest packet in the queue will be removed.
	 */
	if (lle->ln_state == ND6_LLINFO_NOSTATE)
		lle->ln_state = ND6_LLINFO_INCOMPLETE;

	if (lle->la_hold != NULL) {
		struct mbuf *m_hold;
		int i;
		
		i = 0;
		for (m_hold = lle->la_hold; m_hold; m_hold = m_hold->m_nextpkt){
			i++;
			if (m_hold->m_nextpkt == NULL) {
				m_hold->m_nextpkt = m;
				break;
			}
		}
		while (i >= V_nd6_maxqueuelen) {
			m_hold = lle->la_hold;
			lle->la_hold = lle->la_hold->m_nextpkt;
			m_freem(m_hold);
			i--;
		}
	} else {
		lle->la_hold = m;
	}

	/*
	 * If there has been no NS for the neighbor after entering the
	 * INCOMPLETE state, send the first solicitation.
	 */
	if (!ND6_LLINFO_PERMANENT(lle) && lle->la_asked == 0) {
		struct in6_addr src, *psrc;
		lle->la_asked++;
		
		nd6_llinfo_settimer_locked(lle,
		    (long)ND_IFINFO(ifp)->retrans * hz / 1000);
		psrc = nd6_llinfo_get_holdsrc(lle, &src);
		LLE_WUNLOCK(lle);
		nd6_ns_output(ifp, psrc, NULL, &dst->sin6_addr, NULL);
	} else {
		/* We did the lookup so we need to do the unlock here. */
		LLE_WUNLOCK(lle);
	}

	return (EWOULDBLOCK);
}


int
nd6_flush_holdchain(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *chain,
    struct sockaddr_in6 *dst)
{
	struct mbuf *m, *m_head;
	struct ifnet *outifp;
	int error = 0;

	m_head = chain;
	if ((ifp->if_flags & IFF_LOOPBACK) != 0)
		outifp = origifp;
	else
		outifp = ifp;
	
	while (m_head) {
		m = m_head;
		m_head = m_head->m_nextpkt;
		error = nd6_output_ifp(ifp, origifp, m, dst);
	}

	/*
	 * XXX
	 * note that intermediate errors are blindly ignored
	 */
	return (error);
}	

static int
nd6_need_cache(struct ifnet *ifp)
{
	/*
	 * XXX: we currently do not make neighbor cache on any interface
	 * other than ARCnet, Ethernet, FDDI and GIF.
	 *
	 * RFC2893 says:
	 * - unidirectional tunnels needs no ND
	 */
	switch (ifp->if_type) {
	case IFT_ARCNET:
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_IEEE1394:
	case IFT_L2VLAN:
	case IFT_IEEE80211:
	case IFT_INFINIBAND:
	case IFT_BRIDGE:
	case IFT_PROPVIRTUAL:
		return (1);
	default:
		return (0);
	}
}

/*
 * Add pernament ND6 link-layer record for given
 * interface address.
 *
 * Very similar to IPv4 arp_ifinit(), but:
 * 1) IPv6 DAD is performed in different place
 * 2) It is called by IPv6 protocol stack in contrast to
 * arp_ifinit() which is typically called in SIOCSIFADDR
 * driver ioctl handler.
 *
 */
int
nd6_add_ifa_lle(struct in6_ifaddr *ia)
{
	struct ifnet *ifp;
	struct llentry *ln, *ln_tmp;
	struct sockaddr *dst;

	ifp = ia->ia_ifa.ifa_ifp;
	if (nd6_need_cache(ifp) == 0)
		return (0);

	ia->ia_ifa.ifa_rtrequest = nd6_rtrequest;
	dst = (struct sockaddr *)&ia->ia_addr;
	ln = lltable_alloc_entry(LLTABLE6(ifp), LLE_IFADDR, dst);
	if (ln == NULL)
		return (ENOBUFS);

	IF_AFDATA_WLOCK(ifp);
	LLE_WLOCK(ln);
	/* Unlink any entry if exists */
	ln_tmp = lla_lookup(LLTABLE6(ifp), LLE_EXCLUSIVE, dst);
	if (ln_tmp != NULL)
		lltable_unlink_entry(LLTABLE6(ifp), ln_tmp);
	lltable_link_entry(LLTABLE6(ifp), ln);
	IF_AFDATA_WUNLOCK(ifp);

	if (ln_tmp != NULL)
		EVENTHANDLER_INVOKE(lle_event, ln_tmp, LLENTRY_EXPIRED);
	EVENTHANDLER_INVOKE(lle_event, ln, LLENTRY_RESOLVED);

	LLE_WUNLOCK(ln);
	if (ln_tmp != NULL)
		llentry_free(ln_tmp);

	return (0);
}

/*
 * Removes either all lle entries for given @ia, or lle
 * corresponding to @ia address.
 */
void
nd6_rem_ifa_lle(struct in6_ifaddr *ia, int all)
{
	struct sockaddr_in6 mask, addr;
	struct sockaddr *saddr, *smask;
	struct ifnet *ifp;

	ifp = ia->ia_ifa.ifa_ifp;
	memcpy(&addr, &ia->ia_addr, sizeof(ia->ia_addr));
	memcpy(&mask, &ia->ia_prefixmask, sizeof(ia->ia_prefixmask));
	saddr = (struct sockaddr *)&addr;
	smask = (struct sockaddr *)&mask;

	if (all != 0)
		lltable_prefix_free(AF_INET6, saddr, smask, LLE_STATIC);
	else
		lltable_delete_addr(LLTABLE6(ifp), LLE_IFADDR, saddr);
}

static void 
clear_llinfo_pqueue(struct llentry *ln)
{
	struct mbuf *m_hold, *m_hold_next;

	for (m_hold = ln->la_hold; m_hold; m_hold = m_hold_next) {
		m_hold_next = m_hold->m_nextpkt;
		m_freem(m_hold);
	}

	ln->la_hold = NULL;
	return;
}

static int nd6_sysctl_drlist(SYSCTL_HANDLER_ARGS);
static int nd6_sysctl_prlist(SYSCTL_HANDLER_ARGS);
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet6_icmp6);
#endif
SYSCTL_NODE(_net_inet6_icmp6, ICMPV6CTL_ND6_DRLIST, nd6_drlist,
	CTLFLAG_RD, nd6_sysctl_drlist, "");
SYSCTL_NODE(_net_inet6_icmp6, ICMPV6CTL_ND6_PRLIST, nd6_prlist,
	CTLFLAG_RD, nd6_sysctl_prlist, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_MAXQLEN, nd6_maxqueuelen,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(nd6_maxqueuelen), 1, "");
SYSCTL_INT(_net_inet6_icmp6, OID_AUTO, nd6_gctimer,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(nd6_gctimer), (60 * 60 * 24), "");

static int
nd6_sysctl_drlist(SYSCTL_HANDLER_ARGS)
{
	struct in6_defrouter d;
	struct nd_defrouter *dr;
	int error;

	if (req->newptr)
		return (EPERM);

	bzero(&d, sizeof(d));
	d.rtaddr.sin6_family = AF_INET6;
	d.rtaddr.sin6_len = sizeof(d.rtaddr);

	/*
	 * XXX locking
	 */
	TAILQ_FOREACH(dr, &V_nd_defrouter, dr_entry) {
		d.rtaddr.sin6_addr = dr->rtaddr;
		error = sa6_recoverscope(&d.rtaddr);
		if (error != 0)
			return (error);
		d.flags = dr->flags;
		d.rtlifetime = dr->rtlifetime;
		d.expire = dr->expire + (time_second - time_uptime);
		d.if_index = dr->ifp->if_index;
		error = SYSCTL_OUT(req, &d, sizeof(d));
		if (error != 0)
			return (error);
	}
	return (0);
}

static int
nd6_sysctl_prlist(SYSCTL_HANDLER_ARGS)
{
	struct in6_prefix p;
	struct sockaddr_in6 s6;
	struct nd_prefix *pr;
	struct nd_pfxrouter *pfr;
	time_t maxexpire;
	int error;
	char ip6buf[INET6_ADDRSTRLEN];

	if (req->newptr)
		return (EPERM);

	bzero(&p, sizeof(p));
	p.origin = PR_ORIG_RA;
	bzero(&s6, sizeof(s6));
	s6.sin6_family = AF_INET6;
	s6.sin6_len = sizeof(s6);

	/*
	 * XXX locking
	 */
	LIST_FOREACH(pr, &V_nd_prefix, ndpr_entry) {
		p.prefix = pr->ndpr_prefix;
		if (sa6_recoverscope(&p.prefix)) {
			log(LOG_ERR, "scope error in prefix list (%s)\n",
			    ip6_sprintf(ip6buf, &p.prefix.sin6_addr));
			/* XXX: press on... */
		}
		p.raflags = pr->ndpr_raf;
		p.prefixlen = pr->ndpr_plen;
		p.vltime = pr->ndpr_vltime;
		p.pltime = pr->ndpr_pltime;
		p.if_index = pr->ndpr_ifp->if_index;
		if (pr->ndpr_vltime == ND6_INFINITE_LIFETIME)
			p.expire = 0;
		else {
			/* XXX: we assume time_t is signed. */
			maxexpire = (-1) &
			    ~((time_t)1 << ((sizeof(maxexpire) * 8) - 1));
			if (pr->ndpr_vltime < maxexpire - pr->ndpr_lastupdate)
				p.expire = pr->ndpr_lastupdate +
				    pr->ndpr_vltime +
				    (time_second - time_uptime);
			else
				p.expire = maxexpire;
		}
		p.refcnt = pr->ndpr_refcnt;
		p.flags = pr->ndpr_stateflags;
		p.advrtrs = 0;
		LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry)
			p.advrtrs++;
		error = SYSCTL_OUT(req, &p, sizeof(p));
		if (error != 0)
			return (error);
		LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry) {
			s6.sin6_addr = pfr->router->rtaddr;
			if (sa6_recoverscope(&s6))
				log(LOG_ERR,
				    "scope error in prefix list (%s)\n",
				    ip6_sprintf(ip6buf, &pfr->router->rtaddr));
			error = SYSCTL_OUT(req, &s6, sizeof(s6));
			if (error != 0)
				return (error);
		}
	}
	return (0);
}

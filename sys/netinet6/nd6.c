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
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/iso88025.h>
#include <net/fddi.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <net/if_llatbl.h>
#define	L3_ADDR_SIN6(le)	((struct sockaddr_in6 *) L3_ADDR(le))
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

#define SIN6(s) ((struct sockaddr_in6 *)s)

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

/* for debugging? */
#if 0
static int nd6_inuse, nd6_allocated;
#endif

VNET_DEFINE(struct nd_drhead, nd_defrouter);
VNET_DEFINE(struct nd_prhead, nd_prefix);

VNET_DEFINE(int, nd6_recalc_reachtm_interval) = ND6_RECALC_REACHTM_INTERVAL;
#define	V_nd6_recalc_reachtm_interval	VNET(nd6_recalc_reachtm_interval)

static struct sockaddr_in6 all1_sa;

int	(*send_sendso_input_hook)(struct mbuf *, struct ifnet *, int, int);

static int nd6_is_new_addr_neighbor __P((struct sockaddr_in6 *,
	struct ifnet *));
static void nd6_setmtu0(struct ifnet *, struct nd_ifinfo *);
static void nd6_slowtimo(void *);
static int regen_tmpaddr(struct in6_ifaddr *);
static struct llentry *nd6_free(struct llentry *, int);
static void nd6_llinfo_timer(void *);
static void clear_llinfo_pqueue(struct llentry *);

static VNET_DEFINE(struct callout, nd6_slowtimo_ch);
#define	V_nd6_slowtimo_ch		VNET(nd6_slowtimo_ch)

VNET_DEFINE(struct callout, nd6_timer_ch);

void
nd6_init(void)
{
	int i;

	LIST_INIT(&V_nd_prefix);

	all1_sa.sin6_family = AF_INET6;
	all1_sa.sin6_len = sizeof(struct sockaddr_in6);
	for (i = 0; i < sizeof(all1_sa.sin6_addr); i++)
		all1_sa.sin6_addr.s6_addr[i] = 0xff;

	/* initialization of the default router list */
	TAILQ_INIT(&V_nd_defrouter);

	/* start timer */
	callout_init(&V_nd6_slowtimo_ch, 0);
	callout_reset(&V_nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL * hz,
	    nd6_slowtimo, curvnet);
}

#ifdef VIMAGE
void
nd6_destroy()
{

	callout_drain(&V_nd6_slowtimo_ch);
	callout_drain(&V_nd6_timer_ch);
}
#endif

struct nd_ifinfo *
nd6_ifattach(struct ifnet *ifp)
{
	struct nd_ifinfo *nd;

	nd = (struct nd_ifinfo *)malloc(sizeof(*nd), M_IP6NDP, M_WAITOK);
	bzero(nd, sizeof(*nd));

	nd->initialized = 1;

	nd->chlim = IPV6_DEFHLIM;
	nd->basereachable = REACHABLE_TIME;
	nd->reachable = ND_COMPUTE_RTIME(nd->basereachable);
	nd->retrans = RETRANS_TIMER;

	nd->flags = ND6_IFF_PERFORMNUD;

	/* A loopback interface always has ND6_IFF_AUTO_LINKLOCAL. */
	if (V_ip6_auto_linklocal || (ifp->if_flags & IFF_LOOPBACK))
		nd->flags |= ND6_IFF_AUTO_LINKLOCAL;

	/* A loopback interface does not need to accept RTADV. */
	if (V_ip6_accept_rtadv && !(ifp->if_flags & IFF_LOOPBACK))
		nd->flags |= ND6_IFF_ACCEPT_RTADV;

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

	if (ndopts == NULL)
		panic("ndopts == NULL in nd6_option");
	if (ndopts->nd_opts_last == NULL)
		panic("uninitialized ndopts in nd6_option");
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

	if (ndopts == NULL)
		panic("ndopts == NULL in nd6_options");
	if (ndopts->nd_opts_last == NULL)
		panic("uninitialized ndopts in nd6_options");
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
		canceled = callout_stop(&ln->ln_timer_ch);
	} else {
		ln->la_expire = time_second + tick / hz;
		LLE_ADDREF(ln);
		if (tick > INT_MAX) {
			ln->ln_ntick = tick - INT_MAX;
			canceled = callout_reset(&ln->ln_timer_ch, INT_MAX,
			    nd6_llinfo_timer, ln);
		} else {
			ln->ln_ntick = 0;
			canceled = callout_reset(&ln->ln_timer_ch, tick,
			    nd6_llinfo_timer, ln);
		}
	}
	if (canceled)
		LLE_REMREF(ln);
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
	struct in6_addr *dst;
	struct ifnet *ifp;
	struct nd_ifinfo *ndi = NULL;

	KASSERT(arg != NULL, ("%s: arg NULL", __func__));
	ln = (struct llentry *)arg;
	LLE_WLOCK_ASSERT(ln);
	ifp = ln->lle_tbl->llt_ifp;

	CURVNET_SET(ifp->if_vnet);

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

	ndi = ND_IFINFO(ifp);
	dst = &L3_ADDR_SIN6(ln)->sin6_addr;
	if (ln->la_flags & LLE_STATIC) {
		goto done;
	}

	if (ln->la_flags & LLE_DELETED) {
		(void)nd6_free(ln, 0);
		ln = NULL;
		goto done;
	}

	switch (ln->ln_state) {
	case ND6_LLINFO_INCOMPLETE:
		if (ln->la_asked < V_nd6_mmaxtries) {
			ln->la_asked++;
			nd6_llinfo_settimer_locked(ln, (long)ndi->retrans * hz / 1000);
			LLE_WUNLOCK(ln);
			nd6_ns_output(ifp, NULL, dst, ln, 0);
			LLE_WLOCK(ln);
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
			(void)nd6_free(ln, 0);
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
			(void)nd6_free(ln, 1);
			ln = NULL;
		}
		break;

	case ND6_LLINFO_DELAY:
		if (ndi && (ndi->flags & ND6_IFF_PERFORMNUD) != 0) {
			/* We need NUD */
			ln->la_asked = 1;
			ln->ln_state = ND6_LLINFO_PROBE;
			nd6_llinfo_settimer_locked(ln, (long)ndi->retrans * hz / 1000);
			LLE_WUNLOCK(ln);
			nd6_ns_output(ifp, dst, dst, ln, 0);
			LLE_WLOCK(ln);
		} else {
			ln->ln_state = ND6_LLINFO_STALE; /* XXX */
			nd6_llinfo_settimer_locked(ln, (long)V_nd6_gctimer * hz);
		}
		break;
	case ND6_LLINFO_PROBE:
		if (ln->la_asked < V_nd6_umaxtries) {
			ln->la_asked++;
			nd6_llinfo_settimer_locked(ln, (long)ndi->retrans * hz / 1000);
			LLE_WUNLOCK(ln);
			nd6_ns_output(ifp, dst, dst, ln, 0);
			LLE_WLOCK(ln);
		} else {
			(void)nd6_free(ln, 0);
			ln = NULL;
		}
		break;
	default:
		panic("%s: paths in a dark night can be confusing: %d",
		    __func__, ln->ln_state);
	}
done:
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
	int s;
	struct nd_defrouter *dr;
	struct nd_prefix *pr;
	struct in6_ifaddr *ia6, *nia6;
	struct in6_addrlifetime *lt6;

	callout_reset(&V_nd6_timer_ch, V_nd6_prune * hz,
	    nd6_timer, curvnet);

	/* expire default router list */
	s = splnet();
	dr = TAILQ_FIRST(&V_nd_defrouter);
	while (dr) {
		if (dr->expire && dr->expire < time_second) {
			struct nd_defrouter *t;
			t = TAILQ_NEXT(dr, dr_entry);
			defrtrlist_del(dr);
			dr = t;
		} else {
			dr = TAILQ_NEXT(dr, dr_entry);
		}
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
		lt6 = &ia6->ia6_lifetime;
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
	pr = V_nd_prefix.lh_first;
	while (pr) {
		/*
		 * check prefix lifetime.
		 * since pltime is just for autoconf, pltime processing for
		 * prefix is not necessary.
		 */
		if (pr->ndpr_vltime != ND6_INFINITE_LIFETIME &&
		    time_second - pr->ndpr_lastupdate > pr->ndpr_vltime) {
			struct nd_prefix *t;
			t = pr->ndpr_next;

			/*
			 * address expiration and prefix expiration are
			 * separate.  NEVER perform in6_purgeaddr here.
			 */

			prelist_remove(pr);
			pr = t;
		} else
			pr = pr->ndpr_next;
	}
	splx(s);
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
	IF_ADDR_LOCK(ifp);
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

		if (public_ifa6 != NULL)
			ifa_ref(&public_ifa6->ia_ifa);
	}
	IF_ADDR_UNLOCK(ifp);

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
	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr; dr = ndr) {
		ndr = TAILQ_NEXT(dr, dr_entry);
		if (dr->installed)
			continue;

		if (dr->ifp == ifp)
			defrtrlist_del(dr);
	}

	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr; dr = ndr) {
		ndr = TAILQ_NEXT(dr, dr_entry);
		if (!dr->installed)
			continue;

		if (dr->ifp == ifp)
			defrtrlist_del(dr);
	}

	/* Nuke prefix list entries toward ifp */
	for (pr = V_nd_prefix.lh_first; pr; pr = npr) {
		npr = pr->ndpr_next;
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

	if (!V_ip6_forwarding && ND_IFINFO(ifp)->flags & ND6_IFF_ACCEPT_RTADV) {
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
nd6_lookup(struct in6_addr *addr6, int flags, struct ifnet *ifp)
{
	struct sockaddr_in6 sin6;
	struct llentry *ln;
	int llflags;
	
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = *addr6;

	IF_AFDATA_LOCK_ASSERT(ifp);

	llflags = 0;
	if (flags & ND6_CREATE)
	    llflags |= LLE_CREATE;
	if (flags & ND6_EXCLUSIVE)
	    llflags |= LLE_EXCLUSIVE;	
	
	ln = lla_lookup(LLTABLE6(ifp), llflags, (struct sockaddr *)&sin6);
	if ((ln != NULL) && (llflags & LLE_CREATE))
		ln->ln_state = ND6_LLINFO_NOSTATE;
	
	return (ln);
}

/*
 * Test whether a given IPv6 address is a neighbor or not, ignoring
 * the actual neighbor cache.  The neighbor cache is ignored in order
 * to not reenter the routing code from within itself.
 */
static int
nd6_is_new_addr_neighbor(struct sockaddr_in6 *addr, struct ifnet *ifp)
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
	for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		if (pr->ndpr_ifp != ifp)
			continue;

		if (!(pr->ndpr_stateflags & NDPRF_ONLINK)) {
			struct rtentry *rt;
			rt = rtalloc1((struct sockaddr *)&pr->ndpr_prefix, 0, 0);
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
	dstaddr = ifa_ifwithdstaddr((struct sockaddr *)addr);
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
	 * XXX: we restrict the condition to hosts, because routers usually do
	 * not have the "default router list".
	 */
	if (!V_ip6_forwarding && TAILQ_FIRST(&V_nd_defrouter) == NULL &&
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
nd6_is_addr_neighbor(struct sockaddr_in6 *addr, struct ifnet *ifp)
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
	IF_AFDATA_LOCK(ifp);
	if ((lle = nd6_lookup(&addr->sin6_addr, 0, ifp)) != NULL) {
		LLE_RUNLOCK(lle);
		rc = 1;
	}
	IF_AFDATA_UNLOCK(ifp);
	return (rc);
}

/*
 * Free an nd6 llinfo entry.
 * Since the function would cause significant changes in the kernel, DO NOT
 * make it global, unless you have a strong reason for the change, and are sure
 * that the change is safe.
 */
static struct llentry *
nd6_free(struct llentry *ln, int gc)
{
        struct llentry *next;
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

	if (!V_ip6_forwarding) {

		dr = defrouter_lookup(&L3_ADDR_SIN6(ln)->sin6_addr, ifp);

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
			if (dr->expire > time_second)
				nd6_llinfo_settimer_locked(ln,
				    (dr->expire - time_second) * hz);
			else
				nd6_llinfo_settimer_locked(ln,
				    (long)V_nd6_gctimer * hz);

			next = LIST_NEXT(ln, lle_next);
			LLE_REMREF(ln);
			LLE_WUNLOCK(ln);
			return (next);
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
			rt6_flush(&L3_ADDR_SIN6(ln)->sin6_addr, ifp);
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

		if (ln->ln_router || dr)
			LLE_WLOCK(ln);
	}

	/*
	 * Before deleting the entry, remember the next entry as the
	 * return value.  We need this because pfxlist_onlink_check() above
	 * might have freed other entries (particularly the old next entry) as
	 * a side effect (XXX).
	 */
	next = LIST_NEXT(ln, lle_next);

	/*
	 * Save to unlock. We still hold an extra reference and will not
	 * free(9) in llentry_free() if someone else holds one as well.
	 */
	LLE_WUNLOCK(ln);
	IF_AFDATA_LOCK(ifp);
	LLE_WLOCK(ln);
	LLE_REMREF(ln);
	llentry_free(ln);
	IF_AFDATA_UNLOCK(ifp);

	return (next);
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
	IF_AFDATA_LOCK(ifp);
	ln = nd6_lookup(dst6, ND6_EXCLUSIVE, NULL);
	IF_AFDATA_UNLOCK(ifp);
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


int
nd6_ioctl(u_long cmd, caddr_t data, struct ifnet *ifp)
{
	struct in6_drlist *drl = (struct in6_drlist *)data;
	struct in6_oprlist *oprl = (struct in6_oprlist *)data;
	struct in6_ndireq *ndi = (struct in6_ndireq *)data;
	struct in6_nbrinfo *nbi = (struct in6_nbrinfo *)data;
	struct in6_ndifreq *ndif = (struct in6_ndifreq *)data;
	struct nd_defrouter *dr;
	struct nd_prefix *pr;
	int i = 0, error = 0;
	int s;

	switch (cmd) {
	case SIOCGDRLST_IN6:
		/*
		 * obsolete API, use sysctl under net.inet6.icmp6
		 */
		bzero(drl, sizeof(*drl));
		s = splnet();
		dr = TAILQ_FIRST(&V_nd_defrouter);
		while (dr && i < DRLSTSIZ) {
			drl->defrouter[i].rtaddr = dr->rtaddr;
			in6_clearscope(&drl->defrouter[i].rtaddr);

			drl->defrouter[i].flags = dr->flags;
			drl->defrouter[i].rtlifetime = dr->rtlifetime;
			drl->defrouter[i].expire = dr->expire;
			drl->defrouter[i].if_index = dr->ifp->if_index;
			i++;
			dr = TAILQ_NEXT(dr, dr_entry);
		}
		splx(s);
		break;
	case SIOCGPRLST_IN6:
		/*
		 * obsolete API, use sysctl under net.inet6.icmp6
		 *
		 * XXX the structure in6_prlist was changed in backward-
		 * incompatible manner.  in6_oprlist is used for SIOCGPRLST_IN6,
		 * in6_prlist is used for nd6_sysctl() - fill_prlist().
		 */
		/*
		 * XXX meaning of fields, especialy "raflags", is very
		 * differnet between RA prefix list and RR/static prefix list.
		 * how about separating ioctls into two?
		 */
		bzero(oprl, sizeof(*oprl));
		s = splnet();
		pr = V_nd_prefix.lh_first;
		while (pr && i < PRLSTSIZ) {
			struct nd_pfxrouter *pfr;
			int j;

			oprl->prefix[i].prefix = pr->ndpr_prefix.sin6_addr;
			oprl->prefix[i].raflags = pr->ndpr_raf;
			oprl->prefix[i].prefixlen = pr->ndpr_plen;
			oprl->prefix[i].vltime = pr->ndpr_vltime;
			oprl->prefix[i].pltime = pr->ndpr_pltime;
			oprl->prefix[i].if_index = pr->ndpr_ifp->if_index;
			if (pr->ndpr_vltime == ND6_INFINITE_LIFETIME)
				oprl->prefix[i].expire = 0;
			else {
				time_t maxexpire;

				/* XXX: we assume time_t is signed. */
				maxexpire = (-1) &
				    ~((time_t)1 <<
				    ((sizeof(maxexpire) * 8) - 1));
				if (pr->ndpr_vltime <
				    maxexpire - pr->ndpr_lastupdate) {
					oprl->prefix[i].expire =
					    pr->ndpr_lastupdate +
					    pr->ndpr_vltime;
				} else
					oprl->prefix[i].expire = maxexpire;
			}

			pfr = pr->ndpr_advrtrs.lh_first;
			j = 0;
			while (pfr) {
				if (j < DRLSTSIZ) {
#define RTRADDR oprl->prefix[i].advrtr[j]
					RTRADDR = pfr->router->rtaddr;
					in6_clearscope(&RTRADDR);
#undef RTRADDR
				}
				j++;
				pfr = pfr->pfr_next;
			}
			oprl->prefix[i].advrtrs = j;
			oprl->prefix[i].origin = PR_ORIG_RA;

			i++;
			pr = pr->ndpr_next;
		}
		splx(s);

		break;
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
			int duplicated_linklocal = 0;

			IF_ADDR_LOCK(ifp);
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				if (ifa->ifa_addr->sa_family != AF_INET6)
					continue;
				ia = (struct in6_ifaddr *)ifa;
				if ((ia->ia6_flags & IN6_IFF_DUPLICATED) &&
				    IN6_IS_ADDR_LINKLOCAL(&ia->ia_addr.sin6_addr)) {
					duplicated_linklocal = 1;
					break;
				}
			}
			IF_ADDR_UNLOCK(ifp);

			if (duplicated_linklocal) {
				ND.flags |= ND6_IFF_IFDISABLED;
				log(LOG_ERR, "Cannot enable an interface"
				    " with a link-local address marked"
				    " duplicate.\n");
			} else {
				ND_IFINFO(ifp)->flags &= ~ND6_IFF_IFDISABLED;
				in6_if_up(ifp);
			}
		} else if (!(ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) &&
			    (ND.flags & ND6_IFF_IFDISABLED)) {
			/* ifdisabled 0->1 transision */
			/* Mark all IPv6 address as tentative. */

			ND_IFINFO(ifp)->flags |= ND6_IFF_IFDISABLED;
			IF_ADDR_LOCK(ifp);
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				if (ifa->ifa_addr->sa_family != AF_INET6)
					continue;
				ia = (struct in6_ifaddr *)ifa;
				ia->ia6_flags |= IN6_IFF_TENTATIVE;
			}
			IF_ADDR_UNLOCK(ifp);
		}

		if (!(ND_IFINFO(ifp)->flags & ND6_IFF_AUTO_LINKLOCAL) &&
		    (ND.flags & ND6_IFF_AUTO_LINKLOCAL)) {
			/* auto_linklocal 0->1 transision */

			/* If no link-local address on ifp, configure */
			ND_IFINFO(ifp)->flags |= ND6_IFF_AUTO_LINKLOCAL;
			in6_ifattach(ifp, NULL);
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

		s = splnet();
		for (pr = V_nd_prefix.lh_first; pr; pr = next) {
			struct in6_ifaddr *ia, *ia_next;

			next = pr->ndpr_next;

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
		splx(s);
		break;
	}
	case SIOCSRTRFLUSH_IN6:
	{
		/* flush all the default routers */
		struct nd_defrouter *dr, *next;

		s = splnet();
		defrouter_reset();
		for (dr = TAILQ_FIRST(&V_nd_defrouter); dr; dr = next) {
			next = TAILQ_NEXT(dr, dr_entry);
			defrtrlist_del(dr);
		}
		defrouter_select();
		splx(s);
		break;
	}
	case SIOCGNBRINFO_IN6:
	{
		struct llentry *ln;
		struct in6_addr nb_addr = nbi->addr; /* make local for safety */

		if ((error = in6_setscope(&nb_addr, ifp, NULL)) != 0)
			return (error);

		IF_AFDATA_LOCK(ifp);
		ln = nd6_lookup(&nb_addr, 0, ifp);
		IF_AFDATA_UNLOCK(ifp);

		if (ln == NULL) {
			error = EINVAL;
			break;
		}
		nbi->state = ln->ln_state;
		nbi->asked = ln->la_asked;
		nbi->isrouter = ln->ln_router;
		nbi->expire = ln->la_expire;
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
 * Create neighbor cache entry and cache link-layer address,
 * on reception of inbound ND6 packets.  (RS/RA/NS/redirect)
 *
 * type - ICMP6 type
 * code - type dependent information
 *
 * XXXXX
 *  The caller of this function already acquired the ndp 
 *  cache table lock because the cache entry is returned.
 */
struct llentry *
nd6_cache_lladdr(struct ifnet *ifp, struct in6_addr *from, char *lladdr,
    int lladdrlen, int type, int code)
{
	struct llentry *ln = NULL;
	int is_newentry;
	int do_update;
	int olladdr;
	int llchange;
	int flags;
	int newstate = 0;
	uint16_t router = 0;
	struct sockaddr_in6 sin6;
	struct mbuf *chain = NULL;
	int static_route = 0;

	IF_AFDATA_UNLOCK_ASSERT(ifp);

	if (ifp == NULL)
		panic("ifp == NULL in nd6_cache_lladdr");
	if (from == NULL)
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
	flags = lladdr ? ND6_EXCLUSIVE : 0;
	IF_AFDATA_LOCK(ifp);
	ln = nd6_lookup(from, flags, ifp);

	if (ln == NULL) {
		flags |= ND6_EXCLUSIVE;
		ln = nd6_lookup(from, flags | ND6_CREATE, ifp);
		IF_AFDATA_UNLOCK(ifp);
		is_newentry = 1;
	} else {
		IF_AFDATA_UNLOCK(ifp);		
		/* do nothing if static ndp is set */
		if (ln->la_flags & LLE_STATIC) {
			static_route = 1;
			goto done;
		}
		is_newentry = 0;
	}
	if (ln == NULL)
		return (NULL);

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
			/*
			 * XXX: since nd6_output() below will cause
			 * state tansition to DELAY and reset the timer,
			 * we must set the timer now, although it is actually
			 * meaningless.
			 */
			nd6_llinfo_settimer_locked(ln, (long)V_nd6_gctimer * hz);

			if (ln->la_hold) {
				struct mbuf *m_hold, *m_hold_next;

				/*
				 * reset the la_hold in advance, to explicitly
				 * prevent a la_hold lookup in nd6_output()
				 * (wouldn't happen, though...)
				 */
				for (m_hold = ln->la_hold, ln->la_hold = NULL;
				    m_hold; m_hold = m_hold_next) {
					m_hold_next = m_hold->m_nextpkt;
					m_hold->m_nextpkt = NULL;

					/*
					 * we assume ifp is not a p2p here, so
					 * just set the 2nd argument as the
					 * 1st one.
					 */
					nd6_output_lle(ifp, ifp, m_hold, L3_ADDR_SIN6(ln), NULL, ln, &chain);
				}
				/*
				 * If we have mbufs in the chain we need to do
				 * deferred transmit. Copy the address from the
				 * llentry before dropping the lock down below.
				 */
				if (chain != NULL)
					memcpy(&sin6, L3_ADDR_SIN6(ln), sizeof(sin6));
			}
		} else if (ln->ln_state == ND6_LLINFO_INCOMPLETE) {
			/* probe right away */
			nd6_llinfo_settimer_locked((void *)ln, 0);
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
		if (is_newentry)	/* (6-7) */
			ln->ln_router = 0;
		break;
	case ND_REDIRECT:
		/*
		 * If the icmp is a redirect to a better router, always set the
		 * is_router flag.  Otherwise, if the entry is newly created,
		 * clear the flag.  [RFC 2461, sec 8.3]
		 */
		if (code == ND_REDIRECT_ROUTER)
			ln->ln_router = 1;
		else if (is_newentry) /* (6-7) */
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
		if ((!is_newentry && (olladdr || lladdr)) ||	/* (2-5) */
		    (is_newentry && lladdr)) {			/* (7) */
			ln->ln_router = 1;
		}
		break;
	}

	if (ln != NULL) {
		static_route = (ln->la_flags & LLE_STATIC);
		router = ln->ln_router;

		if (flags & ND6_EXCLUSIVE)
			LLE_WUNLOCK(ln);
		else
			LLE_RUNLOCK(ln);
		if (static_route)
			ln = NULL;
	}
	if (chain)
		nd6_output_flush(ifp, ifp, chain, &sin6, NULL);
	
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
	if (do_update && router && !V_ip6_forwarding &&
	    ND_IFINFO(ifp)->flags & ND6_IFF_ACCEPT_RTADV) {
		/*
		 * guaranteed recursion
		 */
		defrouter_select();
	}
	
	return (ln);
done:	
	if (ln != NULL) {
		if (flags & ND6_EXCLUSIVE)
			LLE_WUNLOCK(ln);
		else
			LLE_RUNLOCK(ln);
		if (static_route)
			ln = NULL;
	}
	return (ln);
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
	for (ifp = TAILQ_FIRST(&V_ifnet); ifp;
	    ifp = TAILQ_NEXT(ifp, if_list)) {
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

int
nd6_output(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m0,
    struct sockaddr_in6 *dst, struct rtentry *rt0)
{

	return (nd6_output_lle(ifp, origifp, m0, dst, rt0, NULL, NULL));
}


/*
 * Note that I'm not enforcing any global serialization
 * lle state or asked changes here as the logic is too
 * complicated to avoid having to always acquire an exclusive
 * lock
 * KMM
 *
 */
#define senderr(e) { error = (e); goto bad;}

int
nd6_output_lle(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m0,
    struct sockaddr_in6 *dst, struct rtentry *rt0, struct llentry *lle,
	struct mbuf **chain)
{
	struct mbuf *m = m0;
	struct m_tag *mtag;
	struct llentry *ln = lle;
	struct ip6_hdr *ip6;
	int error = 0;
	int flags = 0;
	int ip6len;

#ifdef INVARIANTS
	if (lle != NULL) {
		
		LLE_WLOCK_ASSERT(lle);

		KASSERT(chain != NULL, (" lle locked but no mbuf chain pointer passed"));
	}
#endif
	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
		goto sendpkt;

	if (nd6_need_cache(ifp) == 0)
		goto sendpkt;

	/*
	 * next hop determination.  This routine is derived from ether_output.
	 */

	/*
	 * Address resolution or Neighbor Unreachability Detection
	 * for the next hop.
	 * At this point, the destination of the packet must be a unicast
	 * or an anycast address(i.e. not a multicast).
	 */

	flags = ((m != NULL) || (lle != NULL)) ? LLE_EXCLUSIVE : 0;
	if (ln == NULL) {
	retry:
		IF_AFDATA_LOCK(ifp);
		ln = lla_lookup(LLTABLE6(ifp), flags, (struct sockaddr *)dst);
		IF_AFDATA_UNLOCK(ifp);
		if ((ln == NULL) && nd6_is_addr_neighbor(dst, ifp))  {
			/*
			 * Since nd6_is_addr_neighbor() internally calls nd6_lookup(),
			 * the condition below is not very efficient.  But we believe
			 * it is tolerable, because this should be a rare case.
			 */
			flags = ND6_CREATE | (m ? ND6_EXCLUSIVE : 0);
			IF_AFDATA_LOCK(ifp);
			ln = nd6_lookup(&dst->sin6_addr, flags, ifp);
			IF_AFDATA_UNLOCK(ifp);
		}
	} 
	if (ln == NULL) {
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0 &&
		    !(ND_IFINFO(ifp)->flags & ND6_IFF_PERFORMNUD)) {
			char ip6buf[INET6_ADDRSTRLEN];
			log(LOG_DEBUG,
			    "nd6_output: can't allocate llinfo for %s "
			    "(ln=%p)\n",
			    ip6_sprintf(ip6buf, &dst->sin6_addr), ln);
			senderr(EIO);	/* XXX: good error? */
		}
		goto sendpkt;	/* send anyway */
	}

	/* We don't have to do link-layer address resolution on a p2p link. */
	if ((ifp->if_flags & IFF_POINTOPOINT) != 0 &&
	    ln->ln_state < ND6_LLINFO_REACHABLE) {
		if ((flags & LLE_EXCLUSIVE) == 0) {
			flags |= LLE_EXCLUSIVE;
			goto retry;
		}
		ln->ln_state = ND6_LLINFO_STALE;
		nd6_llinfo_settimer_locked(ln, (long)V_nd6_gctimer * hz);
	}

	/*
	 * The first time we send a packet to a neighbor whose entry is
	 * STALE, we have to change the state to DELAY and a sets a timer to
	 * expire in DELAY_FIRST_PROBE_TIME seconds to ensure do
	 * neighbor unreachability detection on expiration.
	 * (RFC 2461 7.3.3)
	 */
	if (ln->ln_state == ND6_LLINFO_STALE) {
		if ((flags & LLE_EXCLUSIVE) == 0) {
			flags |= LLE_EXCLUSIVE;
			LLE_RUNLOCK(ln);
			goto retry;
		}
		ln->la_asked = 0;
		ln->ln_state = ND6_LLINFO_DELAY;
		nd6_llinfo_settimer_locked(ln, (long)V_nd6_delay * hz);
	}

	/*
	 * If the neighbor cache entry has a state other than INCOMPLETE
	 * (i.e. its link-layer address is already resolved), just
	 * send the packet.
	 */
	if (ln->ln_state > ND6_LLINFO_INCOMPLETE)
		goto sendpkt;

	/*
	 * There is a neighbor cache entry, but no ethernet address
	 * response yet.  Append this latest packet to the end of the
	 * packet queue in the mbuf, unless the number of the packet
	 * does not exceed nd6_maxqueuelen.  When it exceeds nd6_maxqueuelen,
	 * the oldest packet in the queue will be removed.
	 */
	if (ln->ln_state == ND6_LLINFO_NOSTATE)
		ln->ln_state = ND6_LLINFO_INCOMPLETE;

	if ((flags & LLE_EXCLUSIVE) == 0) {
		flags |= LLE_EXCLUSIVE;
		LLE_RUNLOCK(ln);
		goto retry;
	}

	LLE_WLOCK_ASSERT(ln);

	if (ln->la_hold) {
		struct mbuf *m_hold;
		int i;
		
		i = 0;
		for (m_hold = ln->la_hold; m_hold; m_hold = m_hold->m_nextpkt) {
			i++;
			if (m_hold->m_nextpkt == NULL) {
				m_hold->m_nextpkt = m;
				break;
			}
		}
		while (i >= V_nd6_maxqueuelen) {
			m_hold = ln->la_hold;
			ln->la_hold = ln->la_hold->m_nextpkt;
			m_freem(m_hold);
			i--;
		}
	} else {
		ln->la_hold = m;
	}

	/*
	 * If there has been no NS for the neighbor after entering the
	 * INCOMPLETE state, send the first solicitation.
	 */
	if (!ND6_LLINFO_PERMANENT(ln) && ln->la_asked == 0) {
		ln->la_asked++;
		
		nd6_llinfo_settimer_locked(ln,
		    (long)ND_IFINFO(ifp)->retrans * hz / 1000);
		LLE_WUNLOCK(ln);
		nd6_ns_output(ifp, NULL, &dst->sin6_addr, ln, 0);
		if (lle != NULL && ln == lle)
			LLE_WLOCK(lle);

	} else if (lle == NULL || ln != lle) {
		/*
		 * We did the lookup (no lle arg) so we
		 * need to do the unlock here.
		 */
		LLE_WUNLOCK(ln);
	}

	return (0);

  sendpkt:
	/* discard the packet if IPv6 operation is disabled on the interface */
	if ((ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED)) {
		error = ENETDOWN; /* better error? */
		goto bad;
	}
	/*
	 * ln is valid and the caller did not pass in 
	 * an llentry
	 */
	if ((ln != NULL) && (lle == NULL)) {
		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(ln);
		else
			LLE_RUNLOCK(ln);
	}

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

	/*
	 * We were passed in a pointer to an lle with the lock held 
	 * this means that we can't call if_output as we will
	 * recurse on the lle lock - so what we do is we create
	 * a list of mbufs to send and transmit them in the caller
	 * after the lock is dropped
	 */
	if (lle != NULL) {
		if (*chain == NULL)
			*chain = m;
		else {
			struct mbuf *m = *chain;

			/*
			 * append mbuf to end of deferred chain
			 */
			while (m->m_nextpkt != NULL)
				m = m->m_nextpkt;
			m->m_nextpkt = m;
		}
		return (error);
	}
	if ((ifp->if_flags & IFF_LOOPBACK) != 0) {
		return ((*ifp->if_output)(origifp, m, (struct sockaddr *)dst,
		    NULL));
	}
	error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst, NULL);
	return (error);

  bad:
	/*
	 * ln is valid and the caller did not pass in 
	 * an llentry
	 */
	if ((ln != NULL) && (lle == NULL)) {
		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(ln);
		else
			LLE_RUNLOCK(ln);
	}
	if (m)
		m_freem(m);
	return (error);
}
#undef senderr


int
nd6_output_flush(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *chain,
    struct sockaddr_in6 *dst, struct route *ro)
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
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst, ro);			       
	}

	/*
	 * XXX
	 * note that intermediate errors are blindly ignored - but this is 
	 * the same convention as used with nd6_output when called by
	 * nd6_cache_lladdr
	 */
	return (error);
}	


int
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
#ifdef IFT_L2VLAN
	case IFT_L2VLAN:
#endif
#ifdef IFT_IEEE80211
	case IFT_IEEE80211:
#endif
#ifdef IFT_CARP
	case IFT_CARP:
#endif
	case IFT_GIF:		/* XXX need more cases? */
	case IFT_PPP:
	case IFT_TUNNEL:
	case IFT_BRIDGE:
	case IFT_PROPVIRTUAL:
		return (1);
	default:
		return (0);
	}
}

/*
 * the callers of this function need to be re-worked to drop
 * the lle lock, drop here for now
 */
int
nd6_storelladdr(struct ifnet *ifp, struct mbuf *m,
    struct sockaddr *dst, u_char *desten, struct llentry **lle)
{
	struct llentry *ln;

	*lle = NULL;
	IF_AFDATA_UNLOCK_ASSERT(ifp);
	if (m->m_flags & M_MCAST) {
		int i;

		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_FDDI:
#ifdef IFT_L2VLAN
		case IFT_L2VLAN:
#endif
#ifdef IFT_IEEE80211
		case IFT_IEEE80211:
#endif
		case IFT_BRIDGE:
		case IFT_ISO88025:
			ETHER_MAP_IPV6_MULTICAST(&SIN6(dst)->sin6_addr,
						 desten);
			return (0);
		case IFT_IEEE1394:
			/*
			 * netbsd can use if_broadcastaddr, but we don't do so
			 * to reduce # of ifdef.
			 */
			for (i = 0; i < ifp->if_addrlen; i++)
				desten[i] = ~0;
			return (0);
		case IFT_ARCNET:
			*desten = 0;
			return (0);
		default:
			m_freem(m);
			return (EAFNOSUPPORT);
		}
	}


	/*
	 * the entry should have been created in nd6_store_lladdr
	 */
	IF_AFDATA_LOCK(ifp);
	ln = lla_lookup(LLTABLE6(ifp), 0, dst);
	IF_AFDATA_UNLOCK(ifp);
	if ((ln == NULL) || !(ln->la_flags & LLE_VALID)) {
		if (ln != NULL)
			LLE_RUNLOCK(ln);
		/* this could happen, if we could not allocate memory */
		m_freem(m);
		return (1);
	}

	bcopy(&ln->ll_addr, desten, ifp->if_addrlen);
	*lle = ln;
	LLE_RUNLOCK(ln);
	/*
	 * A *small* use after free race exists here
	 */
	return (0);
}

static void 
clear_llinfo_pqueue(struct llentry *ln)
{
	struct mbuf *m_hold, *m_hold_next;

	for (m_hold = ln->la_hold; m_hold; m_hold = m_hold_next) {
		m_hold_next = m_hold->m_nextpkt;
		m_hold->m_nextpkt = NULL;
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
SYSCTL_VNET_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_MAXQLEN, nd6_maxqueuelen,
	CTLFLAG_RW, &VNET_NAME(nd6_maxqueuelen), 1, "");

static int
nd6_sysctl_drlist(SYSCTL_HANDLER_ARGS)
{
	int error;
	char buf[1024] __aligned(4);
	struct in6_defrouter *d, *de;
	struct nd_defrouter *dr;

	if (req->newptr)
		return EPERM;
	error = 0;

	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		d = (struct in6_defrouter *)buf;
		de = (struct in6_defrouter *)(buf + sizeof(buf));

		if (d + 1 <= de) {
			bzero(d, sizeof(*d));
			d->rtaddr.sin6_family = AF_INET6;
			d->rtaddr.sin6_len = sizeof(d->rtaddr);
			d->rtaddr.sin6_addr = dr->rtaddr;
			error = sa6_recoverscope(&d->rtaddr);
			if (error != 0)
				return (error);
			d->flags = dr->flags;
			d->rtlifetime = dr->rtlifetime;
			d->expire = dr->expire;
			d->if_index = dr->ifp->if_index;
		} else
			panic("buffer too short");

		error = SYSCTL_OUT(req, buf, sizeof(*d));
		if (error)
			break;
	}

	return (error);
}

static int
nd6_sysctl_prlist(SYSCTL_HANDLER_ARGS)
{
	int error;
	char buf[1024] __aligned(4);
	struct in6_prefix *p, *pe;
	struct nd_prefix *pr;
	char ip6buf[INET6_ADDRSTRLEN];

	if (req->newptr)
		return EPERM;
	error = 0;

	for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		u_short advrtrs;
		size_t advance;
		struct sockaddr_in6 *sin6, *s6;
		struct nd_pfxrouter *pfr;

		p = (struct in6_prefix *)buf;
		pe = (struct in6_prefix *)(buf + sizeof(buf));

		if (p + 1 <= pe) {
			bzero(p, sizeof(*p));
			sin6 = (struct sockaddr_in6 *)(p + 1);

			p->prefix = pr->ndpr_prefix;
			if (sa6_recoverscope(&p->prefix)) {
				log(LOG_ERR,
				    "scope error in prefix list (%s)\n",
				    ip6_sprintf(ip6buf, &p->prefix.sin6_addr));
				/* XXX: press on... */
			}
			p->raflags = pr->ndpr_raf;
			p->prefixlen = pr->ndpr_plen;
			p->vltime = pr->ndpr_vltime;
			p->pltime = pr->ndpr_pltime;
			p->if_index = pr->ndpr_ifp->if_index;
			if (pr->ndpr_vltime == ND6_INFINITE_LIFETIME)
				p->expire = 0;
			else {
				time_t maxexpire;

				/* XXX: we assume time_t is signed. */
				maxexpire = (-1) &
				    ~((time_t)1 <<
				    ((sizeof(maxexpire) * 8) - 1));
				if (pr->ndpr_vltime <
				    maxexpire - pr->ndpr_lastupdate) {
				    p->expire = pr->ndpr_lastupdate +
				        pr->ndpr_vltime;
				} else
					p->expire = maxexpire;
			}
			p->refcnt = pr->ndpr_refcnt;
			p->flags = pr->ndpr_stateflags;
			p->origin = PR_ORIG_RA;
			advrtrs = 0;
			for (pfr = pr->ndpr_advrtrs.lh_first; pfr;
			     pfr = pfr->pfr_next) {
				if ((void *)&sin6[advrtrs + 1] > (void *)pe) {
					advrtrs++;
					continue;
				}
				s6 = &sin6[advrtrs];
				bzero(s6, sizeof(*s6));
				s6->sin6_family = AF_INET6;
				s6->sin6_len = sizeof(*sin6);
				s6->sin6_addr = pfr->router->rtaddr;
				if (sa6_recoverscope(s6)) {
					log(LOG_ERR,
					    "scope error in "
					    "prefix list (%s)\n",
					    ip6_sprintf(ip6buf,
						    &pfr->router->rtaddr));
				}
				advrtrs++;
			}
			p->advrtrs = advrtrs;
		} else
			panic("buffer too short");

		advance = sizeof(*p) + sizeof(*sin6) * advrtrs;
		error = SYSCTL_OUT(req, buf, advance);
		if (error)
			break;
	}

	return (error);
}

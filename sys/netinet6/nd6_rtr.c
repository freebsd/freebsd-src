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
 *	$KAME: nd6_rtr.c,v 1.111 2001/04/27 01:37:15 jinmei Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/errno.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/radix.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <net/if_llatbl.h>
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>
#include <netinet6/scope6_var.h>

static int rtpref(struct nd_defrouter *);
static struct nd_defrouter *defrtrlist_update(struct nd_defrouter *);
static int prelist_update __P((struct nd_prefixctl *, struct nd_defrouter *,
    struct mbuf *, int));
static struct in6_ifaddr *in6_ifadd(struct nd_prefixctl *,	int);
static struct nd_pfxrouter *pfxrtr_lookup __P((struct nd_prefix *,
	struct nd_defrouter *));
static void pfxrtr_add(struct nd_prefix *, struct nd_defrouter *);
static void pfxrtr_del(struct nd_pfxrouter *);
static struct nd_pfxrouter *find_pfxlist_reachable_router
(struct nd_prefix *);
static void defrouter_delreq(struct nd_defrouter *);
static void nd6_rtmsg(int, struct rtentry *);

static int in6_init_prefix_ltimes(struct nd_prefix *);
static void in6_init_address_ltimes __P((struct nd_prefix *,
	struct in6_addrlifetime *));

static int rt6_deleteroute(struct radix_node *, void *);

VNET_DECLARE(int, nd6_recalc_reachtm_interval);
#define	V_nd6_recalc_reachtm_interval	VNET(nd6_recalc_reachtm_interval)

static VNET_DEFINE(struct ifnet *, nd6_defifp);
#define	V_nd6_defifp			VNET(nd6_defifp)

VNET_DEFINE(int, nd6_defifindex);
VNET_DEFINE(int, ip6_use_tempaddr);
VNET_DEFINE(int, ip6_desync_factor);
VNET_DEFINE(u_int32_t, ip6_temp_preferred_lifetime);
VNET_DEFINE(u_int32_t, ip6_temp_valid_lifetime);
VNET_DEFINE(int, ip6_temp_regen_advance);

/* RTPREF_MEDIUM has to be 0! */
#define RTPREF_HIGH	1
#define RTPREF_MEDIUM	0
#define RTPREF_LOW	(-1)
#define RTPREF_RESERVED	(-2)
#define RTPREF_INVALID	(-3)	/* internal */

/*
 * Receive Router Solicitation Message - just for routers.
 * Router solicitation/advertisement is mostly managed by userland program
 * (rtadvd) so here we have no function like nd6_ra_output().
 *
 * Based on RFC 2461
 */
void
nd6_rs_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_solicit *nd_rs;
	struct in6_addr saddr6 = ip6->ip6_src;
	char *lladdr = NULL;
	int lladdrlen = 0;
	union nd_opts ndopts;
	char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];

	/* If I'm not a router, ignore it. */
	if (!V_ip6_forwarding)
		goto freeit;

	/* Sanity checks */
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_rs_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim, ip6_sprintf(ip6bufs, &ip6->ip6_src),
		    ip6_sprintf(ip6bufd, &ip6->ip6_dst), if_name(ifp)));
		goto bad;
	}

	/*
	 * Don't update the neighbor cache, if src = ::.
	 * This indicates that the src has no IP address assigned yet.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&saddr6))
		goto freeit;

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, icmp6len,);
	nd_rs = (struct nd_router_solicit *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(nd_rs, struct nd_router_solicit *, m, off, icmp6len);
	if (nd_rs == NULL) {
		ICMP6STAT_INC(icp6s_tooshort);
		return;
	}
#endif

	icmp6len -= sizeof(*nd_rs);
	nd6_option_init(nd_rs + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: lladdrlen mismatch for %s "
		    "(if %d, RS packet %d)\n",
		    ip6_sprintf(ip6bufs, &saddr6),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_SOLICIT, 0);

 freeit:
	m_freem(m);
	return;

 bad:
	ICMP6STAT_INC(icp6s_badrs);
	m_freem(m);
}

/*
 * Receive Router Advertisement Message.
 *
 * Based on RFC 2461
 * TODO: on-link bit on prefix information
 * TODO: ND_RA_FLAG_{OTHER,MANAGED} processing
 */
void
nd6_ra_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_advert *nd_ra;
	struct in6_addr saddr6 = ip6->ip6_src;
	int mcast = 0;
	union nd_opts ndopts;
	struct nd_defrouter *dr;
	char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];

	/*
	 * We only accept RAs only when
	 * the node is not a router and
	 * per-interface variable allows RAs on the receiving interface.
	 */
	if (V_ip6_forwarding || !(ndi->flags & ND6_IFF_ACCEPT_RTADV))
		goto freeit;

	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim, ip6_sprintf(ip6bufs, &ip6->ip6_src),
		    ip6_sprintf(ip6bufd, &ip6->ip6_dst), if_name(ifp)));
		goto bad;
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&saddr6)) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: src %s is not link-local\n",
		    ip6_sprintf(ip6bufs, &saddr6)));
		goto bad;
	}

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, icmp6len,);
	nd_ra = (struct nd_router_advert *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(nd_ra, struct nd_router_advert *, m, off, icmp6len);
	if (nd_ra == NULL) {
		ICMP6STAT_INC(icp6s_tooshort);
		return;
	}
#endif

	icmp6len -= sizeof(*nd_ra);
	nd6_option_init(nd_ra + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

    {
	struct nd_defrouter dr0;
	u_int32_t advreachable = nd_ra->nd_ra_reachable;

	/* remember if this is a multicasted advertisement */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst))
		mcast = 1;

	bzero(&dr0, sizeof(dr0));
	dr0.rtaddr = saddr6;
	dr0.flags  = nd_ra->nd_ra_flags_reserved;
	dr0.rtlifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	dr0.expire = time_second + dr0.rtlifetime;
	dr0.ifp = ifp;
	/* unspecified or not? (RFC 2461 6.3.4) */
	if (advreachable) {
		advreachable = ntohl(advreachable);
		if (advreachable <= MAX_REACHABLE_TIME &&
		    ndi->basereachable != advreachable) {
			ndi->basereachable = advreachable;
			ndi->reachable = ND_COMPUTE_RTIME(ndi->basereachable);
			ndi->recalctm = V_nd6_recalc_reachtm_interval; /* reset */
		}
	}
	if (nd_ra->nd_ra_retransmit)
		ndi->retrans = ntohl(nd_ra->nd_ra_retransmit);
	if (nd_ra->nd_ra_curhoplimit)
		ndi->chlim = nd_ra->nd_ra_curhoplimit;
	dr = defrtrlist_update(&dr0);
    }

	/*
	 * prefix
	 */
	if (ndopts.nd_opts_pi) {
		struct nd_opt_hdr *pt;
		struct nd_opt_prefix_info *pi = NULL;
		struct nd_prefixctl pr;

		for (pt = (struct nd_opt_hdr *)ndopts.nd_opts_pi;
		     pt <= (struct nd_opt_hdr *)ndopts.nd_opts_pi_end;
		     pt = (struct nd_opt_hdr *)((caddr_t)pt +
						(pt->nd_opt_len << 3))) {
			if (pt->nd_opt_type != ND_OPT_PREFIX_INFORMATION)
				continue;
			pi = (struct nd_opt_prefix_info *)pt;

			if (pi->nd_opt_pi_len != 4) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid option "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_len));
				continue;
			}

			if (128 < pi->nd_opt_pi_prefix_len) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_prefix_len));
				continue;
			}

			if (IN6_IS_ADDR_MULTICAST(&pi->nd_opt_pi_prefix)
			 || IN6_IS_ADDR_LINKLOCAL(&pi->nd_opt_pi_prefix)) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "%s, ignored\n",
				    ip6_sprintf(ip6bufs,
					&pi->nd_opt_pi_prefix)));
				continue;
			}

			bzero(&pr, sizeof(pr));
			pr.ndpr_prefix.sin6_family = AF_INET6;
			pr.ndpr_prefix.sin6_len = sizeof(pr.ndpr_prefix);
			pr.ndpr_prefix.sin6_addr = pi->nd_opt_pi_prefix;
			pr.ndpr_ifp = (struct ifnet *)m->m_pkthdr.rcvif;

			pr.ndpr_raf_onlink = (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK) ? 1 : 0;
			pr.ndpr_raf_auto = (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_AUTO) ? 1 : 0;
			pr.ndpr_plen = pi->nd_opt_pi_prefix_len;
			pr.ndpr_vltime = ntohl(pi->nd_opt_pi_valid_time);
			pr.ndpr_pltime = ntohl(pi->nd_opt_pi_preferred_time);
			(void)prelist_update(&pr, dr, m, mcast);
		}
	}

	/*
	 * MTU
	 */
	if (ndopts.nd_opts_mtu && ndopts.nd_opts_mtu->nd_opt_mtu_len == 1) {
		u_long mtu;
		u_long maxmtu;

		mtu = (u_long)ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);

		/* lower bound */
		if (mtu < IPV6_MMTU) {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu option "
			    "mtu=%lu sent from %s, ignoring\n",
			    mtu, ip6_sprintf(ip6bufs, &ip6->ip6_src)));
			goto skip;
		}

		/* upper bound */
		maxmtu = (ndi->maxmtu && ndi->maxmtu < ifp->if_mtu)
		    ? ndi->maxmtu : ifp->if_mtu;
		if (mtu <= maxmtu) {
			int change = (ndi->linkmtu != mtu);

			ndi->linkmtu = mtu;
			if (change) /* in6_maxmtu may change */
				in6_setmaxmtu();
		} else {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu "
			    "mtu=%lu sent from %s; "
			    "exceeds maxmtu %lu, ignoring\n",
			    mtu, ip6_sprintf(ip6bufs, &ip6->ip6_src), maxmtu));
		}
	}

 skip:

	/*
	 * Source link layer address
	 */
    {
	char *lladdr = NULL;
	int lladdrlen = 0;

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: lladdrlen mismatch for %s "
		    "(if %d, RA packet %d)\n", ip6_sprintf(ip6bufs, &saddr6),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr,
	    lladdrlen, ND_ROUTER_ADVERT, 0);

	/*
	 * Installing a link-layer address might change the state of the
	 * router's neighbor cache, which might also affect our on-link
	 * detection of adveritsed prefixes.
	 */
	pfxlist_onlink_check();
    }

 freeit:
	m_freem(m);
	return;

 bad:
	ICMP6STAT_INC(icp6s_badra);
	m_freem(m);
}

/*
 * default router list proccessing sub routines
 */

/* tell the change to user processes watching the routing socket. */
static void
nd6_rtmsg(int cmd, struct rtentry *rt)
{
	struct rt_addrinfo info;
	struct ifnet *ifp;
	struct ifaddr *ifa;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	ifp = rt->rt_ifp;
	if (ifp != NULL) {
		IF_ADDR_LOCK(ifp);
		ifa = TAILQ_FIRST(&ifp->if_addrhead);
		info.rti_info[RTAX_IFP] = ifa->ifa_addr;
		ifa_ref(ifa);
		IF_ADDR_UNLOCK(ifp);
		info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
	} else
		ifa = NULL;

	rt_missmsg(cmd, &info, rt->rt_flags, 0);
	if (ifa != NULL)
		ifa_free(ifa);
}

void
defrouter_addreq(struct nd_defrouter *new)
{
	struct sockaddr_in6 def, mask, gate;
	struct rtentry *newrt = NULL;
	int s;
	int error;

	bzero(&def, sizeof(def));
	bzero(&mask, sizeof(mask));
	bzero(&gate, sizeof(gate));

	def.sin6_len = mask.sin6_len = gate.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6_family = gate.sin6_family = AF_INET6;
	gate.sin6_addr = new->rtaddr;

	s = splnet();
	error = rtrequest(RTM_ADD, (struct sockaddr *)&def,
	    (struct sockaddr *)&gate, (struct sockaddr *)&mask,
	    RTF_GATEWAY, &newrt);
	if (newrt) {
		nd6_rtmsg(RTM_ADD, newrt); /* tell user process */
		RTFREE(newrt);
	}
	if (error == 0)
		new->installed = 1;
	splx(s);
	return;
}

struct nd_defrouter *
defrouter_lookup(struct in6_addr *addr, struct ifnet *ifp)
{
	struct nd_defrouter *dr;

	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		if (dr->ifp == ifp && IN6_ARE_ADDR_EQUAL(addr, &dr->rtaddr))
			return (dr);
	}

	return (NULL);		/* search failed */
}

/*
 * Remove the default route for a given router.
 * This is just a subroutine function for defrouter_select(), and should
 * not be called from anywhere else.
 */
static void
defrouter_delreq(struct nd_defrouter *dr)
{
	struct sockaddr_in6 def, mask, gate;
	struct rtentry *oldrt = NULL;

	bzero(&def, sizeof(def));
	bzero(&mask, sizeof(mask));
	bzero(&gate, sizeof(gate));

	def.sin6_len = mask.sin6_len = gate.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6_family = gate.sin6_family = AF_INET6;
	gate.sin6_addr = dr->rtaddr;

	rtrequest(RTM_DELETE, (struct sockaddr *)&def,
	    (struct sockaddr *)&gate,
	    (struct sockaddr *)&mask, RTF_GATEWAY, &oldrt);
	if (oldrt) {
		nd6_rtmsg(RTM_DELETE, oldrt);
		RTFREE(oldrt);
	}

	dr->installed = 0;
}

/*
 * remove all default routes from default router list
 */
void
defrouter_reset(void)
{
	struct nd_defrouter *dr;

	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry))
		defrouter_delreq(dr);

	/*
	 * XXX should we also nuke any default routers in the kernel, by
	 * going through them by rtalloc1()?
	 */
}

void
defrtrlist_del(struct nd_defrouter *dr)
{
	struct nd_defrouter *deldr = NULL;
	struct nd_prefix *pr;

	/*
	 * Flush all the routing table entries that use the router
	 * as a next hop.
	 */
	if (!V_ip6_forwarding)
		rt6_flush(&dr->rtaddr, dr->ifp);

	if (dr->installed) {
		deldr = dr;
		defrouter_delreq(dr);
	}
	TAILQ_REMOVE(&V_nd_defrouter, dr, dr_entry);

	/*
	 * Also delete all the pointers to the router in each prefix lists.
	 */
	for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		struct nd_pfxrouter *pfxrtr;
		if ((pfxrtr = pfxrtr_lookup(pr, dr)) != NULL)
			pfxrtr_del(pfxrtr);
	}
	pfxlist_onlink_check();

	/*
	 * If the router is the primary one, choose a new one.
	 * Note that defrouter_select() will remove the current gateway
	 * from the routing table.
	 */
	if (deldr)
		defrouter_select();

	free(dr, M_IP6NDP);
}

/*
 * Default Router Selection according to Section 6.3.6 of RFC 2461 and
 * draft-ietf-ipngwg-router-selection:
 * 1) Routers that are reachable or probably reachable should be preferred.
 *    If we have more than one (probably) reachable router, prefer ones
 *    with the highest router preference.
 * 2) When no routers on the list are known to be reachable or
 *    probably reachable, routers SHOULD be selected in a round-robin
 *    fashion, regardless of router preference values.
 * 3) If the Default Router List is empty, assume that all
 *    destinations are on-link.
 *
 * We assume nd_defrouter is sorted by router preference value.
 * Since the code below covers both with and without router preference cases,
 * we do not need to classify the cases by ifdef.
 *
 * At this moment, we do not try to install more than one default router,
 * even when the multipath routing is available, because we're not sure about
 * the benefits for stub hosts comparing to the risk of making the code
 * complicated and the possibility of introducing bugs.
 */
void
defrouter_select(void)
{
	int s = splnet();
	struct nd_defrouter *dr, *selected_dr = NULL, *installed_dr = NULL;
	struct llentry *ln = NULL;

	/*
	 * This function should be called only when acting as an autoconfigured
	 * host.  Although the remaining part of this function is not effective
	 * if the node is not an autoconfigured host, we explicitly exclude
	 * such cases here for safety.
	 */
	if (V_ip6_forwarding) {
		nd6log((LOG_WARNING,
		    "defrouter_select: called unexpectedly (forwarding=%d)\n",
		    V_ip6_forwarding));
		splx(s);
		return;
	}

	/*
	 * Let's handle easy case (3) first:
	 * If default router list is empty, there's nothing to be done.
	 */
	if (!TAILQ_FIRST(&V_nd_defrouter)) {
		splx(s);
		return;
	}

	/*
	 * Search for a (probably) reachable router from the list.
	 * We just pick up the first reachable one (if any), assuming that
	 * the ordering rule of the list described in defrtrlist_update().
	 */
	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		IF_AFDATA_LOCK(dr->ifp);
		if (selected_dr == NULL &&
		    (ln = nd6_lookup(&dr->rtaddr, 0, dr->ifp)) &&
		    ND6_IS_LLINFO_PROBREACH(ln)) {
			selected_dr = dr;
		}
		IF_AFDATA_UNLOCK(dr->ifp);
		if (ln != NULL) {
			LLE_RUNLOCK(ln);
			ln = NULL;
		}

		if (dr->installed && installed_dr == NULL)
			installed_dr = dr;
		else if (dr->installed && installed_dr) {
			/* this should not happen.  warn for diagnosis. */
			log(LOG_ERR, "defrouter_select: more than one router"
			    " is installed\n");
		}
	}
	/*
	 * If none of the default routers was found to be reachable,
	 * round-robin the list regardless of preference.
	 * Otherwise, if we have an installed router, check if the selected
	 * (reachable) router should really be preferred to the installed one.
	 * We only prefer the new router when the old one is not reachable
	 * or when the new one has a really higher preference value.
	 */
	if (selected_dr == NULL) {
		if (installed_dr == NULL || !TAILQ_NEXT(installed_dr, dr_entry))
			selected_dr = TAILQ_FIRST(&V_nd_defrouter);
		else
			selected_dr = TAILQ_NEXT(installed_dr, dr_entry);
	} else if (installed_dr) {
		IF_AFDATA_LOCK(installed_dr->ifp);
		if ((ln = nd6_lookup(&installed_dr->rtaddr, 0, installed_dr->ifp)) &&
		    ND6_IS_LLINFO_PROBREACH(ln) &&
		    rtpref(selected_dr) <= rtpref(installed_dr)) {
			selected_dr = installed_dr;
		}
		IF_AFDATA_UNLOCK(installed_dr->ifp);
		if (ln != NULL)
			LLE_RUNLOCK(ln);
	}

	/*
	 * If the selected router is different than the installed one,
	 * remove the installed router and install the selected one.
	 * Note that the selected router is never NULL here.
	 */
	if (installed_dr != selected_dr) {
		if (installed_dr)
			defrouter_delreq(installed_dr);
		defrouter_addreq(selected_dr);
	}

	splx(s);
	return;
}

/*
 * for default router selection
 * regards router-preference field as a 2-bit signed integer
 */
static int
rtpref(struct nd_defrouter *dr)
{
	switch (dr->flags & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		return (RTPREF_HIGH);
	case ND_RA_FLAG_RTPREF_MEDIUM:
	case ND_RA_FLAG_RTPREF_RSV:
		return (RTPREF_MEDIUM);
	case ND_RA_FLAG_RTPREF_LOW:
		return (RTPREF_LOW);
	default:
		/*
		 * This case should never happen.  If it did, it would mean a
		 * serious bug of kernel internal.  We thus always bark here.
		 * Or, can we even panic?
		 */
		log(LOG_ERR, "rtpref: impossible RA flag %x\n", dr->flags);
		return (RTPREF_INVALID);
	}
	/* NOTREACHED */
}

static struct nd_defrouter *
defrtrlist_update(struct nd_defrouter *new)
{
	struct nd_defrouter *dr, *n;
	int s = splnet();

	if ((dr = defrouter_lookup(&new->rtaddr, new->ifp)) != NULL) {
		/* entry exists */
		if (new->rtlifetime == 0) {
			defrtrlist_del(dr);
			dr = NULL;
		} else {
			int oldpref = rtpref(dr);

			/* override */
			dr->flags = new->flags; /* xxx flag check */
			dr->rtlifetime = new->rtlifetime;
			dr->expire = new->expire;

			/*
			 * If the preference does not change, there's no need
			 * to sort the entries.
			 */
			if (rtpref(new) == oldpref) {
				splx(s);
				return (dr);
			}

			/*
			 * preferred router may be changed, so relocate
			 * this router.
			 * XXX: calling TAILQ_REMOVE directly is a bad manner.
			 * However, since defrtrlist_del() has many side
			 * effects, we intentionally do so here.
			 * defrouter_select() below will handle routing
			 * changes later.
			 */
			TAILQ_REMOVE(&V_nd_defrouter, dr, dr_entry);
			n = dr;
			goto insert;
		}
		splx(s);
		return (dr);
	}

	/* entry does not exist */
	if (new->rtlifetime == 0) {
		splx(s);
		return (NULL);
	}

	n = (struct nd_defrouter *)malloc(sizeof(*n), M_IP6NDP, M_NOWAIT);
	if (n == NULL) {
		splx(s);
		return (NULL);
	}
	bzero(n, sizeof(*n));
	*n = *new;

insert:
	/*
	 * Insert the new router in the Default Router List;
	 * The Default Router List should be in the descending order
	 * of router-preferece.  Routers with the same preference are
	 * sorted in the arriving time order.
	 */

	/* insert at the end of the group */
	for (dr = TAILQ_FIRST(&V_nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		if (rtpref(n) > rtpref(dr))
			break;
	}
	if (dr)
		TAILQ_INSERT_BEFORE(dr, n, dr_entry);
	else
		TAILQ_INSERT_TAIL(&V_nd_defrouter, n, dr_entry);

	defrouter_select();

	splx(s);

	return (n);
}

static struct nd_pfxrouter *
pfxrtr_lookup(struct nd_prefix *pr, struct nd_defrouter *dr)
{
	struct nd_pfxrouter *search;

	for (search = pr->ndpr_advrtrs.lh_first; search; search = search->pfr_next) {
		if (search->router == dr)
			break;
	}

	return (search);
}

static void
pfxrtr_add(struct nd_prefix *pr, struct nd_defrouter *dr)
{
	struct nd_pfxrouter *new;

	new = (struct nd_pfxrouter *)malloc(sizeof(*new), M_IP6NDP, M_NOWAIT);
	if (new == NULL)
		return;
	bzero(new, sizeof(*new));
	new->router = dr;

	LIST_INSERT_HEAD(&pr->ndpr_advrtrs, new, pfr_entry);

	pfxlist_onlink_check();
}

static void
pfxrtr_del(struct nd_pfxrouter *pfr)
{
	LIST_REMOVE(pfr, pfr_entry);
	free(pfr, M_IP6NDP);
}

struct nd_prefix *
nd6_prefix_lookup(struct nd_prefixctl *key)
{
	struct nd_prefix *search;

	for (search = V_nd_prefix.lh_first;
	    search; search = search->ndpr_next) {
		if (key->ndpr_ifp == search->ndpr_ifp &&
		    key->ndpr_plen == search->ndpr_plen &&
		    in6_are_prefix_equal(&key->ndpr_prefix.sin6_addr,
		    &search->ndpr_prefix.sin6_addr, key->ndpr_plen)) {
			break;
		}
	}

	return (search);
}

int
nd6_prelist_add(struct nd_prefixctl *pr, struct nd_defrouter *dr,
    struct nd_prefix **newp)
{
	struct nd_prefix *new = NULL;
	int error = 0;
	int i, s;
	char ip6buf[INET6_ADDRSTRLEN];

	new = (struct nd_prefix *)malloc(sizeof(*new), M_IP6NDP, M_NOWAIT);
	if (new == NULL)
		return(ENOMEM);
	bzero(new, sizeof(*new));
	new->ndpr_ifp = pr->ndpr_ifp;
	new->ndpr_prefix = pr->ndpr_prefix;
	new->ndpr_plen = pr->ndpr_plen;
	new->ndpr_vltime = pr->ndpr_vltime;
	new->ndpr_pltime = pr->ndpr_pltime;
	new->ndpr_flags = pr->ndpr_flags;
	if ((error = in6_init_prefix_ltimes(new)) != 0) {
		free(new, M_IP6NDP);
		return(error);
	}
	new->ndpr_lastupdate = time_second;
	if (newp != NULL)
		*newp = new;

	/* initialization */
	LIST_INIT(&new->ndpr_advrtrs);
	in6_prefixlen2mask(&new->ndpr_mask, new->ndpr_plen);
	/* make prefix in the canonical form */
	for (i = 0; i < 4; i++)
		new->ndpr_prefix.sin6_addr.s6_addr32[i] &=
		    new->ndpr_mask.s6_addr32[i];

	s = splnet();
	/* link ndpr_entry to nd_prefix list */
	LIST_INSERT_HEAD(&V_nd_prefix, new, ndpr_entry);
	splx(s);

	/* ND_OPT_PI_FLAG_ONLINK processing */
	if (new->ndpr_raf_onlink) {
		int e;

		if ((e = nd6_prefix_onlink(new)) != 0) {
			nd6log((LOG_ERR, "nd6_prelist_add: failed to make "
			    "the prefix %s/%d on-link on %s (errno=%d)\n",
			    ip6_sprintf(ip6buf, &pr->ndpr_prefix.sin6_addr),
			    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
			/* proceed anyway. XXX: is it correct? */
		}
	}

	if (dr)
		pfxrtr_add(new, dr);

	return 0;
}

void
prelist_remove(struct nd_prefix *pr)
{
	struct nd_pfxrouter *pfr, *next;
	int e, s;
	char ip6buf[INET6_ADDRSTRLEN];

	/* make sure to invalidate the prefix until it is really freed. */
	pr->ndpr_vltime = 0;
	pr->ndpr_pltime = 0;

	/*
	 * Though these flags are now meaningless, we'd rather keep the value
	 * of pr->ndpr_raf_onlink and pr->ndpr_raf_auto not to confuse users
	 * when executing "ndp -p".
	 */

	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0 &&
	    (e = nd6_prefix_offlink(pr)) != 0) {
		nd6log((LOG_ERR, "prelist_remove: failed to make %s/%d offlink "
		    "on %s, errno=%d\n",
		    ip6_sprintf(ip6buf, &pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
		/* what should we do? */
	}

	if (pr->ndpr_refcnt > 0)
		return;		/* notice here? */

	s = splnet();

	/* unlink ndpr_entry from nd_prefix list */
	LIST_REMOVE(pr, ndpr_entry);

	/* free list of routers that adversed the prefix */
	for (pfr = pr->ndpr_advrtrs.lh_first; pfr; pfr = next) {
		next = pfr->pfr_next;

		free(pfr, M_IP6NDP);
	}
	splx(s);

	free(pr, M_IP6NDP);

	pfxlist_onlink_check();
}

/*
 * dr - may be NULL
 */

static int
prelist_update(struct nd_prefixctl *new, struct nd_defrouter *dr,
    struct mbuf *m, int mcast)
{
	struct in6_ifaddr *ia6 = NULL, *ia6_match = NULL;
	struct ifaddr *ifa;
	struct ifnet *ifp = new->ndpr_ifp;
	struct nd_prefix *pr;
	int s = splnet();
	int error = 0;
	int newprefix = 0;
	int auth;
	struct in6_addrlifetime lt6_tmp;
	char ip6buf[INET6_ADDRSTRLEN];

	auth = 0;
	if (m) {
		/*
		 * Authenticity for NA consists authentication for
		 * both IP header and IP datagrams, doesn't it ?
		 */
#if defined(M_AUTHIPHDR) && defined(M_AUTHIPDGM)
		auth = ((m->m_flags & M_AUTHIPHDR) &&
		    (m->m_flags & M_AUTHIPDGM));
#endif
	}

	if ((pr = nd6_prefix_lookup(new)) != NULL) {
		/*
		 * nd6_prefix_lookup() ensures that pr and new have the same
		 * prefix on a same interface.
		 */

		/*
		 * Update prefix information.  Note that the on-link (L) bit
		 * and the autonomous (A) bit should NOT be changed from 1
		 * to 0.
		 */
		if (new->ndpr_raf_onlink == 1)
			pr->ndpr_raf_onlink = 1;
		if (new->ndpr_raf_auto == 1)
			pr->ndpr_raf_auto = 1;
		if (new->ndpr_raf_onlink) {
			pr->ndpr_vltime = new->ndpr_vltime;
			pr->ndpr_pltime = new->ndpr_pltime;
			(void)in6_init_prefix_ltimes(pr); /* XXX error case? */
			pr->ndpr_lastupdate = time_second;
		}

		if (new->ndpr_raf_onlink &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
			int e;

			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "prelist_update: failed to make "
				    "the prefix %s/%d on-link on %s "
				    "(errno=%d)\n",
				    ip6_sprintf(ip6buf,
					    &pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
				/* proceed anyway. XXX: is it correct? */
			}
		}

		if (dr && pfxrtr_lookup(pr, dr) == NULL)
			pfxrtr_add(pr, dr);
	} else {
		struct nd_prefix *newpr = NULL;

		newprefix = 1;

		if (new->ndpr_vltime == 0)
			goto end;
		if (new->ndpr_raf_onlink == 0 && new->ndpr_raf_auto == 0)
			goto end;

		error = nd6_prelist_add(new, dr, &newpr);
		if (error != 0 || newpr == NULL) {
			nd6log((LOG_NOTICE, "prelist_update: "
			    "nd6_prelist_add failed for %s/%d on %s "
			    "errno=%d, returnpr=%p\n",
			    ip6_sprintf(ip6buf, &new->ndpr_prefix.sin6_addr),
			    new->ndpr_plen, if_name(new->ndpr_ifp),
			    error, newpr));
			goto end; /* we should just give up in this case. */
		}

		/*
		 * XXX: from the ND point of view, we can ignore a prefix
		 * with the on-link bit being zero.  However, we need a
		 * prefix structure for references from autoconfigured
		 * addresses.  Thus, we explicitly make sure that the prefix
		 * itself expires now.
		 */
		if (newpr->ndpr_raf_onlink == 0) {
			newpr->ndpr_vltime = 0;
			newpr->ndpr_pltime = 0;
			in6_init_prefix_ltimes(newpr);
		}

		pr = newpr;
	}

	/*
	 * Address autoconfiguration based on Section 5.5.3 of RFC 2462.
	 * Note that pr must be non NULL at this point.
	 */

	/* 5.5.3 (a). Ignore the prefix without the A bit set. */
	if (!new->ndpr_raf_auto)
		goto end;

	/*
	 * 5.5.3 (b). the link-local prefix should have been ignored in
	 * nd6_ra_input.
	 */

	/* 5.5.3 (c). Consistency check on lifetimes: pltime <= vltime. */
	if (new->ndpr_pltime > new->ndpr_vltime) {
		error = EINVAL;	/* XXX: won't be used */
		goto end;
	}

	/*
	 * 5.5.3 (d).  If the prefix advertised is not equal to the prefix of
	 * an address configured by stateless autoconfiguration already in the
	 * list of addresses associated with the interface, and the Valid
	 * Lifetime is not 0, form an address.  We first check if we have
	 * a matching prefix.
	 * Note: we apply a clarification in rfc2462bis-02 here.  We only
	 * consider autoconfigured addresses while RFC2462 simply said
	 * "address".
	 */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		struct in6_ifaddr *ifa6;
		u_int32_t remaininglifetime;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ifa6 = (struct in6_ifaddr *)ifa;

		/*
		 * We only consider autoconfigured addresses as per rfc2462bis.
		 */
		if (!(ifa6->ia6_flags & IN6_IFF_AUTOCONF))
			continue;

		/*
		 * Spec is not clear here, but I believe we should concentrate
		 * on unicast (i.e. not anycast) addresses.
		 * XXX: other ia6_flags? detached or duplicated?
		 */
		if ((ifa6->ia6_flags & IN6_IFF_ANYCAST) != 0)
			continue;

		/*
		 * Ignore the address if it is not associated with a prefix
		 * or is associated with a prefix that is different from this
		 * one.  (pr is never NULL here)
		 */
		if (ifa6->ia6_ndpr != pr)
			continue;

		if (ia6_match == NULL) /* remember the first one */
			ia6_match = ifa6;

		/*
		 * An already autoconfigured address matched.  Now that we
		 * are sure there is at least one matched address, we can
		 * proceed to 5.5.3. (e): update the lifetimes according to the
		 * "two hours" rule and the privacy extension.
		 * We apply some clarifications in rfc2462bis:
		 * - use remaininglifetime instead of storedlifetime as a
		 *   variable name
		 * - remove the dead code in the "two-hour" rule
		 */
#define TWOHOUR		(120*60)
		lt6_tmp = ifa6->ia6_lifetime;

		if (lt6_tmp.ia6t_vltime == ND6_INFINITE_LIFETIME)
			remaininglifetime = ND6_INFINITE_LIFETIME;
		else if (time_second - ifa6->ia6_updatetime >
			 lt6_tmp.ia6t_vltime) {
			/*
			 * The case of "invalid" address.  We should usually
			 * not see this case.
			 */
			remaininglifetime = 0;
		} else
			remaininglifetime = lt6_tmp.ia6t_vltime -
			    (time_second - ifa6->ia6_updatetime);

		/* when not updating, keep the current stored lifetime. */
		lt6_tmp.ia6t_vltime = remaininglifetime;

		if (TWOHOUR < new->ndpr_vltime ||
		    remaininglifetime < new->ndpr_vltime) {
			lt6_tmp.ia6t_vltime = new->ndpr_vltime;
		} else if (remaininglifetime <= TWOHOUR) {
			if (auth) {
				lt6_tmp.ia6t_vltime = new->ndpr_vltime;
			}
		} else {
			/*
			 * new->ndpr_vltime <= TWOHOUR &&
			 * TWOHOUR < remaininglifetime
			 */
			lt6_tmp.ia6t_vltime = TWOHOUR;
		}

		/* The 2 hour rule is not imposed for preferred lifetime. */
		lt6_tmp.ia6t_pltime = new->ndpr_pltime;

		in6_init_address_ltimes(pr, &lt6_tmp);

		/*
		 * We need to treat lifetimes for temporary addresses
		 * differently, according to
		 * draft-ietf-ipv6-privacy-addrs-v2-01.txt 3.3 (1);
		 * we only update the lifetimes when they are in the maximum
		 * intervals.
		 */
		if ((ifa6->ia6_flags & IN6_IFF_TEMPORARY) != 0) {
			u_int32_t maxvltime, maxpltime;

			if (V_ip6_temp_valid_lifetime >
			    (u_int32_t)((time_second - ifa6->ia6_createtime) +
			    V_ip6_desync_factor)) {
				maxvltime = V_ip6_temp_valid_lifetime -
				    (time_second - ifa6->ia6_createtime) -
				    V_ip6_desync_factor;
			} else
				maxvltime = 0;
			if (V_ip6_temp_preferred_lifetime >
			    (u_int32_t)((time_second - ifa6->ia6_createtime) +
			    V_ip6_desync_factor)) {
				maxpltime = V_ip6_temp_preferred_lifetime -
				    (time_second - ifa6->ia6_createtime) -
				    V_ip6_desync_factor;
			} else
				maxpltime = 0;

			if (lt6_tmp.ia6t_vltime == ND6_INFINITE_LIFETIME ||
			    lt6_tmp.ia6t_vltime > maxvltime) {
				lt6_tmp.ia6t_vltime = maxvltime;
			}
			if (lt6_tmp.ia6t_pltime == ND6_INFINITE_LIFETIME ||
			    lt6_tmp.ia6t_pltime > maxpltime) {
				lt6_tmp.ia6t_pltime = maxpltime;
			}
		}
		ifa6->ia6_lifetime = lt6_tmp;
		ifa6->ia6_updatetime = time_second;
	}
	IF_ADDR_UNLOCK(ifp);
	if (ia6_match == NULL && new->ndpr_vltime) {
		int ifidlen;

		/*
		 * 5.5.3 (d) (continued)
		 * No address matched and the valid lifetime is non-zero.
		 * Create a new address.
		 */

		/*
		 * Prefix Length check:
		 * If the sum of the prefix length and interface identifier
		 * length does not equal 128 bits, the Prefix Information
		 * option MUST be ignored.  The length of the interface
		 * identifier is defined in a separate link-type specific
		 * document.
		 */
		ifidlen = in6_if2idlen(ifp);
		if (ifidlen < 0) {
			/* this should not happen, so we always log it. */
			log(LOG_ERR, "prelist_update: IFID undefined (%s)\n",
			    if_name(ifp));
			goto end;
		}
		if (ifidlen + pr->ndpr_plen != 128) {
			nd6log((LOG_INFO,
			    "prelist_update: invalid prefixlen "
			    "%d for %s, ignored\n",
			    pr->ndpr_plen, if_name(ifp)));
			goto end;
		}

		if ((ia6 = in6_ifadd(new, mcast)) != NULL) {
			/*
			 * note that we should use pr (not new) for reference.
			 */
			pr->ndpr_refcnt++;
			ia6->ia6_ndpr = pr;

			/*
			 * RFC 3041 3.3 (2).
			 * When a new public address is created as described
			 * in RFC2462, also create a new temporary address.
			 *
			 * RFC 3041 3.5.
			 * When an interface connects to a new link, a new
			 * randomized interface identifier should be generated
			 * immediately together with a new set of temporary
			 * addresses.  Thus, we specifiy 1 as the 2nd arg of
			 * in6_tmpifadd().
			 */
			if (V_ip6_use_tempaddr) {
				int e;
				if ((e = in6_tmpifadd(ia6, 1, 1)) != 0) {
					nd6log((LOG_NOTICE, "prelist_update: "
					    "failed to create a temporary "
					    "address, errno=%d\n",
					    e));
				}
			}
			ifa_free(&ia6->ia_ifa);

			/*
			 * A newly added address might affect the status
			 * of other addresses, so we check and update it.
			 * XXX: what if address duplication happens?
			 */
			pfxlist_onlink_check();
		} else {
			/* just set an error. do not bark here. */
			error = EADDRNOTAVAIL; /* XXX: might be unused. */
		}
	}

 end:
	splx(s);
	return error;
}

/*
 * A supplement function used in the on-link detection below;
 * detect if a given prefix has a (probably) reachable advertising router.
 * XXX: lengthy function name...
 */
static struct nd_pfxrouter *
find_pfxlist_reachable_router(struct nd_prefix *pr)
{
	struct nd_pfxrouter *pfxrtr;
	struct llentry *ln;
	int canreach;

	for (pfxrtr = LIST_FIRST(&pr->ndpr_advrtrs); pfxrtr != NULL;
	     pfxrtr = LIST_NEXT(pfxrtr, pfr_entry)) {
		IF_AFDATA_LOCK(pfxrtr->router->ifp);
		ln = nd6_lookup(&pfxrtr->router->rtaddr, 0, pfxrtr->router->ifp);
		IF_AFDATA_UNLOCK(pfxrtr->router->ifp);
		if (ln == NULL)
			continue;
		canreach = ND6_IS_LLINFO_PROBREACH(ln);
		LLE_RUNLOCK(ln);
		if (canreach)
			break;
	}
	return (pfxrtr);
}

/*
 * Check if each prefix in the prefix list has at least one available router
 * that advertised the prefix (a router is "available" if its neighbor cache
 * entry is reachable or probably reachable).
 * If the check fails, the prefix may be off-link, because, for example,
 * we have moved from the network but the lifetime of the prefix has not
 * expired yet.  So we should not use the prefix if there is another prefix
 * that has an available router.
 * But, if there is no prefix that has an available router, we still regards
 * all the prefixes as on-link.  This is because we can't tell if all the
 * routers are simply dead or if we really moved from the network and there
 * is no router around us.
 */
void
pfxlist_onlink_check()
{
	struct nd_prefix *pr;
	struct in6_ifaddr *ifa;
	struct nd_defrouter *dr;
	struct nd_pfxrouter *pfxrtr = NULL;

	/*
	 * Check if there is a prefix that has a reachable advertising
	 * router.
	 */
	for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		if (pr->ndpr_raf_onlink && find_pfxlist_reachable_router(pr))
			break;
	}

	/*
	 * If we have no such prefix, check whether we still have a router
	 * that does not advertise any prefixes.
	 */
	if (pr == NULL) {
		for (dr = TAILQ_FIRST(&V_nd_defrouter); dr;
		    dr = TAILQ_NEXT(dr, dr_entry)) {
			struct nd_prefix *pr0;

			for (pr0 = V_nd_prefix.lh_first; pr0;
			    pr0 = pr0->ndpr_next) {
				if ((pfxrtr = pfxrtr_lookup(pr0, dr)) != NULL)
					break;
			}
			if (pfxrtr != NULL)
				break;
		}
	}
	if (pr != NULL || (TAILQ_FIRST(&V_nd_defrouter) && pfxrtr == NULL)) {
		/*
		 * There is at least one prefix that has a reachable router,
		 * or at least a router which probably does not advertise
		 * any prefixes.  The latter would be the case when we move
		 * to a new link where we have a router that does not provide
		 * prefixes and we configure an address by hand.
		 * Detach prefixes which have no reachable advertising
		 * router, and attach other prefixes.
		 */
		for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
			/* XXX: a link-local prefix should never be detached */
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			/*
			 * we aren't interested in prefixes without the L bit
			 * set.
			 */
			if (pr->ndpr_raf_onlink == 0)
				continue;

			if (pr->ndpr_raf_auto == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
			    find_pfxlist_reachable_router(pr) == NULL)
				pr->ndpr_stateflags |= NDPRF_DETACHED;
			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
			    find_pfxlist_reachable_router(pr) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	} else {
		/* there is no prefix that has a reachable router */
		for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			if (pr->ndpr_raf_onlink == 0)
				continue;

			if (pr->ndpr_raf_auto == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	}

	/*
	 * Remove each interface route associated with a (just) detached
	 * prefix, and reinstall the interface route for a (just) attached
	 * prefix.  Note that all attempt of reinstallation does not
	 * necessarily success, when a same prefix is shared among multiple
	 * interfaces.  Such cases will be handled in nd6_prefix_onlink,
	 * so we don't have to care about them.
	 */
	for (pr = V_nd_prefix.lh_first; pr; pr = pr->ndpr_next) {
		int e;
		char ip6buf[INET6_ADDRSTRLEN];

		if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
			continue;

		if (pr->ndpr_raf_onlink == 0)
			continue;

		if (pr->ndpr_raf_auto == 0)
			continue;

		if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
			if ((e = nd6_prefix_offlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d offlink, errno=%d\n",
				    ip6_sprintf(ip6buf,
					    &pr->ndpr_prefix.sin6_addr),
					    pr->ndpr_plen, e));
			}
		}
		if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0 &&
		    pr->ndpr_raf_onlink) {
			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d onlink, errno=%d\n",
				    ip6_sprintf(ip6buf,
					    &pr->ndpr_prefix.sin6_addr),
					    pr->ndpr_plen, e));
			}
		}
	}

	/*
	 * Changes on the prefix status might affect address status as well.
	 * Make sure that all addresses derived from an attached prefix are
	 * attached, and that all addresses derived from a detached prefix are
	 * detached.  Note, however, that a manually configured address should
	 * always be attached.
	 * The precise detection logic is same as the one for prefixes.
	 *
	 * XXXRW: in6_ifaddrhead locking.
	 */
	TAILQ_FOREACH(ifa, &V_in6_ifaddrhead, ia_link) {
		if (!(ifa->ia6_flags & IN6_IFF_AUTOCONF))
			continue;

		if (ifa->ia6_ndpr == NULL) {
			/*
			 * This can happen when we first configure the address
			 * (i.e. the address exists, but the prefix does not).
			 * XXX: complicated relationships...
			 */
			continue;
		}

		if (find_pfxlist_reachable_router(ifa->ia6_ndpr))
			break;
	}
	if (ifa) {
		TAILQ_FOREACH(ifa, &V_in6_ifaddrhead, ia_link) {
			if ((ifa->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			if (ifa->ia6_ndpr == NULL) /* XXX: see above. */
				continue;

			if (find_pfxlist_reachable_router(ifa->ia6_ndpr)) {
				if (ifa->ia6_flags & IN6_IFF_DETACHED) {
					ifa->ia6_flags &= ~IN6_IFF_DETACHED;
					ifa->ia6_flags |= IN6_IFF_TENTATIVE;
					nd6_dad_start((struct ifaddr *)ifa, 0);
				}
			} else {
				ifa->ia6_flags |= IN6_IFF_DETACHED;
			}
		}
	}
	else {
		TAILQ_FOREACH(ifa, &V_in6_ifaddrhead, ia_link) {
			if ((ifa->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			if (ifa->ia6_flags & IN6_IFF_DETACHED) {
				ifa->ia6_flags &= ~IN6_IFF_DETACHED;
				ifa->ia6_flags |= IN6_IFF_TENTATIVE;
				/* Do we need a delay in this case? */
				nd6_dad_start((struct ifaddr *)ifa, 0);
			}
		}
	}
}

int
nd6_prefix_onlink(struct nd_prefix *pr)
{
	struct ifaddr *ifa;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct sockaddr_in6 mask6;
	struct nd_prefix *opr;
	u_long rtflags;
	int error = 0;
	struct radix_node_head *rnh;
	struct rtentry *rt = NULL;
	char ip6buf[INET6_ADDRSTRLEN];
	struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_onlink: %s/%d is already on-link\n",
		    ip6_sprintf(ip6buf, &pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen));
		return (EEXIST);
	}

	/*
	 * Add the interface route associated with the prefix.  Before
	 * installing the route, check if there's the same prefix on another
	 * interface, and the prefix has already installed the interface route.
	 * Although such a configuration is expected to be rare, we explicitly
	 * allow it.
	 */
	for (opr = V_nd_prefix.lh_first; opr; opr = opr->ndpr_next) {
		if (opr == pr)
			continue;

		if ((opr->ndpr_stateflags & NDPRF_ONLINK) == 0)
			continue;

		if (opr->ndpr_plen == pr->ndpr_plen &&
		    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
		    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen))
			return (0);
	}

	/*
	 * We prefer link-local addresses as the associated interface address.
	 */
	/* search for a link-local addr */
	ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp,
	    IN6_IFF_NOTREADY | IN6_IFF_ANYCAST);
	if (ifa == NULL) {
		/* XXX: freebsd does not have ifa_ifwithaf */
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family == AF_INET6)
				break;
		}
		if (ifa != NULL)
			ifa_ref(ifa);
		IF_ADDR_UNLOCK(ifp);
		/* should we care about ia6_flags? */
	}
	if (ifa == NULL) {
		/*
		 * This can still happen, when, for example, we receive an RA
		 * containing a prefix with the L bit set and the A bit clear,
		 * after removing all IPv6 addresses on the receiving
		 * interface.  This should, of course, be rare though.
		 */
		nd6log((LOG_NOTICE,
		    "nd6_prefix_onlink: failed to find any ifaddr"
		    " to add route for a prefix(%s/%d) on %s\n",
		    ip6_sprintf(ip6buf, &pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(ifp)));
		return (0);
	}

	/*
	 * in6_ifinit() sets nd6_rtrequest to ifa_rtrequest for all ifaddrs.
	 * ifa->ifa_rtrequest = nd6_rtrequest;
	 */
	bzero(&mask6, sizeof(mask6));
	mask6.sin6_len = sizeof(mask6);
	mask6.sin6_addr = pr->ndpr_mask;
	rtflags = ifa->ifa_flags | RTF_UP;
	error = rtrequest(RTM_ADD, (struct sockaddr *)&pr->ndpr_prefix,
	    ifa->ifa_addr, (struct sockaddr *)&mask6, rtflags, &rt);
	if (error == 0) {
		if (rt != NULL) /* this should be non NULL, though */ {
			rnh = rt_tables_get_rnh(rt->rt_fibnum, AF_INET6);
			/* XXX what if rhn == NULL? */
			RADIX_NODE_HEAD_LOCK(rnh);
			RT_LOCK(rt);
			if (!rt_setgate(rt, rt_key(rt), (struct sockaddr *)&null_sdl)) {
				((struct sockaddr_dl *)rt->rt_gateway)->sdl_type =
					rt->rt_ifp->if_type;
				((struct sockaddr_dl *)rt->rt_gateway)->sdl_index =
					rt->rt_ifp->if_index;
			}
			RADIX_NODE_HEAD_UNLOCK(rnh);
			nd6_rtmsg(RTM_ADD, rt);
			RT_UNLOCK(rt);
		}
		pr->ndpr_stateflags |= NDPRF_ONLINK;
	} else {
		char ip6bufg[INET6_ADDRSTRLEN], ip6bufm[INET6_ADDRSTRLEN];
		nd6log((LOG_ERR, "nd6_prefix_onlink: failed to add route for a"
		    " prefix (%s/%d) on %s, gw=%s, mask=%s, flags=%lx "
		    "errno = %d\n",
		    ip6_sprintf(ip6buf, &pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(ifp),
		    ip6_sprintf(ip6bufg, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr),
		    ip6_sprintf(ip6bufm, &mask6.sin6_addr), rtflags, error));
	}

	if (rt != NULL) {
		RT_LOCK(rt);
		RT_REMREF(rt);
		RT_UNLOCK(rt);
	}
	if (ifa != NULL)
		ifa_free(ifa);

	return (error);
}

int
nd6_prefix_offlink(struct nd_prefix *pr)
{
	int error = 0;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct nd_prefix *opr;
	struct sockaddr_in6 sa6, mask6;
	struct rtentry *rt = NULL;
	char ip6buf[INET6_ADDRSTRLEN];

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: %s/%d is already off-link\n",
		    ip6_sprintf(ip6buf, &pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen));
		return (EEXIST);
	}

	bzero(&sa6, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	sa6.sin6_len = sizeof(sa6);
	bcopy(&pr->ndpr_prefix.sin6_addr, &sa6.sin6_addr,
	    sizeof(struct in6_addr));
	bzero(&mask6, sizeof(mask6));
	mask6.sin6_family = AF_INET6;
	mask6.sin6_len = sizeof(sa6);
	bcopy(&pr->ndpr_mask, &mask6.sin6_addr, sizeof(struct in6_addr));
	error = rtrequest(RTM_DELETE, (struct sockaddr *)&sa6, NULL,
	    (struct sockaddr *)&mask6, 0, &rt);
	if (error == 0) {
		pr->ndpr_stateflags &= ~NDPRF_ONLINK;

		/* report the route deletion to the routing socket. */
		if (rt != NULL)
			nd6_rtmsg(RTM_DELETE, rt);

		/*
		 * There might be the same prefix on another interface,
		 * the prefix which could not be on-link just because we have
		 * the interface route (see comments in nd6_prefix_onlink).
		 * If there's one, try to make the prefix on-link on the
		 * interface.
		 */
		for (opr = V_nd_prefix.lh_first; opr; opr = opr->ndpr_next) {
			if (opr == pr)
				continue;

			if ((opr->ndpr_stateflags & NDPRF_ONLINK) != 0)
				continue;

			/*
			 * KAME specific: detached prefixes should not be
			 * on-link.
			 */
			if ((opr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				continue;

			if (opr->ndpr_plen == pr->ndpr_plen &&
			    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
			    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen)) {
				int e;

				if ((e = nd6_prefix_onlink(opr)) != 0) {
					nd6log((LOG_ERR,
					    "nd6_prefix_offlink: failed to "
					    "recover a prefix %s/%d from %s "
					    "to %s (errno = %d)\n",
					    ip6_sprintf(ip6buf,
						&opr->ndpr_prefix.sin6_addr),
					    opr->ndpr_plen, if_name(ifp),
					    if_name(opr->ndpr_ifp), e));
				}
			}
		}
	} else {
		/* XXX: can we still set the NDPRF_ONLINK flag? */
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: failed to delete route: "
		    "%s/%d on %s (errno = %d)\n",
		    ip6_sprintf(ip6buf, &sa6.sin6_addr), pr->ndpr_plen,
		    if_name(ifp), error));
	}

	if (rt != NULL) {
		RTFREE(rt);
	}

	return (error);
}

static struct in6_ifaddr *
in6_ifadd(struct nd_prefixctl *pr, int mcast)
{
	struct ifnet *ifp = pr->ndpr_ifp;
	struct ifaddr *ifa;
	struct in6_aliasreq ifra;
	struct in6_ifaddr *ia, *ib;
	int error, plen0;
	struct in6_addr mask;
	int prefixlen = pr->ndpr_plen;
	int updateflags;
	char ip6buf[INET6_ADDRSTRLEN];

	in6_prefixlen2mask(&mask, prefixlen);

	/*
	 * find a link-local address (will be interface ID).
	 * Is it really mandatory? Theoretically, a global or a site-local
	 * address can be configured without a link-local address, if we
	 * have a unique interface identifier...
	 *
	 * it is not mandatory to have a link-local address, we can generate
	 * interface identifier on the fly.  we do this because:
	 * (1) it should be the easiest way to find interface identifier.
	 * (2) RFC2462 5.4 suggesting the use of the same interface identifier
	 * for multiple addresses on a single interface, and possible shortcut
	 * of DAD.  we omitted DAD for this reason in the past.
	 * (3) a user can prevent autoconfiguration of global address
	 * by removing link-local address by hand (this is partly because we
	 * don't have other way to control the use of IPv6 on an interface.
	 * this has been our design choice - cf. NRL's "ifconfig auto").
	 * (4) it is easier to manage when an interface has addresses
	 * with the same interface identifier, than to have multiple addresses
	 * with different interface identifiers.
	 */
	ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp, 0); /* 0 is OK? */
	if (ifa)
		ib = (struct in6_ifaddr *)ifa;
	else
		return NULL;

	/* prefixlen + ifidlen must be equal to 128 */
	plen0 = in6_mask2len(&ib->ia_prefixmask.sin6_addr, NULL);
	if (prefixlen != plen0) {
		ifa_free(ifa);
		nd6log((LOG_INFO, "in6_ifadd: wrong prefixlen for %s "
		    "(prefix=%d ifid=%d)\n",
		    if_name(ifp), prefixlen, 128 - plen0));
		return NULL;
	}

	/* make ifaddr */

	bzero(&ifra, sizeof(ifra));
	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));
	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	/* prefix */
	ifra.ifra_addr.sin6_addr = pr->ndpr_prefix.sin6_addr;
	ifra.ifra_addr.sin6_addr.s6_addr32[0] &= mask.s6_addr32[0];
	ifra.ifra_addr.sin6_addr.s6_addr32[1] &= mask.s6_addr32[1];
	ifra.ifra_addr.sin6_addr.s6_addr32[2] &= mask.s6_addr32[2];
	ifra.ifra_addr.sin6_addr.s6_addr32[3] &= mask.s6_addr32[3];

	/* interface ID */
	ifra.ifra_addr.sin6_addr.s6_addr32[0] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[0] & ~mask.s6_addr32[0]);
	ifra.ifra_addr.sin6_addr.s6_addr32[1] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[1] & ~mask.s6_addr32[1]);
	ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[2] & ~mask.s6_addr32[2]);
	ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[3] & ~mask.s6_addr32[3]);
	ifa_free(ifa);

	/* new prefix mask. */
	ifra.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_prefixmask.sin6_family = AF_INET6;
	bcopy(&mask, &ifra.ifra_prefixmask.sin6_addr,
	    sizeof(ifra.ifra_prefixmask.sin6_addr));

	/* lifetimes. */
	ifra.ifra_lifetime.ia6t_vltime = pr->ndpr_vltime;
	ifra.ifra_lifetime.ia6t_pltime = pr->ndpr_pltime;

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= IN6_IFF_AUTOCONF; /* obey autoconf */

	/*
	 * Make sure that we do not have this address already.  This should
	 * usually not happen, but we can still see this case, e.g., if we
	 * have manually configured the exact address to be configured.
	 */
	ifa = (struct ifaddr *)in6ifa_ifpwithaddr(ifp,
	    &ifra.ifra_addr.sin6_addr);
	if (ifa != NULL) {
		ifa_free(ifa);
		/* this should be rare enough to make an explicit log */
		log(LOG_INFO, "in6_ifadd: %s is already configured\n",
		    ip6_sprintf(ip6buf, &ifra.ifra_addr.sin6_addr));
		return (NULL);
	}

	/*
	 * Allocate ifaddr structure, link into chain, etc.
	 * If we are going to create a new address upon receiving a multicasted
	 * RA, we need to impose a random delay before starting DAD.
	 * [draft-ietf-ipv6-rfc2462bis-02.txt, Section 5.4.2]
	 */
	updateflags = 0;
	if (mcast)
		updateflags |= IN6_IFAUPDATE_DADDELAY;
	if ((error = in6_update_ifa(ifp, &ifra, NULL, updateflags)) != 0) {
		nd6log((LOG_ERR,
		    "in6_ifadd: failed to make ifaddr %s on %s (errno=%d)\n",
		    ip6_sprintf(ip6buf, &ifra.ifra_addr.sin6_addr),
		    if_name(ifp), error));
		return (NULL);	/* ifaddr must not have been allocated. */
	}

	ia = in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr);
	/*
	 * XXXRW: Assumption of non-NULLness here might not be true with
	 * fine-grained locking -- should we validate it?  Or just return
	 * earlier ifa rather than looking it up again?
	 */
	return (ia);		/* this is always non-NULL  and referenced. */
}

/*
 * ia0 - corresponding public address
 */
int
in6_tmpifadd(const struct in6_ifaddr *ia0, int forcegen, int delay)
{
	struct ifnet *ifp = ia0->ia_ifa.ifa_ifp;
	struct in6_ifaddr *newia, *ia;
	struct in6_aliasreq ifra;
	int i, error;
	int trylimit = 3;	/* XXX: adhoc value */
	int updateflags;
	u_int32_t randid[2];
	time_t vltime0, pltime0;

	bzero(&ifra, sizeof(ifra));
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));
	ifra.ifra_addr = ia0->ia_addr;
	/* copy prefix mask */
	ifra.ifra_prefixmask = ia0->ia_prefixmask;
	/* clear the old IFID */
	for (i = 0; i < 4; i++) {
		ifra.ifra_addr.sin6_addr.s6_addr32[i] &=
		    ifra.ifra_prefixmask.sin6_addr.s6_addr32[i];
	}

  again:
	if (in6_get_tmpifid(ifp, (u_int8_t *)randid,
	    (const u_int8_t *)&ia0->ia_addr.sin6_addr.s6_addr[8], forcegen)) {
		nd6log((LOG_NOTICE, "in6_tmpifadd: failed to find a good "
		    "random IFID\n"));
		return (EINVAL);
	}
	ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
	    (randid[0] & ~(ifra.ifra_prefixmask.sin6_addr.s6_addr32[2]));
	ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
	    (randid[1] & ~(ifra.ifra_prefixmask.sin6_addr.s6_addr32[3]));

	/*
	 * in6_get_tmpifid() quite likely provided a unique interface ID.
	 * However, we may still have a chance to see collision, because
	 * there may be a time lag between generation of the ID and generation
	 * of the address.  So, we'll do one more sanity check.
	 */
	IN6_IFADDR_RLOCK();
	TAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
		if (IN6_ARE_ADDR_EQUAL(&ia->ia_addr.sin6_addr,
		    &ifra.ifra_addr.sin6_addr)) {
			if (trylimit-- == 0) {
				IN6_IFADDR_RUNLOCK();
				/*
				 * Give up.  Something strange should have
				 * happened.
				 */
				nd6log((LOG_NOTICE, "in6_tmpifadd: failed to "
				    "find a unique random IFID\n"));
				return (EEXIST);
			}
			IN6_IFADDR_RUNLOCK();
			forcegen = 1;
			goto again;
		}
	}
	IN6_IFADDR_RUNLOCK();

	/*
	 * The Valid Lifetime is the lower of the Valid Lifetime of the
         * public address or TEMP_VALID_LIFETIME.
	 * The Preferred Lifetime is the lower of the Preferred Lifetime
         * of the public address or TEMP_PREFERRED_LIFETIME -
         * DESYNC_FACTOR.
	 */
	if (ia0->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
		vltime0 = IFA6_IS_INVALID(ia0) ? 0 :
		    (ia0->ia6_lifetime.ia6t_vltime -
		    (time_second - ia0->ia6_updatetime));
		if (vltime0 > V_ip6_temp_valid_lifetime)
			vltime0 = V_ip6_temp_valid_lifetime;
	} else
		vltime0 = V_ip6_temp_valid_lifetime;
	if (ia0->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
		pltime0 = IFA6_IS_DEPRECATED(ia0) ? 0 :
		    (ia0->ia6_lifetime.ia6t_pltime -
		    (time_second - ia0->ia6_updatetime));
		if (pltime0 > V_ip6_temp_preferred_lifetime - V_ip6_desync_factor){
			pltime0 = V_ip6_temp_preferred_lifetime -
			    V_ip6_desync_factor;
		}
	} else
		pltime0 = V_ip6_temp_preferred_lifetime - V_ip6_desync_factor;
	ifra.ifra_lifetime.ia6t_vltime = vltime0;
	ifra.ifra_lifetime.ia6t_pltime = pltime0;

	/*
	 * A temporary address is created only if this calculated Preferred
	 * Lifetime is greater than REGEN_ADVANCE time units.
	 */
	if (ifra.ifra_lifetime.ia6t_pltime <= V_ip6_temp_regen_advance)
		return (0);

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= (IN6_IFF_AUTOCONF|IN6_IFF_TEMPORARY);

	/* allocate ifaddr structure, link into chain, etc. */
	updateflags = 0;
	if (delay)
		updateflags |= IN6_IFAUPDATE_DADDELAY;
	if ((error = in6_update_ifa(ifp, &ifra, NULL, updateflags)) != 0)
		return (error);

	newia = in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr);
	if (newia == NULL) {	/* XXX: can it happen? */
		nd6log((LOG_ERR,
		    "in6_tmpifadd: ifa update succeeded, but we got "
		    "no ifaddr\n"));
		return (EINVAL); /* XXX */
	}
	newia->ia6_ndpr = ia0->ia6_ndpr;
	newia->ia6_ndpr->ndpr_refcnt++;
	ifa_free(&newia->ia_ifa);

	/*
	 * A newly added address might affect the status of other addresses.
	 * XXX: when the temporary address is generated with a new public
	 * address, the onlink check is redundant.  However, it would be safe
	 * to do the check explicitly everywhere a new address is generated,
	 * and, in fact, we surely need the check when we create a new
	 * temporary address due to deprecation of an old temporary address.
	 */
	pfxlist_onlink_check();

	return (0);
}

static int
in6_init_prefix_ltimes(struct nd_prefix *ndpr)
{
	if (ndpr->ndpr_pltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_preferred = 0;
	else
		ndpr->ndpr_preferred = time_second + ndpr->ndpr_pltime;
	if (ndpr->ndpr_vltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_expire = 0;
	else
		ndpr->ndpr_expire = time_second + ndpr->ndpr_vltime;

	return 0;
}

static void
in6_init_address_ltimes(struct nd_prefix *new, struct in6_addrlifetime *lt6)
{
	/* init ia6t_expire */
	if (lt6->ia6t_vltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_expire = 0;
	else {
		lt6->ia6t_expire = time_second;
		lt6->ia6t_expire += lt6->ia6t_vltime;
	}

	/* init ia6t_preferred */
	if (lt6->ia6t_pltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_preferred = 0;
	else {
		lt6->ia6t_preferred = time_second;
		lt6->ia6t_preferred += lt6->ia6t_pltime;
	}
}

/*
 * Delete all the routing table entries that use the specified gateway.
 * XXX: this function causes search through all entries of routing table, so
 * it shouldn't be called when acting as a router.
 */
void
rt6_flush(struct in6_addr *gateway, struct ifnet *ifp)
{
	struct radix_node_head *rnh;
	int s = splnet();

	/* We'll care only link-local addresses */
	if (!IN6_IS_ADDR_LINKLOCAL(gateway)) {
		splx(s);
		return;
	}

	rnh = rt_tables_get_rnh(0, AF_INET6);
	if (rnh == NULL)
		return;

	RADIX_NODE_HEAD_LOCK(rnh);
	rnh->rnh_walktree(rnh, rt6_deleteroute, (void *)gateway);
	RADIX_NODE_HEAD_UNLOCK(rnh);
	splx(s);
}

static int
rt6_deleteroute(struct radix_node *rn, void *arg)
{
#define SIN6(s)	((struct sockaddr_in6 *)s)
	struct rtentry *rt = (struct rtentry *)rn;
	struct in6_addr *gate = (struct in6_addr *)arg;

	if (rt->rt_gateway == NULL || rt->rt_gateway->sa_family != AF_INET6)
		return (0);

	if (!IN6_ARE_ADDR_EQUAL(gate, &SIN6(rt->rt_gateway)->sin6_addr)) {
		return (0);
	}

	/*
	 * Do not delete a static route.
	 * XXX: this seems to be a bit ad-hoc. Should we consider the
	 * 'cloned' bit instead?
	 */
	if ((rt->rt_flags & RTF_STATIC) != 0)
		return (0);

	/*
	 * We delete only host route. This means, in particular, we don't
	 * delete default route.
	 */
	if ((rt->rt_flags & RTF_HOST) == 0)
		return (0);

	return (rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
	    rt_mask(rt), rt->rt_flags, 0));
#undef SIN6
}

int
nd6_setdefaultiface(int ifindex)
{
	int error = 0;

	if (ifindex < 0 || V_if_index < ifindex)
		return (EINVAL);
	if (ifindex != 0 && !ifnet_byindex(ifindex))
		return (EINVAL);

	if (V_nd6_defifindex != ifindex) {
		V_nd6_defifindex = ifindex;
		if (V_nd6_defifindex > 0)
			V_nd6_defifp = ifnet_byindex(V_nd6_defifindex);
		else
			V_nd6_defifp = NULL;

		/*
		 * Our current implementation assumes one-to-one maping between
		 * interfaces and links, so it would be natural to use the
		 * default interface as the default link.
		 */
		scope6_setdefault(V_nd6_defifp);
	}

	return (error);
}

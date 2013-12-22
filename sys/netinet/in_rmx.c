/*-
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/callout.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>

extern int	in_inithead(void **head, int off);
#ifdef VIMAGE
extern int	in_detachhead(void **head, int off);
#endif

#define RTPRF_OURS		RTF_PROTO3	/* set on routes we manage */

/*
 * Do what we need to do when inserting a route.
 */
static struct radix_node *
in_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
    struct radix_node *treenodes)
{
	struct rtentry *rt = (struct rtentry *)treenodes;
	struct sockaddr_in *sin = (struct sockaddr_in *)rt_key(rt);

	RADIX_NODE_HEAD_WLOCK_ASSERT(head);
	/*
	 * A little bit of help for both IP output and input:
	 *   For host routes, we make sure that RTF_BROADCAST
	 *   is set for anything that looks like a broadcast address.
	 *   This way, we can avoid an expensive call to in_broadcast()
	 *   in ip_output() most of the time (because the route passed
	 *   to ip_output() is almost always a host route).
	 *
	 *   We also do the same for local addresses, with the thought
	 *   that this might one day be used to speed up ip_input().
	 *
	 * We also mark routes to multicast addresses as such, because
	 * it's easy to do and might be useful (but this is much more
	 * dubious since it's so easy to inspect the address).
	 */
	if (rt->rt_flags & RTF_HOST) {
		if (in_broadcast(sin->sin_addr, rt->rt_ifp)) {
			rt->rt_flags |= RTF_BROADCAST;
		} else if (satosin(rt->rt_ifa->ifa_addr)->sin_addr.s_addr ==
		    sin->sin_addr.s_addr) {
			rt->rt_flags |= RTF_LOCAL;
		}
	}
	if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
		rt->rt_flags |= RTF_MULTICAST;

	if (!rt->rt_rmx.rmx_mtu && rt->rt_ifp)
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;

	return (rn_addroute(v_arg, n_arg, head, treenodes));
}

/*
 * This code is the inverse of in_clsroute: on first reference, if we
 * were managing the route, stop doing so and set the expiration timer
 * back off again.
 */
static struct radix_node *
in_matroute(void *v_arg, struct radix_node_head *head)
{
	struct radix_node *rn = rn_match(v_arg, head);
	struct rtentry *rt = (struct rtentry *)rn;

	if (rt) {
		RT_LOCK(rt);
		if (rt->rt_flags & RTPRF_OURS) {
			rt->rt_flags &= ~RTPRF_OURS;
			rt->rt_rmx.rmx_expire = 0;
		}
		RT_UNLOCK(rt);
	}
	return rn;
}

static VNET_DEFINE(int, rtq_reallyold) = 60*60; /* one hour is "really old" */
#define	V_rtq_reallyold		VNET(rtq_reallyold)
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_RTEXPIRE, rtexpire, CTLFLAG_RW,
    &VNET_NAME(rtq_reallyold), 0,
    "Default expiration time on dynamically learned routes");

/* never automatically crank down to less */
static VNET_DEFINE(int, rtq_minreallyold) = 10;
#define	V_rtq_minreallyold	VNET(rtq_minreallyold)
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_RTMINEXPIRE, rtminexpire, CTLFLAG_RW,
    &VNET_NAME(rtq_minreallyold), 0,
    "Minimum time to attempt to hold onto dynamically learned routes");

/* 128 cached routes is "too many" */
static VNET_DEFINE(int, rtq_toomany) = 128;
#define	V_rtq_toomany		VNET(rtq_toomany)
SYSCTL_VNET_INT(_net_inet_ip, IPCTL_RTMAXCACHE, rtmaxcache, CTLFLAG_RW,
    &VNET_NAME(rtq_toomany), 0,
    "Upper limit on dynamically learned routes");

/*
 * On last reference drop, mark the route as belong to us so that it can be
 * timed out.
 */
static void
in_clsroute(struct radix_node *rn, struct radix_node_head *head)
{
	struct rtentry *rt = (struct rtentry *)rn;

	RT_LOCK_ASSERT(rt);

	if (!(rt->rt_flags & RTF_UP))
		return;			/* prophylactic measures */

	if (rt->rt_flags & RTPRF_OURS)
		return;

	if (!(rt->rt_flags & RTF_DYNAMIC))
		return;

	/*
	 * If rtq_reallyold is 0, just delete the route without
	 * waiting for a timeout cycle to kill it.
	 */
	if (V_rtq_reallyold != 0) {
		rt->rt_flags |= RTPRF_OURS;
		rt->rt_rmx.rmx_expire = time_uptime + V_rtq_reallyold;
	} else {
		rtexpunge(rt);
	}
}

struct rtqk_arg {
	struct radix_node_head *rnh;
	int draining;
	int killed;
	int found;
	int updating;
	time_t nextstop;
};

/*
 * Get rid of old routes.  When draining, this deletes everything, even when
 * the timeout is not expired yet.  When updating, this makes sure that
 * nothing has a timeout longer than the current value of rtq_reallyold.
 */
static int
in_rtqkill(struct radix_node *rn, void *rock)
{
	struct rtqk_arg *ap = rock;
	struct rtentry *rt = (struct rtentry *)rn;
	int err;

	RADIX_NODE_HEAD_WLOCK_ASSERT(ap->rnh);

	if (rt->rt_flags & RTPRF_OURS) {
		ap->found++;

		if (ap->draining || rt->rt_rmx.rmx_expire <= time_uptime) {
			if (rt->rt_refcnt > 0)
				panic("rtqkill route really not free");

			err = in_rtrequest(RTM_DELETE,
					(struct sockaddr *)rt_key(rt),
					rt->rt_gateway, rt_mask(rt),
					rt->rt_flags | RTF_RNH_LOCKED, 0,
					rt->rt_fibnum);
			if (err) {
				log(LOG_WARNING, "in_rtqkill: error %d\n", err);
			} else {
				ap->killed++;
			}
		} else {
			if (ap->updating &&
			    (rt->rt_rmx.rmx_expire - time_uptime >
			     V_rtq_reallyold)) {
				rt->rt_rmx.rmx_expire =
				    time_uptime + V_rtq_reallyold;
			}
			ap->nextstop = lmin(ap->nextstop,
					    rt->rt_rmx.rmx_expire);
		}
	}

	return 0;
}

#define RTQ_TIMEOUT	60*10	/* run no less than once every ten minutes */
static VNET_DEFINE(int, rtq_timeout) = RTQ_TIMEOUT;
static VNET_DEFINE(struct callout, rtq_timer);

#define	V_rtq_timeout		VNET(rtq_timeout)
#define	V_rtq_timer		VNET(rtq_timer)

static void in_rtqtimo_one(void *rock);

static void
in_rtqtimo(void *rock)
{
	CURVNET_SET((struct vnet *) rock);
	int fibnum;
	void *newrock;
	struct timeval atv;

	for (fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		newrock = rt_tables_get_rnh(fibnum, AF_INET);
		if (newrock != NULL)
			in_rtqtimo_one(newrock);
	}
	atv.tv_usec = 0;
	atv.tv_sec = V_rtq_timeout;
	callout_reset(&V_rtq_timer, tvtohz(&atv), in_rtqtimo, rock);
	CURVNET_RESTORE();
}

static void
in_rtqtimo_one(void *rock)
{
	struct radix_node_head *rnh = rock;
	struct rtqk_arg arg;
	static time_t last_adjusted_timeout = 0;

	arg.found = arg.killed = 0;
	arg.rnh = rnh;
	arg.nextstop = time_uptime + V_rtq_timeout;
	arg.draining = arg.updating = 0;
	RADIX_NODE_HEAD_LOCK(rnh);
	rnh->rnh_walktree(rnh, in_rtqkill, &arg);
	RADIX_NODE_HEAD_UNLOCK(rnh);

	/*
	 * Attempt to be somewhat dynamic about this:
	 * If there are ``too many'' routes sitting around taking up space,
	 * then crank down the timeout, and see if we can't make some more
	 * go away.  However, we make sure that we will never adjust more
	 * than once in rtq_timeout seconds, to keep from cranking down too
	 * hard.
	 */
	if ((arg.found - arg.killed > V_rtq_toomany) &&
	    (time_uptime - last_adjusted_timeout >= V_rtq_timeout) &&
	    V_rtq_reallyold > V_rtq_minreallyold) {
		V_rtq_reallyold = 2 * V_rtq_reallyold / 3;
		if (V_rtq_reallyold < V_rtq_minreallyold) {
			V_rtq_reallyold = V_rtq_minreallyold;
		}

		last_adjusted_timeout = time_uptime;
#ifdef DIAGNOSTIC
		log(LOG_DEBUG, "in_rtqtimo: adjusted rtq_reallyold to %d\n",
		    V_rtq_reallyold);
#endif
		arg.found = arg.killed = 0;
		arg.updating = 1;
		RADIX_NODE_HEAD_LOCK(rnh);
		rnh->rnh_walktree(rnh, in_rtqkill, &arg);
		RADIX_NODE_HEAD_UNLOCK(rnh);
	}

}

void
in_rtqdrain(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct radix_node_head *rnh;
	struct rtqk_arg arg;
	int 	fibnum;

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);

		for ( fibnum = 0; fibnum < rt_numfibs; fibnum++) {
			rnh = rt_tables_get_rnh(fibnum, AF_INET);
			arg.found = arg.killed = 0;
			arg.rnh = rnh;
			arg.nextstop = 0;
			arg.draining = 1;
			arg.updating = 0;
			RADIX_NODE_HEAD_LOCK(rnh);
			rnh->rnh_walktree(rnh, in_rtqkill, &arg);
			RADIX_NODE_HEAD_UNLOCK(rnh);
		}
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
}

void
in_setmatchfunc(struct radix_node_head *rnh, int val)
{

	rnh->rnh_matchaddr = (val != 0) ? rn_match : in_matroute;
}

static int _in_rt_was_here;
/*
 * Initialize our routing tree.
 */
int
in_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	/* XXX MRT
	 * This can be called from vfs_export.c too in which case 'off'
	 * will be 0. We know the correct value so just use that and
	 * return directly if it was 0.
	 * This is a hack that replaces an even worse hack on a bad hack
	 * on a bad design. After RELENG_7 this should be fixed but that
	 * will change the ABI, so for now do it this way.
	 */
	if (!rn_inithead(head, 32))
		return 0;

	if (off == 0)		/* XXX MRT  see above */
		return 1;	/* only do the rest for a real routing table */

	rnh = *head;
	rnh->rnh_addaddr = in_addroute;
	in_setmatchfunc(rnh, V_drop_redirect);
	rnh->rnh_close = in_clsroute;
	if (_in_rt_was_here == 0 ) {
		callout_init(&V_rtq_timer, CALLOUT_MPSAFE);
		callout_reset(&V_rtq_timer, 1, in_rtqtimo, curvnet);
		_in_rt_was_here = 1;
	}
	return 1;
}

#ifdef VIMAGE
int
in_detachhead(void **head, int off)
{

	callout_drain(&V_rtq_timer);
	return (1);
}
#endif

/*
 * This zaps old routes when the interface goes down or interface
 * address is deleted.  In the latter case, it deletes static routes
 * that point to this address.  If we don't do this, we may end up
 * using the old address in the future.  The ones we always want to
 * get rid of are things like ARP entries, since the user might down
 * the interface, walk over to a completely different network, and
 * plug back in.
 */
struct in_ifadown_arg {
	struct ifaddr *ifa;
	int del;
};

static int
in_ifadownkill(struct radix_node *rn, void *xap)
{
	struct in_ifadown_arg *ap = xap;
	struct rtentry *rt = (struct rtentry *)rn;

	RT_LOCK(rt);
	if (rt->rt_ifa == ap->ifa &&
	    (ap->del || !(rt->rt_flags & RTF_STATIC))) {
		/*
		 * Aquire a reference so that it can later be freed
		 * as the refcount would be 0 here in case of at least
		 * ap->del.
		 */
		RT_ADDREF(rt);
		/*
		 * Disconnect it from the tree and permit protocols
		 * to cleanup.
		 */
		rtexpunge(rt);
		/*
		 * At this point it is an rttrash node, and in case
		 * the above is the only reference we must free it.
		 * If we do not noone will have a pointer and the
		 * rtentry will be leaked forever.
		 * In case someone else holds a reference, we are
		 * fine as we only decrement the refcount. In that
		 * case if the other entity calls RT_REMREF, we
		 * will still be leaking but at least we tried.
		 */
		RTFREE_LOCKED(rt);
		return (0);
	}
	RT_UNLOCK(rt);
	return 0;
}

void
in_ifadown(struct ifaddr *ifa, int delete)
{
	struct in_ifadown_arg arg;
	struct radix_node_head *rnh;
	int	fibnum;

	KASSERT(ifa->ifa_addr->sa_family == AF_INET,
	    ("%s: wrong family", __func__));

	for ( fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		rnh = rt_tables_get_rnh(fibnum, AF_INET);
		arg.ifa = ifa;
		arg.del = delete;
		RADIX_NODE_HEAD_LOCK(rnh);
		rnh->rnh_walktree(rnh, in_ifadownkill, &arg);
		RADIX_NODE_HEAD_UNLOCK(rnh);
		ifa->ifa_flags &= ~IFA_ROUTE;		/* XXXlocking? */
	}
}

/*
 * inet versions of rt functions. These have fib extensions and 
 * for now will just reference the _fib variants.
 * eventually this order will be reversed,
 */
void
in_rtalloc_ign(struct route *ro, u_long ignflags, u_int fibnum)
{
	rtalloc_ign_fib(ro, ignflags, fibnum);
}

int
in_rtrequest( int req,
	struct sockaddr *dst,
	struct sockaddr *gateway,
	struct sockaddr *netmask,
	int flags,
	struct rtentry **ret_nrt,
	u_int fibnum)
{
	return (rtrequest_fib(req, dst, gateway, netmask, 
	    flags, ret_nrt, fibnum));
}

struct rtentry *
in_rtalloc1(struct sockaddr *dst, int report, u_long ignflags, u_int fibnum)
{
	return (rtalloc1_fib(dst, report, ignflags, fibnum));
}

void
in_rtredirect(struct sockaddr *dst,
	struct sockaddr *gateway,
	struct sockaddr *netmask,
	int flags,
	struct sockaddr *src,
	u_int fibnum)
{
	rtredirect_fib(dst, gateway, netmask, flags, src, fibnum);
}
 
void
in_rtalloc(struct route *ro, u_int fibnum)
{
	rtalloc_ign_fib(ro, 0UL, fibnum);
}

#if 0
int	 in_rt_getifa(struct rt_addrinfo *, u_int fibnum);
int	 in_rtioctl(u_long, caddr_t, u_int);
int	 in_rtrequest1(int, struct rt_addrinfo *, struct rtentry **, u_int);
#endif



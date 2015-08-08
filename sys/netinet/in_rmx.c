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

static void in_setifarnh(struct radix_node_head *rnh, uint32_t fibnum,
    int af, void *_arg);
static void in_rtqtimo_setrnh(struct radix_node_head *rnh, uint32_t fibnum,
    int af, void *_arg);

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

	if (rt->rt_ifp != NULL) {

		/*
		 * Check route MTU:
		 * inherit interface MTU if not set or
		 * check if MTU is too large.
		 */
		if (rt->rt_mtu == 0) {
			rt->rt_mtu = rt->rt_ifp->if_mtu;
		} else if (rt->rt_mtu > rt->rt_ifp->if_mtu)
			rt->rt_mtu = rt->rt_ifp->if_mtu;
	}

	return (rn_addroute(v_arg, n_arg, head, treenodes));
}

static int _in_rt_was_here;
/*
 * Initialize our routing tree.
 */
int
in_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if (!rn_inithead(head, 32))
		return 0;

	rnh = *head;
	RADIX_NODE_HEAD_LOCK_INIT(rnh);

	rnh->rnh_addaddr = in_addroute;
	if (_in_rt_was_here == 0 ) {
		_in_rt_was_here = 1;
	}
	return 1;
}

#ifdef VIMAGE
int
in_detachhead(void **head, int off)
{

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
	struct radix_node_head *rnh;
	struct ifaddr *ifa;
	int del;
};

static int
in_ifadownkill(struct rtentry *rt, void *xap)
{
	struct in_ifadown_arg *ap = xap;

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
		rt_expunge(ap->rnh, rt);
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

static void
in_setifarnh(struct radix_node_head *rnh, uint32_t fibnum, int af,
    void *_arg)
{
	struct in_ifadown_arg *arg;

	arg = (struct in_ifadown_arg *)_arg;

	arg->rnh = rnh;
}

void
in_ifadown(struct ifaddr *ifa, int delete)
{
	struct in_ifadown_arg arg;

	KASSERT(ifa->ifa_addr->sa_family == AF_INET,
	    ("%s: wrong family", __func__));

	arg.ifa = ifa;
	arg.del = delete;

	rt_foreach_fib(AF_INET, in_setifarnh, in_ifadownkill, &arg);
	ifa->ifa_flags &= ~IFA_ROUTE;		/* XXXlocking? */
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



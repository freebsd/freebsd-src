/*
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
 *
 * $Id: in_rmx.c,v 1.17 1995/10/29 15:32:28 phk Exp $
 */

/*
 * This code does two things necessary for the enhanced TCP metrics to
 * function in a useful manner:
 *  1) It marks all non-host routes as `cloning', thus ensuring that
 *     every actual reference to such a route actually gets turned
 *     into a reference to a host route to the specific destination
 *     requested.
 *  2) When such routes lose all their references, it arranges for them
 *     to be deleted in some random collection of circumstances, so that
 *     a large quantity of stale routing data is not kept in kernel memory
 *     indefinitely.  See in_rtqtimo() below for the exact mechanism.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#ifndef MTUDISC
#include <netinet/tcpip.h>
#endif /* not MTUDISC */

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

	/*
	 * For IP, all unicast non-host routes are automatically cloning.
	 */
	if(!(rt->rt_flags & (RTF_HOST | RTF_CLONING))) {
		if(!IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			rt->rt_flags |= RTF_PRCLONING;
		}
	}

	/*
	 * We also specify a send and receive pipe size for every
	 * route added, to help TCP a bit.  TCP doesn't actually
	 * want a true pipe size, which would be prohibitive in memory
	 * costs and is hard to compute anyway; it simply uses these
	 * values to size its buffers.  So, we fill them in with the
	 * same values that TCP would have used anyway, and allow the
	 * installing program or the link layer to override these values
	 * as it sees fit.  This will hopefully allow TCP more
	 * opportunities to save its ssthresh value.
	 */
	if (!rt->rt_rmx.rmx_sendpipe && !(rt->rt_rmx.rmx_locks & RTV_SPIPE))
		rt->rt_rmx.rmx_sendpipe = tcp_sendspace;

	if (!rt->rt_rmx.rmx_recvpipe && !(rt->rt_rmx.rmx_locks & RTV_RPIPE))
		rt->rt_rmx.rmx_recvpipe = tcp_recvspace;

#ifndef MTUDISC
	/*
	 * Finally, set an MTU, again duplicating logic in TCP.
	 * The in_localaddr() business will go away when we have
	 * proper PMTU discovery.
	 */
#endif /* not MTUDISC */
	if (!rt->rt_rmx.rmx_mtu && !(rt->rt_rmx.rmx_locks & RTV_MTU) 
	    && rt->rt_ifp)
#ifndef MTUDISC
		rt->rt_rmx.rmx_mtu = (in_localaddr(sin->sin_addr)
				      ? rt->rt_ifp->if_mtu
				      : tcp_mssdflt + sizeof(struct tcpiphdr));
#else /* MTUDISC */
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
#endif /* MTUDISC */

	return rn_addroute(v_arg, n_arg, head, treenodes);
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

	if(rt && rt->rt_refcnt == 0) { /* this is first reference */
		if(rt->rt_flags & RTPRF_OURS) {
			rt->rt_flags &= ~RTPRF_OURS;
			rt->rt_rmx.rmx_expire = 0;
		}
	}
	return rn;
}

int rtq_reallyold = 60*60;
	/* one hour is ``really old'' */
SYSCTL_INT(_net_inet_ip, IPCTL_RTEXPIRE, rtexpire,
	CTLFLAG_RW, &rtq_reallyold , 0, "");
				   
int rtq_minreallyold = 10;
	/* never automatically crank down to less */
SYSCTL_INT(_net_inet_ip, IPCTL_RTMINEXPIRE, rtminexpire,
	CTLFLAG_RW, &rtq_minreallyold , 0, "");
				   
int rtq_toomany = 128;
	/* 128 cached routes is ``too many'' */
SYSCTL_INT(_net_inet_ip, IPCTL_RTMAXCACHE, rtmaxcache,
	CTLFLAG_RW, &rtq_toomany , 0, "");
				   

/*
 * On last reference drop, mark the route as belong to us so that it can be
 * timed out.
 */
static void
in_clsroute(struct radix_node *rn, struct radix_node_head *head)
{
	struct rtentry *rt = (struct rtentry *)rn;

	if(!(rt->rt_flags & RTF_UP))
		return;		/* prophylactic measures */

	if((rt->rt_flags & (RTF_LLINFO | RTF_HOST)) != RTF_HOST)
		return;

	if((rt->rt_flags & (RTF_WASCLONED | RTPRF_OURS))
	   != RTF_WASCLONED)
		return;

	/*
	 * As requested by David Greenman:
	 * If rtq_reallyold is 0, just delete the route without
	 * waiting for a timeout cycle to kill it.
	 */
	if(rtq_reallyold != 0) {
		rt->rt_flags |= RTPRF_OURS;
		rt->rt_rmx.rmx_expire = time.tv_sec + rtq_reallyold;
	} else {
		rtrequest(RTM_DELETE,
			  (struct sockaddr *)rt_key(rt),
			  rt->rt_gateway, rt_mask(rt),
			  rt->rt_flags, 0);
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

	if(rt->rt_flags & RTPRF_OURS) {
		ap->found++;

		if(ap->draining || rt->rt_rmx.rmx_expire <= time.tv_sec) {
			if(rt->rt_refcnt > 0)
				panic("rtqkill route really not free");

			err = rtrequest(RTM_DELETE,
					(struct sockaddr *)rt_key(rt),
					rt->rt_gateway, rt_mask(rt),
					rt->rt_flags, 0);
			if(err) {
				log(LOG_WARNING, "in_rtqkill: error %d\n", err);
			} else {
				ap->killed++;
			}
		} else {
			if(ap->updating
			   && (rt->rt_rmx.rmx_expire - time.tv_sec
			       > rtq_reallyold)) {
				rt->rt_rmx.rmx_expire = time.tv_sec
					+ rtq_reallyold;
			}
			ap->nextstop = lmin(ap->nextstop,
					    rt->rt_rmx.rmx_expire);
		}
	}

	return 0;
}

#define RTQ_TIMEOUT	60*10	/* run no less than once every ten minutes */
int rtq_timeout = RTQ_TIMEOUT;

static void
in_rtqtimo(void *rock)
{
	struct radix_node_head *rnh = rock;
	struct rtqk_arg arg;
	struct timeval atv;
	static time_t last_adjusted_timeout = 0;
	int s;

	arg.found = arg.killed = 0;
	arg.rnh = rnh;
	arg.nextstop = time.tv_sec + rtq_timeout;
	arg.draining = arg.updating = 0;
	s = splnet();
	rnh->rnh_walktree(rnh, in_rtqkill, &arg);
	splx(s);

	/*
	 * Attempt to be somewhat dynamic about this:
	 * If there are ``too many'' routes sitting around taking up space,
	 * then crank down the timeout, and see if we can't make some more
	 * go away.  However, we make sure that we will never adjust more
	 * than once in rtq_timeout seconds, to keep from cranking down too
	 * hard.
	 */
	if((arg.found - arg.killed > rtq_toomany)
	   && (time.tv_sec - last_adjusted_timeout >= rtq_timeout)
	   && rtq_reallyold > rtq_minreallyold) {
		rtq_reallyold = 2*rtq_reallyold / 3;
		if(rtq_reallyold < rtq_minreallyold) {
			rtq_reallyold = rtq_minreallyold;
		}

		last_adjusted_timeout = time.tv_sec;
		log(LOG_DEBUG, "in_rtqtimo: adjusted rtq_reallyold to %d\n",
		    rtq_reallyold);
		arg.found = arg.killed = 0;
		arg.updating = 1;
		s = splnet();
		rnh->rnh_walktree(rnh, in_rtqkill, &arg);
		splx(s);
	}

	atv.tv_usec = 0;
	atv.tv_sec = arg.nextstop;
	timeout(in_rtqtimo, rock, hzto(&atv));
}

void
in_rtqdrain(void)
{
	struct radix_node_head *rnh = rt_tables[AF_INET];
	struct rtqk_arg arg;
	int s;
	arg.found = arg.killed = 0;
	arg.rnh = rnh;
	arg.nextstop = 0;
	arg.draining = 1;
	arg.updating = 0;
	s = splnet();
	rnh->rnh_walktree(rnh, in_rtqkill, &arg);
	splx(s);
}

/*
 * Initialize our routing tree.
 */
int
in_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if(!rn_inithead(head, off))
		return 0;

	if(head != (void **)&rt_tables[AF_INET]) /* BOGUS! */
		return 1;	/* only do this for the real routing table */

	rnh = *head;
	rnh->rnh_addaddr = in_addroute;
	rnh->rnh_matchaddr = in_matroute;
	rnh->rnh_close = in_clsroute;
	in_rtqtimo(rnh);	/* kick off timeout first time */
	return 1;
}


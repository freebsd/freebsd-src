/*
 * Copyright 1994, Massachusetts Institute of Technology.  All Rights Reserved.
 *
 * You may copy this file verbatim until I find the official 
 * Institute boilerplate.
 *
 * $Id: in_rmx.c,v 1.7 1994/12/21 17:25:52 wollman Exp $
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

#define RTPRF_OURS		RTF_PROTO3	/* set on routes we manage */

/*
 * Do what we need to do when inserting a route.
 */
static struct radix_node *
in_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
	    struct radix_node *treenodes)
{
	struct rtentry *rt = (struct rtentry *)treenodes;

	/*
	 * For IP, all unicast non-host routes are automatically cloning.
	 */
	if(!(rt->rt_flags & (RTF_HOST | RTF_CLONING))) {
		struct sockaddr_in *sin = (struct sockaddr_in *)rt_key(rt);
		if(!IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			rt->rt_flags |= RTF_PRCLONING;
		}
	}

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

#define RTQ_REALLYOLD	60*60	/* one hour is ``really old'' */
int rtq_reallyold = RTQ_REALLYOLD;

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

	rt->rt_flags |= RTPRF_OURS;
	rt->rt_rmx.rmx_expire = time.tv_sec + rtq_reallyold;
}

struct rtqk_arg {
	struct radix_node_head *rnh;
	int draining;
	int killed;
	int found;
	time_t nextstop;
};

/*
 * Get rid of old routes.  When draining, this deletes everything, even when
 * the timeout is not expired yet.
 */
static int
in_rtqkill(struct radix_node *rn, void *rock)
{
	struct rtqk_arg *ap = rock;
	struct radix_node_head *rnh = ap->rnh;
	struct rtentry *rt = (struct rtentry *)rn;
	int err;

	if(rt->rt_flags & RTPRF_OURS) {
		ap->found++;

		if(ap->draining || rt->rt_rmx.rmx_expire <= time.tv_sec) {
			if(rt->rt_refcnt > 0)
				panic("rtqkill route really not free\n");

			err = rtrequest(RTM_DELETE,
					(struct sockaddr *)rt_key(rt),
					rt->rt_gateway, rt_mask(rt),
					rt->rt_flags, 0);
			if(err) {
				log(LOG_WARNING, "in_rtqkill: error %d", err);
			} else {
				ap->killed++;
			}
		} else {
			ap->nextstop = lmin(ap->nextstop,
					    rt->rt_rmx.rmx_expire);
		}
	}

	return 0;
}

#define RTQ_TIMEOUT	600	/* run no less than once every ten minutes */
int rtq_timeout = RTQ_TIMEOUT;

static void
in_rtqtimo(void *rock)
{
	struct radix_node_head *rnh = rock;
	struct rtqk_arg arg;
	struct timeval atv;
	int s;

	arg.found = arg.killed = 0;
	arg.rnh = rnh;
	arg.nextstop = time.tv_sec + rtq_timeout;
	arg.draining = 0;
	s = splnet();
	rnh->rnh_walktree(rnh, in_rtqkill, &arg);
	splx(s);
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


/*
 * Copyright 1994, Massachusetts Institute of Technology.  All Rights Reserved.
 *
 * You may copy this file verbatim until I find the official 
 * Institute boilerplate.
 *
 * $Id$
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
 *
 * At least initially, we think that this should have lower overhead than
 * using the existing `expire' mechanism and walking the radix tree
 * periodically, deleting things as we go.  That method would be relatively
 * easy to implement within the framework used here, and in the future
 * we made code both ways, so that folks with large routing tables can use
 * the external queue, and the majority with small routing tables can do
 * the tree-walk.
 */

/*
 * XXX - look for races
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

/*
 * The list of unreferenced IP routes...
 */
struct in_rtq {
	TAILQ_ENTRY(in_rtq)	inr_entry;
	struct rtentry		*inr_rt;
	time_t			inr_whenadded;
};

TAILQ_HEAD(, in_rtq) inr_head;
int inr_nelem = -1;

/*
 * Do what we need to do when inserting a route.
 * Note that we don't automatically add the route to the queue, because
 * our caller, rtrequest(), will immediately bump the refcount and give
 * a the route to someone.
 */
static struct radix_node *
in_addroute(void *v_arg, void *n_arg, struct radix_node_head *head,
	    struct radix_node *treenodes)
{
	struct rtentry *rt = (struct rtentry *)treenodes;
	struct radix_node *rn;
	struct in_rtq *inr;

	/*
	 * For IP, all non-host routes are automatically cloning.
	 */
	if(!(rt->rt_flags & RTF_HOST))
		rt->rt_flags |= RTF_CLONING;

	rn = rn_addroute(v_arg, n_arg, head, treenodes);
	rt = (struct rtentry *)rn;
	if(!rt
	   || !(rt->rt_flags & RTF_HOST)
	   || (rt->rt_flags & (RTF_LLINFO | RTF_STATIC | RTF_DYNAMIC)))
		return rn;

	return rn;
}

static struct radix_node *
in_delroute(void *v_arg, void *netmask_arg, struct radix_node_head *head)
{
#ifdef DIAGNOSTIC
	int nentries = 0;
#endif
	struct rtentry *rt = (struct rtentry *)rn_search(v_arg, 
							 head->rnh_treetop);
	if(rt && rt->rt_refcnt <= 0
	   && ((rt->rt_flags & (RTF_HOST | RTF_LLINFO | RTF_STATIC))
	       == RTF_HOST)) {
		struct in_rtq *inr = inr_head.tqh_first;

		while(inr) {
			if(inr->inr_rt == rt) {
				TAILQ_REMOVE(&inr_head, inr, inr_entry);
				inr_nelem--;
				FREE(inr, M_RTABLE);
#ifdef DIAGNOSTIC
				nentries++;
#else
				break;
#endif
			}
			inr = inr->inr_entry.tqe_next;
		}

#ifdef DIAGNOSTIC
		if(nentries != 1) {
			log(LOG_DEBUG, "route %p had %d queue entries!\n",
			    (void *)rt, nentries);
		}
#endif
	}

	return rn_delete(v_arg, netmask_arg, head);
}

/*
 * Find something in the queue.
 */
static inline struct in_rtq *
inr_findit(struct rtentry *rt)
{
	struct in_rtq *inr;

#ifndef DIAGNOSTIC
	if((rt->rt_flags & (RTF_HOST | RTF_LLINFO | RTF_STATIC)) != RTF_HOST)
		return 0;
#endif
	inr = inr_head.tqh_first;

	while(inr) {
		if(inr->inr_rt == rt)
			return inr;
		inr = inr->inr_entry.tqe_next;
	}

	return 0;
}
	

/*
 * This code is the inverse of in_clsroute: on first reference, we remove the
 * route from our queue (if it's in there)
 */
static struct radix_node *
in_matroute(void *v_arg, struct radix_node_head *head)
{
	struct radix_node *rn = rn_match(v_arg, head);
	struct rtentry *rt = (struct rtentry *)rn;

	if(rt && rt->rt_refcnt == 0) { /* this is first reference */
		struct in_rtq *inr = inr_findit(rt);
		if(inr) {
			TAILQ_REMOVE(&inr_head, inr, inr_entry);
			inr_nelem--;
			FREE(inr, M_RTABLE);
		}
	}
	return rn;
}

/*
 * On last reference drop, add the route to the queue so that it can be
 * timed out.
 */
static void
in_clsroute(struct radix_node *rn, struct radix_node_head *head)
{
	struct rtentry *rt = (struct rtentry *)rn;
	struct in_rtq *inr;
	
	inr = inr_findit(rt);
	if(inr) {
		/*
		 * In this case, we are probably in the process of deleting
		 * this route; the code path in route.c results in rnh_close()
		 * being called when during deletion of unreferenced routes.
		 * In any case, don't allocate a new queue entry for this
		 * route because there already is one.
		 */
		return;
	}

	MALLOC(inr, struct in_rtq *, sizeof *inr, M_RTABLE, M_NOWAIT);
	if(!inr)		
		return;		/* oops... no matter */

	inr->inr_rt = rt;
	inr->inr_whenadded = time.tv_sec;
	TAILQ_INSERT_TAIL(&inr_head, inr, inr_entry);
	inr_nelem++;
}

/*
 * Get rid of everything in the queue, we are short on memory.
 * Although this looks like an infinite loop, it really isn't;
 * rtrequest() eventually calls in_delroute() which ends up deleting
 * the node at the head (or so we hope).  This should be called from
 * ip_drain().
 */
void
in_rtqdrain(void)
{
	struct in_rtq *inr;

	while(inr = inr_head.tqh_first) {
		rtrequest(RTM_DELETE, rt_key(inr->inr_rt), 
			  inr->inr_rt->rt_gateway, rt_mask(inr->inr_rt),
			  RTF_HOST, 0);
		/* KILL! KILL! KILL! */
	}
}

#define RTQ_TIMEOUT	(60*hz)	/* run once a minute */
#define RTQ_REALLYOLD	4*60*60	/* four hours is ``really old'' */
#define RTQ_TOOMANY	128	/* > 128 routes is ``too many'' */

/*
 * Get rid of old routes.  We have two strategies here:
 * 1) If there are more than RTQ_TOOMANY routes, delete half of them.
 * 2) Delete all routes older than RTQ_REALLYOLD (the LRU nature of the
 *    queue ensures that these are all at the front).
 */
static void
in_rtqtimo(void *rock)
{
	int s = splnet();
	struct in_rtq *inr = inr_head.tqh_first;
	
	if(inr_nelem > RTQ_TOOMANY) {
		int ntodel = inr_nelem / 2;

		for(; inr && ntodel; inr = inr_head.tqh_first, ntodel--) {
			rtrequest(RTM_DELETE, rt_key(inr->inr_rt),
				  inr->inr_rt->rt_gateway, 
				  rt_mask(inr->inr_rt), RTF_HOST, 0);
		}
	}

	while(inr && (time.tv_sec - inr->inr_whenadded) > RTQ_REALLYOLD) {
		rtrequest(RTM_DELETE, rt_key(inr->inr_rt), 
			  inr->inr_rt->rt_gateway, rt_mask(inr->inr_rt),
			  RTF_HOST, 0);
		inr = inr_head.tqh_first;
	}

	timeout(in_rtqtimo, (void *)0, RTQ_TIMEOUT);
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

	rnh = *head;
	rnh->rnh_addaddr = in_addroute;
	rnh->rnh_deladdr = in_delroute;
	rnh->rnh_matchaddr = in_matroute;
	rnh->rnh_close = in_clsroute;
	TAILQ_INIT(&inr_head);
	inr_nelem = 0;
	in_rtqtimo(0);		/* kick off timeout first time */
	return 1;
}


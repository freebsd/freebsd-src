/*
 * Copyright (c) 1983, 1988, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */

#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)tables.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif
#ident "$Revision: 1.25 $"

#include "defs.h"

static struct rt_spare *rts_better(struct rt_entry *);

struct radix_node_head *rhead;		/* root of the radix tree */

int	need_flash = 1;			/* flash update needed
					 * start =1 to suppress the 1st
					 */

struct timeval age_timer;		/* next check of old routes */
struct timeval need_kern = {		/* need to update kernel table */
	EPOCH+MIN_WAITTIME-1
};

int	stopint;

int	total_routes;

naddr	age_bad_gate;


/* It is desirable to "aggregate" routes, to combine differing routes of
 * the same metric and next hop into a common route with a smaller netmask
 * or to suppress redundant routes, routes that add no information to
 * routes with smaller netmasks.
 *
 * A route is redundant if and only if any and all routes with smaller
 * but matching netmasks and nets are the same.  Since routes are
 * kept sorted in the radix tree, redundant routes always come second.
 *
 * There are two kinds of aggregations.  First, two routes of the same bit
 * mask and differing only in the least significant bit of the network
 * number can be combined into a single route with a coarser mask.
 *
 * Second, a route can be suppressed in favor of another route with a more
 * coarse mask provided no incompatible routes with intermediate masks
 * are present.  The second kind of aggregation involves suppressing routes.
 * A route must not be suppressed if an incompatible route exists with
 * an intermediate mask, since the suppressed route would be covered
 * by the intermediate.
 *
 * This code relies on the radix tree walk encountering routes
 * sorted first by address, with the smallest address first.
 */

struct ag_info ag_slots[NUM_AG_SLOTS], *ag_avail, *ag_corsest, *ag_finest;

/* #define DEBUG_AG */
#ifdef DEBUG_AG
#define CHECK_AG() {int acnt = 0; struct ag_info *cag;		\
	for (cag = ag_avail; cag != 0; cag = cag->ag_fine)	\
		acnt++;						\
	for (cag = ag_corsest; cag != 0; cag = cag->ag_fine)	\
		acnt++;						\
	if (acnt != NUM_AG_SLOTS) {				\
		(void)fflush(stderr);				\
		abort();					\
	}							\
}
#else
#define CHECK_AG()
#endif


/* Output the contents of an aggregation table slot.
 *	This function must always be immediately followed with the deletion
 *	of the target slot.
 */
static void
ag_out(struct ag_info *ag,
	 void (*out)(struct ag_info *))
{
	struct ag_info *ag_cors;
	naddr bit;


	/* If we output both the even and odd twins, then the immediate parent,
	 * if it is present, is redundant, unless the parent manages to
	 * aggregate into something coarser.
	 * On successive calls, this code detects the even and odd twins,
	 * and marks the parent.
	 *
	 * Note that the order in which the radix tree code emits routes
	 * ensures that the twins are seen before the parent is emitted.
	 */
	ag_cors = ag->ag_cors;
	if (ag_cors != 0
	    && ag_cors->ag_mask == ag->ag_mask<<1
	    && ag_cors->ag_dst_h == (ag->ag_dst_h & ag_cors->ag_mask)) {
		ag_cors->ag_state |= ((ag_cors->ag_dst_h == ag->ag_dst_h)
				      ? AGS_REDUN0
				      : AGS_REDUN1);
	}

	/* Skip it if this route is itself redundant.
	 *
	 * It is ok to change the contents of the slot here, since it is
	 * always deleted next.
	 */
	if (ag->ag_state & AGS_REDUN0) {
		if (ag->ag_state & AGS_REDUN1)
			return;
		bit = (-ag->ag_mask) >> 1;
		ag->ag_dst_h |= bit;
		ag->ag_mask |= bit;

	} else if (ag->ag_state & AGS_REDUN1) {
		bit = (-ag->ag_mask) >> 1;
		ag->ag_mask |= bit;
	}
	out(ag);
}


static void
ag_del(struct ag_info *ag)
{
	CHECK_AG();

	if (ag->ag_cors == 0)
		ag_corsest = ag->ag_fine;
	else
		ag->ag_cors->ag_fine = ag->ag_fine;

	if (ag->ag_fine == 0)
		ag_finest = ag->ag_cors;
	else
		ag->ag_fine->ag_cors = ag->ag_cors;

	ag->ag_fine = ag_avail;
	ag_avail = ag;

	CHECK_AG();
}


/* Flush routes waiting for aggretation.
 *	This must not suppress a route unless it is known that among all
 *	routes with coarser masks that match it, the one with the longest
 *	mask is appropriate.  This is ensured by scanning the routes
 *	in lexical order, and with the most restritive mask first
 *	among routes to the same destination.
 */
void
ag_flush(naddr lim_dst_h,		/* flush routes to here */
	 naddr lim_mask,		/* matching this mask */
	 void (*out)(struct ag_info *))
{
	struct ag_info *ag, *ag_cors;
	naddr dst_h;


	for (ag = ag_finest;
	     ag != 0 && ag->ag_mask >= lim_mask;
	     ag = ag_cors) {
		ag_cors = ag->ag_cors;

		/* work on only the specified routes */
		dst_h = ag->ag_dst_h;
		if ((dst_h & lim_mask) != lim_dst_h)
			continue;

		if (!(ag->ag_state & AGS_SUPPRESS))
			ag_out(ag, out);

		else for ( ; ; ag_cors = ag_cors->ag_cors) {
			/* Look for a route that can suppress the
			 * current route */
			if (ag_cors == 0) {
				/* failed, so output it and look for
				 * another route to work on
				 */
				ag_out(ag, out);
				break;
			}

			if ((dst_h & ag_cors->ag_mask) == ag_cors->ag_dst_h) {
				/* We found a route with a coarser mask that
				 * aggregates the current target.
				 *
				 * If it has a different next hop, it
				 * cannot replace the target, so output
				 * the target.
				 */
				if (ag->ag_gate != ag_cors->ag_gate
				    && !(ag->ag_state & AGS_FINE_GATE)
				    && !(ag_cors->ag_state & AGS_CORS_GATE)) {
					ag_out(ag, out);
					break;
				}

				/* If the coarse route has a good enough
				 * metric, it suppresses the target.
				 */
				if (ag_cors->ag_pref <= ag->ag_pref) {
				    if (ag_cors->ag_seqno > ag->ag_seqno)
					ag_cors->ag_seqno = ag->ag_seqno;
				    if (AG_IS_REDUN(ag->ag_state)
					&& ag_cors->ag_mask==ag->ag_mask<<1) {
					if (ag_cors->ag_dst_h == dst_h)
					    ag_cors->ag_state |= AGS_REDUN0;
					else
					    ag_cors->ag_state |= AGS_REDUN1;
				    }
				    if (ag->ag_tag != ag_cors->ag_tag)
					    ag_cors->ag_tag = 0;
				    if (ag->ag_nhop != ag_cors->ag_nhop)
					    ag_cors->ag_nhop = 0;
				    break;
				}
			}
		}

		/* That route has either been output or suppressed */
		ag_cors = ag->ag_cors;
		ag_del(ag);
	}

	CHECK_AG();
}


/* Try to aggregate a route with previous routes.
 */
void
ag_check(naddr	dst,
	 naddr	mask,
	 naddr	gate,
	 naddr	nhop,
	 char	metric,
	 char	pref,
	 u_int	seqno,
	 u_short tag,
	 u_short state,
	 void (*out)(struct ag_info *))	/* output using this */
{
	struct ag_info *ag, *nag, *ag_cors;
	naddr xaddr;
	int x;

	NTOHL(dst);

	/* Punt non-contiguous subnet masks.
	 *
	 * (X & -X) contains a single bit if and only if X is a power of 2.
	 * (X + (X & -X)) == 0 if and only if X is a power of 2.
	 */
	if ((mask & -mask) + mask != 0) {
		struct ag_info nc_ag;

		nc_ag.ag_dst_h = dst;
		nc_ag.ag_mask = mask;
		nc_ag.ag_gate = gate;
		nc_ag.ag_nhop = nhop;
		nc_ag.ag_metric = metric;
		nc_ag.ag_pref = pref;
		nc_ag.ag_tag = tag;
		nc_ag.ag_state = state;
		nc_ag.ag_seqno = seqno;
		out(&nc_ag);
		return;
	}

	/* Search for the right slot in the aggregation table.
	 */
	ag_cors = 0;
	ag = ag_corsest;
	while (ag != 0) {
		if (ag->ag_mask >= mask)
			break;

		/* Suppress old routes (i.e. combine with compatible routes
		 * with coarser masks) as we look for the right slot in the
		 * aggregation table for the new route.
		 * A route to an address less than the current destination
		 * will not be affected by the current route or any route
		 * seen hereafter.  That means it is safe to suppress it.
		 * This check keeps poor routes (eg. with large hop counts)
		 * from preventing suppresion of finer routes.
		 */
		if (ag_cors != 0
		    && ag->ag_dst_h < dst
		    && (ag->ag_state & AGS_SUPPRESS)
		    && ag_cors->ag_pref <= ag->ag_pref
		    && (ag->ag_dst_h & ag_cors->ag_mask) == ag_cors->ag_dst_h
		    && (ag_cors->ag_gate == ag->ag_gate
			|| (ag->ag_state & AGS_FINE_GATE)
			|| (ag_cors->ag_state & AGS_CORS_GATE))) {
			if (ag_cors->ag_seqno > ag->ag_seqno)
				ag_cors->ag_seqno = ag->ag_seqno;
			if (AG_IS_REDUN(ag->ag_state)
			    && ag_cors->ag_mask==ag->ag_mask<<1) {
				if (ag_cors->ag_dst_h == dst)
					ag_cors->ag_state |= AGS_REDUN0;
				else
					ag_cors->ag_state |= AGS_REDUN1;
			}
			if (ag->ag_tag != ag_cors->ag_tag)
				ag_cors->ag_tag = 0;
			if (ag->ag_nhop != ag_cors->ag_nhop)
				ag_cors->ag_nhop = 0;
			ag_del(ag);
			CHECK_AG();
		} else {
			ag_cors = ag;
		}
		ag = ag_cors->ag_fine;
	}

	/* If we find the even/odd twin of the new route, and if the
	 * masks and so forth are equal, we can aggregate them.
	 * We can probably promote one of the pair.
	 *
	 * Since the routes are encountered in lexical order,
	 * the new route must be odd.  However, the second or later
	 * times around this loop, it could be the even twin promoted
	 * from the even/odd pair of twins of the finer route.
	 */
	while (ag != 0
	       && ag->ag_mask == mask
	       && ((ag->ag_dst_h ^ dst) & (mask<<1)) == 0) {

		/* Here we know the target route and the route in the current
		 * slot have the same netmasks and differ by at most the
		 * last bit.  They are either for the same destination, or
		 * for an even/odd pair of destinations.
		 */
		if (ag->ag_dst_h == dst) {
			/* We have two routes to the same destination.
			 * Routes are encountered in lexical order, so a
			 * route is never promoted until the parent route is
			 * already present.  So we know that the new route is
			 * a promoted pair and the route already in the slot
			 * is the explicit route.
			 *
			 * Prefer the best route if their metrics differ,
			 * or the promoted one if not, following a sort
			 * of longest-match rule.
			 */
			if (pref <= ag->ag_pref) {
				ag->ag_gate = gate;
				ag->ag_nhop = nhop;
				ag->ag_tag = tag;
				ag->ag_metric = metric;
				ag->ag_pref = pref;
				x = ag->ag_state;
				ag->ag_state = state;
				state = x;
			}

			/* The sequence number controls flash updating,
			 * and should be the smaller of the two.
			 */
			if (ag->ag_seqno > seqno)
				ag->ag_seqno = seqno;

			/* some bits are set if they are set on either route */
			ag->ag_state |= (state & (AGS_PROMOTE_EITHER
						  | AGS_REDUN0 | AGS_REDUN1));
			return;
		}

		/* If one of the routes can be promoted and the other can
		 * be suppressed, it may be possible to combine them or
		 * worthwhile to promote one.
		 *
		 * Note that any route that can be promoted is always
		 * marked to be eligible to be suppressed.
		 */
		if (!((state & AGS_PROMOTE)
		      && (ag->ag_state & AGS_SUPPRESS))
		    && !((ag->ag_state & AGS_PROMOTE)
			 && (state & AGS_SUPPRESS)))
			break;

		/* A pair of even/odd twin routes can be combined
		 * if either is redundant, or if they are via the
		 * same gateway and have the same metric.
		 */
		if (AG_IS_REDUN(ag->ag_state)
		    || AG_IS_REDUN(state)
		    || (ag->ag_gate == gate
			&& ag->ag_pref == pref
			&& (state & ag->ag_state & AGS_PROMOTE) != 0)) {

			/* We have both the even and odd pairs.
			 * Since the routes are encountered in order,
			 * the route in the slot must be the even twin.
			 *
			 * Combine and promote the pair of routes.
			 */
			if (seqno > ag->ag_seqno)
				seqno = ag->ag_seqno;
			if (!AG_IS_REDUN(state))
				state &= ~AGS_REDUN1;
			if (AG_IS_REDUN(ag->ag_state))
				state |= AGS_REDUN0;
			else
				state &= ~AGS_REDUN0;
			state |= (ag->ag_state & AGS_PROMOTE_EITHER);
			if (ag->ag_tag != tag)
				tag = 0;
			if (ag->ag_nhop != nhop)
				nhop = 0;

			/* Get rid of the even twin that was already
			 * in the slot.
			 */
			ag_del(ag);

		} else if (ag->ag_pref >= pref
			   && (ag->ag_state & AGS_PROMOTE)) {
			/* If we cannot combine the pair, maybe the route
			 * with the worse metric can be promoted.
			 *
			 * Promote the old, even twin, by giving its slot
			 * in the table to the new, odd twin.
			 */
			ag->ag_dst_h = dst;

			xaddr = ag->ag_gate;
			ag->ag_gate = gate;
			gate = xaddr;

			xaddr = ag->ag_nhop;
			ag->ag_nhop = nhop;
			nhop = xaddr;

			x = ag->ag_tag;
			ag->ag_tag = tag;
			tag = x;

			x = ag->ag_state;
			ag->ag_state = state;
			state = x;
			if (!AG_IS_REDUN(state))
				state &= ~AGS_REDUN0;

			x = ag->ag_metric;
			ag->ag_metric = metric;
			metric = x;

			x = ag->ag_pref;
			ag->ag_pref = pref;
			pref = x;

			if (seqno >= ag->ag_seqno)
				seqno = ag->ag_seqno;
			else
				ag->ag_seqno = seqno;

		} else {
			if (!(state & AGS_PROMOTE))
				break;	/* cannot promote either twin */

			/* promote the new, odd twin by shaving its
			 * mask and address.
			 */
			if (seqno > ag->ag_seqno)
				seqno = ag->ag_seqno;
			else
				ag->ag_seqno = seqno;
			if (!AG_IS_REDUN(state))
				state &= ~AGS_REDUN1;
		}

		mask <<= 1;
		dst &= mask;

		if (ag_cors == 0) {
			ag = ag_corsest;
			break;
		}
		ag = ag_cors;
		ag_cors = ag->ag_cors;
	}

	/* When we can no longer promote and combine routes,
	 * flush the old route in the target slot.  Also flush
	 * any finer routes that we know will never be aggregated by
	 * the new route.
	 *
	 * In case we moved toward coarser masks,
	 * get back where we belong
	 */
	if (ag != 0
	    && ag->ag_mask < mask) {
		ag_cors = ag;
		ag = ag->ag_fine;
	}

	/* Empty the target slot
	 */
	if (ag != 0 && ag->ag_mask == mask) {
		ag_flush(ag->ag_dst_h, ag->ag_mask, out);
		ag = (ag_cors == 0) ? ag_corsest : ag_cors->ag_fine;
	}

#ifdef DEBUG_AG
	(void)fflush(stderr);
	if (ag == 0 && ag_cors != ag_finest)
		abort();
	if (ag_cors == 0 && ag != ag_corsest)
		abort();
	if (ag != 0 && ag->ag_cors != ag_cors)
		abort();
	if (ag_cors != 0 && ag_cors->ag_fine != ag)
		abort();
	CHECK_AG();
#endif

	/* Save the new route on the end of the table.
	 */
	nag = ag_avail;
	ag_avail = nag->ag_fine;

	nag->ag_dst_h = dst;
	nag->ag_mask = mask;
	nag->ag_gate = gate;
	nag->ag_nhop = nhop;
	nag->ag_metric = metric;
	nag->ag_pref = pref;
	nag->ag_tag = tag;
	nag->ag_state = state;
	nag->ag_seqno = seqno;

	nag->ag_fine = ag;
	if (ag != 0)
		ag->ag_cors = nag;
	else
		ag_finest = nag;
	nag->ag_cors = ag_cors;
	if (ag_cors == 0)
		ag_corsest = nag;
	else
		ag_cors->ag_fine = nag;
	CHECK_AG();
}


static char *
rtm_type_name(u_char type)
{
	static char *rtm_types[] = {
		"RTM_ADD",
		"RTM_DELETE",
		"RTM_CHANGE",
		"RTM_GET",
		"RTM_LOSING",
		"RTM_REDIRECT",
		"RTM_MISS",
		"RTM_LOCK",
		"RTM_OLDADD",
		"RTM_OLDDEL",
		"RTM_RESOLVE",
		"RTM_NEWADDR",
		"RTM_DELADDR",
		"RTM_IFINFO"
	};
	static char name0[10];


	if (type > sizeof(rtm_types)/sizeof(rtm_types[0])
	    || type == 0) {
		sprintf(name0, "RTM type %#x", type);
		return name0;
	} else {
		return rtm_types[type-1];
	}
}


/* Trim a mask in a sockaddr
 *	Produce a length of 0 for an address of 0.
 *	Otherwise produce the index of the first zero byte.
 */
void
#ifdef _HAVE_SIN_LEN
masktrim(struct sockaddr_in *ap)
#else
masktrim(struct sockaddr_in_new *ap)
#endif
{
	register char *cp;

	if (ap->sin_addr.s_addr == 0) {
		ap->sin_len = 0;
		return;
	}
	cp = (char *)(&ap->sin_addr.s_addr+1);
	while (*--cp == 0)
		continue;
	ap->sin_len = cp - (char*)ap + 1;
}


/* Tell the kernel to add, delete or change a route
 */
static void
rtioctl(int action,			/* RTM_DELETE, etc */
	naddr dst,
	naddr gate,
	naddr mask,
	int metric,
	int flags)
{
	struct {
		struct rt_msghdr w_rtm;
		struct sockaddr_in w_dst;
		struct sockaddr_in w_gate;
#ifdef _HAVE_SA_LEN
		struct sockaddr_in w_mask;
#else
		struct sockaddr_in_new w_mask;
#endif
	} w;
	long cc;

again:
	bzero(&w, sizeof(w));
	w.w_rtm.rtm_msglen = sizeof(w);
	w.w_rtm.rtm_version = RTM_VERSION;
	w.w_rtm.rtm_type = action;
	w.w_rtm.rtm_flags = flags;
	w.w_rtm.rtm_seq = ++rt_sock_seqno;
	w.w_rtm.rtm_addrs = RTA_DST|RTA_GATEWAY;
	if (metric != 0) {
		w.w_rtm.rtm_rmx.rmx_hopcount = metric;
		w.w_rtm.rtm_inits |= RTV_HOPCOUNT;
	}
	w.w_dst.sin_family = AF_INET;
	w.w_dst.sin_addr.s_addr = dst;
	w.w_gate.sin_family = AF_INET;
	w.w_gate.sin_addr.s_addr = gate;
#ifdef _HAVE_SA_LEN
	w.w_dst.sin_len = sizeof(w.w_dst);
	w.w_gate.sin_len = sizeof(w.w_gate);
#endif
	if (mask == HOST_MASK) {
		w.w_rtm.rtm_flags |= RTF_HOST;
		w.w_rtm.rtm_msglen -= sizeof(w.w_mask);
	} else {
		w.w_rtm.rtm_addrs |= RTA_NETMASK;
		w.w_mask.sin_addr.s_addr = htonl(mask);
#ifdef _HAVE_SA_LEN
		masktrim(&w.w_mask);
		if (w.w_mask.sin_len == 0)
			w.w_mask.sin_len = sizeof(long);
		w.w_rtm.rtm_msglen -= (sizeof(w.w_mask) - w.w_mask.sin_len);
#endif
	}

	if (TRACEKERNEL)
		trace_kernel("write kernel %s %s->%s metric=%d flags=%#x\n",
			     rtm_type_name(action),
			     addrname(dst, mask, 0), naddr_ntoa(gate),
			     metric, flags);

#ifndef NO_INSTALL
	cc = write(rt_sock, &w, w.w_rtm.rtm_msglen);
	if (cc == w.w_rtm.rtm_msglen)
		return;
	if (cc < 0) {
		if (errno == ESRCH
		    && (action == RTM_CHANGE || action == RTM_DELETE)) {
			trace_act("route to %s disappeared before %s\n",
				  addrname(dst, mask, 0),
				  rtm_type_name(action));
			if (action == RTM_CHANGE) {
				action = RTM_ADD;
				goto again;
			}
			return;
		}
		msglog("write(rt_sock) %s %s --> %s: %s",
		       rtm_type_name(action),
		       addrname(dst, mask, 0), naddr_ntoa(gate),
		       strerror(errno));
	} else {
		msglog("write(rt_sock) wrote %d instead of %d",
		       cc, w.w_rtm.rtm_msglen);
	}
#endif
}


#define KHASH_SIZE 71			/* should be prime */
#define KHASH(a,m) khash_bins[((a) ^ (m)) % KHASH_SIZE]
static struct khash {
	struct khash *k_next;
	naddr	k_dst;
	naddr	k_mask;
	naddr	k_gate;
	short	k_metric;
	u_short	k_state;
#define	    KS_NEW	0x001
#define	    KS_DELETE	0x002
#define	    KS_ADD	0x004		/* add to the kernel */
#define	    KS_CHANGE	0x008		/* tell kernel to change the route */
#define	    KS_DEL_ADD	0x010		/* delete & add to change the kernel */
#define	    KS_STATIC	0x020		/* Static flag in kernel */
#define	    KS_GATEWAY	0x040		/* G flag in kernel */
#define	    KS_DYNAMIC	0x080		/* result of redirect */
#define	    KS_DELETED	0x100		/* already deleted */
	time_t	k_keep;
#define	    K_KEEP_LIM	30
	time_t	k_redirect_time;
} *khash_bins[KHASH_SIZE];


static struct khash*
kern_find(naddr dst, naddr mask, struct khash ***ppk)
{
	struct khash *k, **pk;

	for (pk = &KHASH(dst,mask); (k = *pk) != 0; pk = &k->k_next) {
		if (k->k_dst == dst && k->k_mask == mask)
			break;
	}
	if (ppk != 0)
		*ppk = pk;
	return k;
}


static struct khash*
kern_add(naddr dst, naddr mask)
{
	struct khash *k, **pk;

	k = kern_find(dst, mask, &pk);
	if (k != 0)
		return k;

	k = (struct khash *)malloc(sizeof(*k));

	bzero(k, sizeof(*k));
	k->k_dst = dst;
	k->k_mask = mask;
	k->k_state = KS_NEW;
	k->k_keep = now.tv_sec;
	*pk = k;

	return k;
}


/* If a kernel route has a non-zero metric, check that it is still in the
 *	daemon table, and not deleted by interfaces coming and going.
 */
static void
kern_check_static(struct khash *k,
		  struct interface *ifp)
{
	struct rt_entry *rt;
	naddr int_addr;

	if (k->k_metric == 0)
		return;

	int_addr = (ifp != 0) ? ifp->int_addr : loopaddr;

	rt = rtget(k->k_dst, k->k_mask);
	if (rt != 0) {
		if (!(rt->rt_state & RS_STATIC))
			rtchange(rt, rt->rt_state | RS_STATIC,
				 k->k_gate, int_addr,
				 k->k_metric, 0, ifp, now.tv_sec, 0);
	} else {
		rtadd(k->k_dst, k->k_mask, k->k_gate, int_addr,
		      k->k_metric, 0, RS_STATIC, ifp);
	}
}


/* add a route the kernel told us
 */
static void
rtm_add(struct rt_msghdr *rtm,
	struct rt_addrinfo *info,
	time_t keep)
{
	struct khash *k;
	struct interface *ifp;
	naddr mask;


	if (rtm->rtm_flags & RTF_HOST) {
		mask = HOST_MASK;
	} else if (INFO_MASK(info) != 0) {
		mask = ntohl(S_ADDR(INFO_MASK(info)));
	} else {
		msglog("punt %s without mask",
		       rtm_type_name(rtm->rtm_type));
		return;
	}

	if (INFO_GATE(info) == 0
	    || INFO_GATE(info)->sa_family != AF_INET) {
		msglog("punt %s without gateway",
		       rtm_type_name(rtm->rtm_type));
		return;
	}

	k = kern_add(S_ADDR(INFO_DST(info)), mask);
	if (k->k_state & KS_NEW)
		k->k_keep = now.tv_sec+keep;
	k->k_gate = S_ADDR(INFO_GATE(info));
	k->k_metric = rtm->rtm_rmx.rmx_hopcount;
	if (k->k_metric < 0)
		k->k_metric = 0;
	else if (k->k_metric > HOPCNT_INFINITY)
		 k->k_metric = HOPCNT_INFINITY;
	k->k_state &= ~(KS_DELETED | KS_GATEWAY | KS_STATIC | KS_NEW);
	if (rtm->rtm_flags & RTF_GATEWAY)
		k->k_state |= KS_GATEWAY;
	if (rtm->rtm_flags & RTF_STATIC)
		k->k_state |= KS_STATIC;

	if (0 != (rtm->rtm_flags & (RTF_DYNAMIC | RTF_MODIFIED))) {
		if (supplier) {
			/* Routers are not supposed to listen to redirects,
			 * so delete it.
			 */
			k->k_state &= ~KS_DYNAMIC;
			k->k_state |= KS_DELETE;
			LIM_SEC(need_kern, 0);
			trace_act("mark redirected %s --> %s for deletion"
				  " since this is a router\n",
				  addrname(k->k_dst, k->k_mask, 0),
				  naddr_ntoa(k->k_gate));
		} else {
			k->k_state |= KS_DYNAMIC;
			k->k_redirect_time = now.tv_sec;
		}
		return;
	}

	/* If it is not a static route, quit until the next comparison
	 * between the kernel and daemon tables, when it will be deleted.
	 */
	if (!(k->k_state & KS_STATIC)) {
		k->k_state |= KS_DELETE;
		LIM_SEC(need_kern, k->k_keep);
		return;
	}

	/* Put static routes with real metrics into the daemon table so
	 * they can be advertised.
	 *
	 * Find the interface concerned
	 */
	ifp = iflookup(k->k_gate);
	if (ifp == 0) {
		/* if there is no known interface,
		 * maybe there is a new interface
		 */
		ifinit();
		ifp = iflookup(k->k_gate);
		if (ifp == 0)
			msglog("static route %s --> %s impossibly lacks ifp",
			       addrname(S_ADDR(INFO_DST(info)), mask, 0),
			       naddr_ntoa(k->k_gate));
	}

	kern_check_static(k, ifp);
}


/* deal with packet loss
 */
static void
rtm_lose(struct rt_msghdr *rtm,
	 struct rt_addrinfo *info)
{
	if (INFO_GATE(info) == 0
	    || INFO_GATE(info)->sa_family != AF_INET) {
		msglog("punt %s without gateway",
		       rtm_type_name(rtm->rtm_type));
		return;
	}

	if (!supplier)
		rdisc_age(S_ADDR(INFO_GATE(info)));

	age(S_ADDR(INFO_GATE(info)));
}


/* Clean the kernel table by copying it to the daemon image.
 * Eventually the daemon will delete any extra routes.
 */
void
flush_kern(void)
{
	size_t needed;
	int mib[6];
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;
	struct interface *ifp;
	static struct sockaddr_in gate_sa;
	struct rt_addrinfo info;


	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, 0, &needed, 0, 0) < 0) {
		DBGERR(1,"RT_DUMP-sysctl-estimate");
		return;
	}
	buf = malloc(needed);
	if (sysctl(mib, 6, buf, &needed, 0, 0) < 0)
		BADERR(1,"RT_DUMP");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;

		rt_xaddrs(&info,
			  (struct sockaddr *)(rtm+1),
			  (struct sockaddr *)(next + rtm->rtm_msglen),
			  rtm->rtm_addrs);

		if (INFO_DST(&info) == 0
		    || INFO_DST(&info)->sa_family != AF_INET)
			continue;

		/* ignore ARP table entries on systems with a merged route
		 * and ARP table.
		 */
		if (rtm->rtm_flags & RTF_LLINFO)
			continue;

		if (INFO_GATE(&info) == 0)
			continue;
		if (INFO_GATE(&info)->sa_family != AF_INET) {
			if (INFO_GATE(&info)->sa_family != AF_LINK)
				continue;
			ifp = ifwithindex(((struct sockaddr_dl *)
					   INFO_GATE(&info))->sdl_index);
			if (ifp == 0)
				continue;
			if ((ifp->int_if_flags & IFF_POINTOPOINT)
			    || S_ADDR(INFO_DST(&info)) == ifp->int_addr)
				gate_sa.sin_addr.s_addr = ifp->int_addr;
			else
				gate_sa.sin_addr.s_addr = htonl(ifp->int_net);
#ifdef _HAVE_SA_LEN
			gate_sa.sin_len = sizeof(gate_sa);
#endif
			gate_sa.sin_family = AF_INET;
			INFO_GATE(&info) = (struct sockaddr *)&gate_sa;
		}

		/* ignore multicast addresses
		 */
		if (IN_MULTICAST(ntohl(S_ADDR(INFO_DST(&info)))))
			continue;

		/* Note static routes and interface routes, and also
		 * preload the image of the kernel table so that
		 * we can later clean it, as well as avoid making
		 * unneeded changes.  Keep the old kernel routes for a
		 * few seconds to allow a RIP or router-discovery
		 * response to be heard.
		 */
		rtm_add(rtm,&info,MIN_WAITTIME);
	}
	free(buf);
}


/* Listen to announcements from the kernel
 */
void
read_rt(void)
{
	long cc;
	struct interface *ifp;
	naddr mask;
	union {
		struct {
			struct rt_msghdr rtm;
			struct sockaddr addrs[RTAX_MAX];
		} r;
		struct if_msghdr ifm;
	} m;
	char str[100], *strp;
	struct rt_addrinfo info;


	for (;;) {
		cc = read(rt_sock, &m, sizeof(m));
		if (cc <= 0) {
			if (cc < 0 && errno != EWOULDBLOCK)
				LOGERR("read(rt_sock)");
			return;
		}

		if (m.r.rtm.rtm_version != RTM_VERSION) {
			msglog("bogus routing message version %d",
			       m.r.rtm.rtm_version);
			continue;
		}

		/* Ignore our own results.
		 */
		if (m.r.rtm.rtm_type <= RTM_CHANGE
		    && m.r.rtm.rtm_pid == mypid) {
			static int complained = 0;
			if (!complained) {
				msglog("receiving our own change messages");
				complained = 1;
			}
			continue;
		}

		if (m.r.rtm.rtm_type == RTM_IFINFO
		    || m.r.rtm.rtm_type == RTM_NEWADDR
		    || m.r.rtm.rtm_type == RTM_DELADDR) {
			ifp = ifwithindex(m.ifm.ifm_index);
			if (ifp == 0)
				trace_act("note %s with flags %#x"
					  " for index #%d\n",
					  rtm_type_name(m.r.rtm.rtm_type),
					  m.ifm.ifm_flags,
					  m.ifm.ifm_index);
			else
				trace_act("note %s with flags %#x for %s\n",
					  rtm_type_name(m.r.rtm.rtm_type),
					  m.ifm.ifm_flags,
					  ifp->int_name);

			/* After being informed of a change to an interface,
			 * check them all now if the check would otherwise
			 * be a long time from now, if the interface is
			 * not known, or if the interface has been turned
			 * off or on.
			 */
			if (ifinit_timer.tv_sec-now.tv_sec>=CHECK_BAD_INTERVAL
			    || ifp == 0
			    || ((ifp->int_if_flags ^ m.ifm.ifm_flags)
				& IFF_UP_RUNNING) != 0)
				ifinit_timer.tv_sec = now.tv_sec;
			continue;
		}

		strcpy(str, rtm_type_name(m.r.rtm.rtm_type));
		strp = &str[strlen(str)];
		if (m.r.rtm.rtm_type <= RTM_CHANGE)
			strp += sprintf(strp," from pid %d",m.r.rtm.rtm_pid);

		rt_xaddrs(&info, m.r.addrs, &m.r.addrs[RTAX_MAX],
			  m.r.rtm.rtm_addrs);

		if (INFO_DST(&info) == 0) {
			trace_act("ignore %s without dst\n", str);
			continue;
		}

		if (INFO_DST(&info)->sa_family != AF_INET) {
			trace_act("ignore %s for AF %d\n", str,
				  INFO_DST(&info)->sa_family);
			continue;
		}

		mask = ((INFO_MASK(&info) != 0)
			? ntohl(S_ADDR(INFO_MASK(&info)))
			: (m.r.rtm.rtm_flags & RTF_HOST)
			? HOST_MASK
			: std_mask(S_ADDR(INFO_DST(&info))));

		strp += sprintf(strp, ": %s",
				addrname(S_ADDR(INFO_DST(&info)), mask, 0));

		if (IN_MULTICAST(ntohl(S_ADDR(INFO_DST(&info))))) {
			trace_act("ignore multicast %s\n", str);
			continue;
		}

		if (INFO_GATE(&info) != 0
		    && INFO_GATE(&info)->sa_family == AF_INET)
			strp += sprintf(strp, " --> %s",
					saddr_ntoa(INFO_GATE(&info)));

		if (INFO_AUTHOR(&info) != 0)
			strp += sprintf(strp, " by authority of %s",
					saddr_ntoa(INFO_AUTHOR(&info)));

		switch (m.r.rtm.rtm_type) {
		case RTM_ADD:
		case RTM_CHANGE:
		case RTM_REDIRECT:
			if (m.r.rtm.rtm_errno != 0) {
				trace_act("ignore %s with \"%s\" error\n",
					  str, strerror(m.r.rtm.rtm_errno));
			} else {
				trace_act("%s\n", str);
				rtm_add(&m.r.rtm,&info,0);
			}
			break;

		case RTM_DELETE:
			if (m.r.rtm.rtm_errno != 0) {
				trace_act("ignore %s with \"%s\" error\n",
					  str, strerror(m.r.rtm.rtm_errno));
			} else {
				trace_act("%s\n", str);
				del_static(S_ADDR(INFO_DST(&info)), mask, 1);
			}
			break;

		case RTM_LOSING:
			trace_act("%s\n", str);
			rtm_lose(&m.r.rtm,&info);
			break;

		default:
			trace_act("ignore %s\n", str);
			break;
		}
	}
}


/* after aggregating, note routes that belong in the kernel
 */
static void
kern_out(struct ag_info *ag)
{
	struct khash *k;


	/* Do not install bad routes if they are not already present.
	 * This includes routes that had RS_NET_SYN for interfaces that
	 * recently died.
	 */
	if (ag->ag_metric == HOPCNT_INFINITY) {
		k = kern_find(htonl(ag->ag_dst_h), ag->ag_mask, 0);
		if (k == 0)
			return;
	} else {
		k = kern_add(htonl(ag->ag_dst_h), ag->ag_mask);
	}

	if (k->k_state & KS_NEW) {
		/* will need to add new entry to the kernel table */
		k->k_state = KS_ADD;
		if (ag->ag_state & AGS_GATEWAY)
			k->k_state |= KS_GATEWAY;
		k->k_gate = ag->ag_gate;
		k->k_metric = ag->ag_metric;
		return;
	}

	if (k->k_state & KS_STATIC)
		return;

	/* modify existing kernel entry if necessary */
	if (k->k_gate != ag->ag_gate
	    || k->k_metric != ag->ag_metric) {
		k->k_gate = ag->ag_gate;
		k->k_metric = ag->ag_metric;
		k->k_state |= KS_CHANGE;
	}

	if (k->k_state & KS_DYNAMIC) {
		k->k_state &= ~KS_DYNAMIC;
		k->k_state |= (KS_ADD | KS_DEL_ADD);
	}

	if ((k->k_state & KS_GATEWAY)
	    && !(ag->ag_state & AGS_GATEWAY)) {
		k->k_state &= ~KS_GATEWAY;
		k->k_state |= (KS_ADD | KS_DEL_ADD);
	} else if (!(k->k_state & KS_GATEWAY)
		   && (ag->ag_state & AGS_GATEWAY)) {
		k->k_state |= KS_GATEWAY;
		k->k_state |= (KS_ADD | KS_DEL_ADD);
	}

	/* Deleting-and-adding is necessary to change aspects of a route.
	 * Just delete instead of deleting and then adding a bad route.
	 * Otherwise, we want to keep the route in the kernel.
	 */
	if (k->k_metric == HOPCNT_INFINITY
	    && (k->k_state & KS_DEL_ADD))
		k->k_state |= KS_DELETE;
	else
		k->k_state &= ~KS_DELETE;
#undef RT
}


/* ARGSUSED */
static int
walk_kern(struct radix_node *rn,
	  struct walkarg *w)
{
#define RT ((struct rt_entry *)rn)
	char metric, pref;
	u_int ags = 0;


	/* Do not install synthetic routes */
	if (RT->rt_state & RS_NET_SYN)
		return 0;

	if (!(RT->rt_state & RS_IF)) {
		ags |= (AGS_GATEWAY | AGS_SUPPRESS | AGS_PROMOTE);

	} else {
		/* Do not install routes for "external" remote interfaces.
		 */
		if (RT->rt_ifp != 0 && (RT->rt_ifp->int_state & IS_EXTERNAL))
			return 0;

		ags |= AGS_IF;

		/* If it is not an interface, or an alias for an interface,
		 * it must be a "gateway."
		 *
		 * If it is a "remote" interface, it is also a "gateway" to
		 * the kernel if is not a alias.
		 */
		if (RT->rt_ifp == 0
		    || ((RT->rt_ifp->int_state & IS_REMOTE)
			&& RT->rt_ifp->int_metric == 0))
			ags |= (AGS_GATEWAY | AGS_SUPPRESS | AGS_PROMOTE);
	}

	if (RT->rt_state & RS_RDISC)
		ags |= AGS_CORS_GATE;

	/* aggregate good routes without regard to their metric */
	pref = 1;
	metric = RT->rt_metric;
	if (metric == HOPCNT_INFINITY) {
		/* if the route is dead, so try hard to aggregate. */
		pref = HOPCNT_INFINITY;
		ags |= (AGS_FINE_GATE | AGS_SUPPRESS);
	}

	ag_check(RT->rt_dst, RT->rt_mask, RT->rt_gate, 0,
		 metric,pref, 0, 0, ags, kern_out);
	return 0;
#undef RT
}


/* Update the kernel table to match the daemon table.
 */
static void
fix_kern(void)
{
	int i, flags;
	struct khash *k, **pk;


	need_kern = age_timer;

	/* Walk daemon table, updating the copy of the kernel table.
	 */
	(void)rn_walktree(rhead, walk_kern, 0);
	ag_flush(0,0,kern_out);

	for (i = 0; i < KHASH_SIZE; i++) {
		for (pk = &khash_bins[i]; (k = *pk) != 0; ) {
			/* Do not touch static routes */
			if (k->k_state & KS_STATIC) {
				kern_check_static(k,0);
				pk = &k->k_next;
				continue;
			}

			/* check hold on routes deleted by the operator */
			if (k->k_keep > now.tv_sec) {
				LIM_SEC(need_kern, k->k_keep);
				k->k_state |= KS_DELETE;
				pk = &k->k_next;
				continue;
			}

			if ((k->k_state & (KS_DELETE | KS_DYNAMIC))
			    == KS_DELETE) {
				if (!(k->k_state & KS_DELETED))
					rtioctl(RTM_DELETE,
						k->k_dst, k->k_gate, k->k_mask,
						0, 0);
				*pk = k->k_next;
				free(k);
				continue;
			}

			if (0 != (k->k_state&(KS_ADD|KS_CHANGE|KS_DEL_ADD))) {
				if (k->k_state & KS_DEL_ADD) {
					rtioctl(RTM_DELETE,
						k->k_dst,k->k_gate,k->k_mask,
						0, 0);
					k->k_state &= ~KS_DYNAMIC;
				}

				flags = 0;
				if (0 != (k->k_state&(KS_GATEWAY|KS_DYNAMIC)))
					flags |= RTF_GATEWAY;

				if (k->k_state & KS_ADD) {
					rtioctl(RTM_ADD,
						k->k_dst, k->k_gate, k->k_mask,
						k->k_metric, flags);
				} else if (k->k_state & KS_CHANGE) {
					rtioctl(RTM_CHANGE,
						k->k_dst,k->k_gate,k->k_mask,
						k->k_metric, flags);
				}
				k->k_state &= ~(KS_ADD|KS_CHANGE|KS_DEL_ADD);
			}

			/* Mark this route to be deleted in the next cycle.
			 * This deletes routes that disappear from the
			 * daemon table, since the normal aging code
			 * will clear the bit for routes that have not
			 * disappeared from the daemon table.
			 */
			k->k_state |= KS_DELETE;
			pk = &k->k_next;
		}
	}
}


/* Delete a static route in the image of the kernel table.
 */
void
del_static(naddr dst,
	   naddr mask,
	   int gone)
{
	struct khash *k;
	struct rt_entry *rt;

	/* Just mark it in the table to be deleted next time the kernel
	 * table is updated.
	 * If it has already been deleted, mark it as such, and set its
	 * keep-timer so that it will not be deleted again for a while.
	 * This lets the operator delete a route added by the daemon
	 * and add a replacement.
	 */
	k = kern_find(dst, mask, 0);
	if (k != 0) {
		k->k_state &= ~(KS_STATIC | KS_DYNAMIC);
		k->k_state |= KS_DELETE;
		if (gone) {
			k->k_state |= KS_DELETED;
			k->k_keep = now.tv_sec + K_KEEP_LIM;
		}
	}

	rt = rtget(dst, mask);
	if (rt != 0 && (rt->rt_state & RS_STATIC))
		rtbad(rt);
}


/* Delete all routes generated from ICMP Redirects that use a given gateway,
 * as well as old redirected routes.
 */
void
del_redirects(naddr bad_gate,
	      time_t old)
{
	int i;
	struct khash *k;


	for (i = 0; i < KHASH_SIZE; i++) {
		for (k = khash_bins[i]; k != 0; k = k->k_next) {
			if (!(k->k_state & KS_DYNAMIC)
			    || (k->k_state & KS_STATIC))
				continue;

			if (k->k_gate != bad_gate
			    && k->k_redirect_time > old
			    && !supplier)
				continue;

			k->k_state |= KS_DELETE;
			k->k_state &= ~KS_DYNAMIC;
			need_kern.tv_sec = now.tv_sec;
			trace_act("mark redirected %s --> %s for deletion\n",
				  addrname(k->k_dst, k->k_mask, 0),
				  naddr_ntoa(k->k_gate));
		}
	}
}


/* Start the daemon tables.
 */
void
rtinit(void)
{
	extern int max_keylen;
	int i;
	struct ag_info *ag;

	/* Initialize the radix trees */
	max_keylen = sizeof(struct sockaddr_in);
	rn_init();
	rn_inithead((void**)&rhead, 32);

	/* mark all of the slots in the table free */
	ag_avail = ag_slots;
	for (ag = ag_slots, i = 1; i < NUM_AG_SLOTS; i++) {
		ag->ag_fine = ag+1;
		ag++;
	}
}


#ifdef _HAVE_SIN_LEN
static struct sockaddr_in dst_sock = {sizeof(dst_sock), AF_INET};
static struct sockaddr_in mask_sock = {sizeof(mask_sock), AF_INET};
#else
static struct sockaddr_in_new dst_sock = {_SIN_ADDR_SIZE, AF_INET};
static struct sockaddr_in_new mask_sock = {_SIN_ADDR_SIZE, AF_INET};
#endif


void
set_need_flash(void)
{
	if (!need_flash) {
		need_flash = 1;
		/* Do not send the flash update immediately.  Wait a little
		 * while to hear from other routers.
		 */
		no_flash.tv_sec = now.tv_sec + MIN_WAITTIME;
	}
}


/* Get a particular routing table entry
 */
struct rt_entry *
rtget(naddr dst, naddr mask)
{
	struct rt_entry *rt;

	dst_sock.sin_addr.s_addr = dst;
	mask_sock.sin_addr.s_addr = mask;
	masktrim(&mask_sock);
	rt = (struct rt_entry *)rhead->rnh_lookup(&dst_sock,&mask_sock,rhead);
	if (!rt
	    || rt->rt_dst != dst
	    || rt->rt_mask != mask)
		return 0;

	return rt;
}


/* Find a route to dst as the kernel would.
 */
struct rt_entry *
rtfind(naddr dst)
{
	dst_sock.sin_addr.s_addr = dst;
	return (struct rt_entry *)rhead->rnh_matchaddr(&dst_sock, rhead);
}


/* add a route to the table
 */
void
rtadd(naddr	dst,
      naddr	mask,
      naddr	gate,			/* forward packets here */
      naddr	router,			/* on the authority of this router */
      int	metric,
      u_short	tag,
      u_int	state,			/* rs_state for the entry */
      struct interface *ifp)
{
	struct rt_entry *rt;
	naddr smask;
	int i;
	struct rt_spare *rts;

	rt = (struct rt_entry *)rtmalloc(sizeof (*rt), "rtadd");
	bzero(rt, sizeof(*rt));
	for (rts = rt->rt_spares, i = NUM_SPARES; i != 0; i--, rts++)
		rts->rts_metric = HOPCNT_INFINITY;

	rt->rt_nodes->rn_key = (caddr_t)&rt->rt_dst_sock;
	rt->rt_dst = dst;
	rt->rt_dst_sock.sin_family = AF_INET;
#ifdef _HAVE_SIN_LEN
	rt->rt_dst_sock.sin_len = dst_sock.sin_len;
#endif
	if (mask != HOST_MASK) {
		smask = std_mask(dst);
		if ((smask & ~mask) == 0 && mask > smask)
			state |= RS_SUBNET;
	}
	mask_sock.sin_addr.s_addr = mask;
	masktrim(&mask_sock);
	rt->rt_mask = mask;
	rt->rt_state = state;
	rt->rt_gate = gate;
	rt->rt_router = router;
	rt->rt_time = now.tv_sec;
	rt->rt_metric = metric;
	rt->rt_poison_metric = HOPCNT_INFINITY;
	rt->rt_tag = tag;
	rt->rt_ifp = ifp;
	rt->rt_seqno = update_seqno;

	if (++total_routes == MAX_ROUTES)
		msglog("have maximum (%d) routes", total_routes);
	if (TRACEACTIONS)
		trace_add_del("Add", rt);

	need_kern.tv_sec = now.tv_sec;
	set_need_flash();

	if (0 == rhead->rnh_addaddr(&rt->rt_dst_sock, &mask_sock,
				    rhead, rt->rt_nodes)) {
		msglog("rnh_addaddr() failed for %s mask=%#x",
		       naddr_ntoa(dst), mask);
	}
}


/* notice a changed route
 */
void
rtchange(struct rt_entry *rt,
	 u_int	state,			/* new state bits */
	 naddr	gate,			/* now forward packets here */
	 naddr	router,			/* on the authority of this router */
	 int	metric,			/* new metric */
	 u_short tag,
	 struct interface *ifp,
	 time_t	new_time,
	 char	*label)
{
	if (rt->rt_metric != metric) {
		/* Fix the kernel immediately if it seems the route
		 * has gone bad, since there may be a working route that
		 * aggregates this route.
		 */
		if (metric == HOPCNT_INFINITY) {
			need_kern.tv_sec = now.tv_sec;
			if (new_time >= now.tv_sec - EXPIRE_TIME)
				new_time = now.tv_sec - EXPIRE_TIME;
		}
		rt->rt_seqno = update_seqno;
		set_need_flash();
	}

	if (rt->rt_gate != gate) {
		need_kern.tv_sec = now.tv_sec;
		rt->rt_seqno = update_seqno;
		set_need_flash();
	}

	state |= (rt->rt_state & RS_SUBNET);

	/* Keep various things from deciding ageless routes are stale.
	 */
	if (!AGE_RT(state, ifp))
		new_time = now.tv_sec;

	if (TRACEACTIONS)
		trace_change(rt, state, gate, router, metric, tag, ifp,
			     new_time,
			     label ? label : "Chg   ");

	rt->rt_state = state;
	rt->rt_gate = gate;
	rt->rt_router = router;
	rt->rt_metric = metric;
	rt->rt_tag = tag;
	rt->rt_ifp = ifp;
	rt->rt_time = new_time;
}


/* check for a better route among the spares
 */
static struct rt_spare *
rts_better(struct rt_entry *rt)
{
	struct rt_spare *rts, *rts1;
	int i;

	/* find the best alternative among the spares */
	rts = rt->rt_spares+1;
	for (i = NUM_SPARES, rts1 = rts+1; i > 2; i--, rts1++) {
		if (BETTER_LINK(rt,rts1,rts))
			rts = rts1;
	}

	return rts;
}


/* switch to a backup route
 */
void
rtswitch(struct rt_entry *rt,
	 struct rt_spare *rts)
{
	struct rt_spare swap;
	char label[10];


	/* Do not change permanent routes */
	if (0 != (rt->rt_state & (RS_MHOME | RS_STATIC | RS_RDISC
				  | RS_NET_SYN | RS_IF)))
		return;

	/* find the best alternative among the spares */
	if (rts == 0)
		rts = rts_better(rt);

	/* Do not bother if it is not worthwhile.
	 */
	if (!BETTER_LINK(rt, rts, rt->rt_spares))
		return;

	swap = rt->rt_spares[0];
	(void)sprintf(label, "Use #%d", rts - rt->rt_spares);
	rtchange(rt, rt->rt_state & ~(RS_NET_SYN | RS_RDISC),
		 rts->rts_gate, rts->rts_router, rts->rts_metric,
		 rts->rts_tag, rts->rts_ifp, rts->rts_time, label);
	*rts = swap;
}


void
rtdelete(struct rt_entry *rt)
{
	struct khash *k;


	if (TRACEACTIONS)
		trace_add_del("Del", rt);

	k = kern_find(rt->rt_dst, rt->rt_mask, 0);
	if (k != 0) {
		k->k_state |= KS_DELETE;
		need_kern.tv_sec = now.tv_sec;
	}

	dst_sock.sin_addr.s_addr = rt->rt_dst;
	mask_sock.sin_addr.s_addr = rt->rt_mask;
	masktrim(&mask_sock);
	if (rt != (struct rt_entry *)rhead->rnh_deladdr(&dst_sock, &mask_sock,
							rhead)) {
		msglog("rnh_deladdr() failed");
	} else {
		free(rt);
		total_routes--;
	}
}


/* Get rid of a bad route, and try to switch to a replacement.
 */
void
rtbad(struct rt_entry *rt)
{
	/* Poison the route */
	rtchange(rt, rt->rt_state & ~(RS_IF | RS_LOCAL | RS_STATIC),
		 rt->rt_gate, rt->rt_router, HOPCNT_INFINITY, rt->rt_tag,
		 0, rt->rt_time, 0);

	rtswitch(rt, 0);
}


/* Junk a RS_NET_SYN or RS_LOCAL route,
 *	unless it is needed by another interface.
 */
void
rtbad_sub(struct rt_entry *rt)
{
	struct interface *ifp, *ifp1;
	struct intnet *intnetp;
	u_int state;


	ifp1 = 0;
	state = 0;

	if (rt->rt_state & RS_LOCAL) {
		/* Is this the route through loopback for the interface?
		 * If so, see if it is used by any other interfaces, such
		 * as a point-to-point interface with the same local address.
		 */
		for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
			/* Retain it if another interface needs it.
			 */
			if (ifp->int_addr == rt->rt_ifp->int_addr) {
				state |= RS_LOCAL;
				ifp1 = ifp;
				break;
			}
		}

	}

	if (!(state & RS_LOCAL)) {
		/* Retain RIPv1 logical network route if there is another
		 * interface that justifies it.
		 */
		if (rt->rt_state & RS_NET_SYN) {
			for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
				if ((ifp->int_state & IS_NEED_NET_SYN)
				    && rt->rt_mask == ifp->int_std_mask
				    && rt->rt_dst == ifp->int_std_addr) {
					state |= RS_NET_SYN;
					ifp1 = ifp;
					break;
				}
			}
		}

		/* or if there is an authority route that needs it. */
		for (intnetp = intnets;
		     intnetp != 0;
		     intnetp = intnetp->intnet_next) {
			if (intnetp->intnet_addr == rt->rt_dst
			    && intnetp->intnet_mask == rt->rt_mask) {
				state |= (RS_NET_SYN | RS_NET_INT);
				break;
			}
		}
	}

	if (ifp1 != 0 || (state & RS_NET_SYN)) {
		rtchange(rt, ((rt->rt_state & ~(RS_NET_SYN | RS_LOCAL))
			      | state),
			 rt->rt_gate, rt->rt_router, rt->rt_metric,
			 rt->rt_tag, ifp1, rt->rt_time, 0);
	} else {
		rtbad(rt);
	}
}


/* Called while walking the table looking for sick interfaces
 * or after a time change.
 */
/* ARGSUSED */
int
walk_bad(struct radix_node *rn,
	 struct walkarg *w)
{
#define RT ((struct rt_entry *)rn)
	struct rt_spare *rts;
	int i;
	time_t new_time;


	/* fix any spare routes through the interface
	 */
	rts = RT->rt_spares;
	for (i = NUM_SPARES; i != 1; i--) {
		rts++;

		if (rts->rts_ifp != 0
		    && (rts->rts_ifp->int_state & IS_BROKE)) {
			/* mark the spare route to be deleted immediately */
			new_time = rts->rts_time;
			if (new_time >= now_garbage)
				new_time = now_garbage-1;
			trace_upslot(RT, rts, rts->rts_gate,
				     rts->rts_router, 0,
				     HOPCNT_INFINITY, rts->rts_tag,
				     new_time);
			rts->rts_ifp = 0;
			rts->rts_metric = HOPCNT_INFINITY;
			rts->rts_time = new_time;
		}
	}

	/* Deal with the main route
	 */
	/* finished if it has been handled before or if its interface is ok
	 */
	if (RT->rt_ifp == 0 || !(RT->rt_ifp->int_state & IS_BROKE))
		return 0;

	/* Bad routes for other than interfaces are easy.
	 */
	if (0 == (RT->rt_state & (RS_IF | RS_NET_SYN | RS_LOCAL))) {
		rtbad(RT);
		return 0;
	}

	rtbad_sub(RT);
	return 0;
#undef RT
}


/* Check the age of an individual route.
 */
/* ARGSUSED */
static int
walk_age(struct radix_node *rn,
	   struct walkarg *w)
{
#define RT ((struct rt_entry *)rn)
	struct interface *ifp;
	struct rt_spare *rts;
	int i;


	/* age all of the spare routes, including the primary route
	 * currently in use
	 */
	rts = RT->rt_spares;
	for (i = NUM_SPARES; i != 0; i--, rts++) {

		ifp = rts->rts_ifp;
		if (i == NUM_SPARES) {
			if (!AGE_RT(RT->rt_state, ifp)) {
				/* Keep various things from deciding ageless
				 * routes are stale
				 */
				rts->rts_time = now.tv_sec;
				continue;
			}

			/* forget RIP routes after RIP has been turned off.
			 */
			if (rip_sock < 0) {
				rtdelete(RT);
				return 0;
			}
		}

		/* age failing routes
		 */
		if (age_bad_gate == rts->rts_gate
		    && rts->rts_time >= now_stale) {
			rts->rts_time -= SUPPLY_INTERVAL;
		}

		/* trash the spare routes when they go bad */
		if (rts->rts_metric < HOPCNT_INFINITY
		    && now_garbage > rts->rts_time) {
			trace_upslot(RT, rts, rts->rts_gate,
				     rts->rts_router, rts->rts_ifp,
				     HOPCNT_INFINITY, rts->rts_tag,
				     rts->rts_time);
			rts->rts_metric = HOPCNT_INFINITY;
		}
	}


	/* finished if the active route is still fresh */
	if (now_stale <= RT->rt_time)
		return 0;

	/* try to switch to an alternative */
	rtswitch(RT, 0);

	/* Delete a dead route after it has been publically mourned. */
	if (now_garbage > RT->rt_time) {
		rtdelete(RT);
		return 0;
	}

	/* Start poisoning a bad route before deleting it. */
	if (now.tv_sec - RT->rt_time > EXPIRE_TIME)
		rtchange(RT, RT->rt_state, RT->rt_gate, RT->rt_router,
			 HOPCNT_INFINITY, RT->rt_tag, RT->rt_ifp,
			 RT->rt_time, 0);
	return 0;
}


/* Watch for dead routes and interfaces.
 */
void
age(naddr bad_gate)
{
	struct interface *ifp;


	age_timer.tv_sec = now.tv_sec + (rip_sock < 0
					 ? NEVER
					 : SUPPLY_INTERVAL);

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		/* Check for dead IS_REMOTE interfaces by timing their
		 * transmissions.
		 */
		if ((ifp->int_state & IS_REMOTE)
		    && !(ifp->int_state & IS_PASSIVE)
		    && (ifp->int_state & IS_ACTIVE)) {
			LIM_SEC(age_timer, now.tv_sec+SUPPLY_INTERVAL);

			if (now.tv_sec - ifp->int_act_time > EXPIRE_TIME
			    && !(ifp->int_state & IS_BROKE)) {
				msglog("remote interface %s to %s timed out"
				       "--turned off",
				       ifp->int_name,
				       naddr_ntoa(ifp->int_addr));
				if_bad(ifp);
			}
		}
	}

	/* Age routes. */
	age_bad_gate = bad_gate;
	(void)rn_walktree(rhead, walk_age, 0);

	/* Update the kernel routing table. */
	fix_kern();
}

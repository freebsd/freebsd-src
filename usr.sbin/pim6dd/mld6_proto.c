/*
 * Copyright (C) 1998 WIDE Project.
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
 */

/*
 *  Copyright (c) 1998 by the University of Oregon.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Oregon.
 *  The name of the University of Oregon may not be used to endorse or 
 *  promote products derived from this software without specific prior 
 *  written permission.
 *
 *  THE UNIVERSITY OF OREGON DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL UO, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to 
 *  Kurt Windisch (kurtw@antc.uoregon.edu)
 *
 *  $Id: mld6_proto.c,v 1.4 2000/05/05 12:38:30 jinmei Exp $
 */
/*
 * Part of this program has been derived from PIM sparse-mode pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *  
 * The pimd program is COPYRIGHT 1998 by University of Southern California.
 *
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 * 
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6dd/mld6_proto.c,v 1.1.2.1 2000/07/15 07:36:29 kris Exp $
 */

#include "defs.h"

extern struct in6_addr in6addr_any;

typedef struct {
	mifi_t  mifi;
	struct listaddr *g;
	int    q_time;
} cbk_t;


/*
 * Forward declarations.
 */
static void DelVif __P((void *arg));
static int SetTimer __P((int mifi, struct listaddr *g));
static int DeleteTimer __P((int id));
static void SendQuery __P((void *arg));
static int SetQueryTimer __P((struct listaddr *g, int mifi, int to_expire,
			      int q_time));

/*
 * Send group membership queries on that interface if I am querier.
 */
void
query_groups(v)
	register struct uvif *v;
{
	register struct listaddr *g;
    
	v->uv_gq_timer = MLD6_QUERY_INTERVAL;
	if (v->uv_flags & VIFF_QUERIER && (v->uv_flags & VIFF_NOLISTENER) == 0)
		send_mld6(MLD6_LISTENER_QUERY, 0, &v->uv_linklocal->pa_addr,
			  NULL, (struct in6_addr *)&in6addr_any,
			  v->uv_ifindex, MLD6_QUERY_RESPONSE_INTERVAL, 0, 1);

	/*
	 * Decrement the old-hosts-present timer for each
	 * active group on that vif.
	 */
	for (g = v->uv_groups; g != NULL; g = g->al_next)
		if (g->al_old > TIMER_INTERVAL)
			g->al_old -= TIMER_INTERVAL;
		else
			g->al_old = 0;
}


/*
 * Process an incoming host membership query
 */
void
accept_listener_query(src, dst, group, tmo)
	struct sockaddr_in6 *src;
	struct in6_addr *dst, *group;
	int tmo;
{
	register int mifi;
	register struct uvif *v;
	struct sockaddr_in6 group_sa = {sizeof(group_sa), AF_INET6};

	/* Ignore my own membership query */
	if (local_address(src) != NO_VIF)
		return;

	if ((mifi = find_vif_direct(src)) == NO_VIF) {
		IF_DEBUG(DEBUG_MLD)
			log(LOG_INFO, 0,
			    "accept_listener_query: can't find a mif");
		return;
	}

	v = &uvifs[mifi];

	if (v->uv_querier == NULL || !inet6_equal(&v->uv_querier->al_addr, src))
	{
		/*
		 * This might be:
		 * - A query from a new querier, with a lower source address
		 *   than the current querier (who might be me)
		 * - A query from a new router that just started up and doesn't
		 *   know who the querier is.
		 * - A query from the current querier
		 */
		if (inet6_lessthan(src, (v->uv_querier ? &v->uv_querier->al_addr
					 : &v->uv_linklocal->pa_addr))) {
			IF_DEBUG(DEBUG_MLD)
				log(LOG_DEBUG, 0, "new querier %s (was %s) "
				    "on mif %d",
				    inet6_fmt(&src->sin6_addr),
				    v->uv_querier ?
				    inet6_fmt(&v->uv_querier->al_addr.sin6_addr) :
				    "me", mifi);
			if (!v->uv_querier) {
				v->uv_querier = (struct listaddr *)
					malloc(sizeof(struct listaddr));
				memset(v->uv_querier, 0,
				       sizeof(struct listaddr));
			}
			v->uv_flags &= ~VIFF_QUERIER;
			v->uv_querier->al_addr = *src;
			time(&v->uv_querier->al_ctime);
		}
	}
    
	/*
	 * Reset the timer since we've received a query.
	 */
	if (v->uv_querier && inet6_equal(src, &v->uv_querier->al_addr))
		v->uv_querier->al_timer = MLD6_OTHER_QUERIER_PRESENT_INTERVAL;

	/*
	 * If this is a Group-Specific query which we did not source,
	 * we must set our membership timer to [Last Member Query Count] *
	 * the [Max Response Time] in the packet.
	 */
	if (!IN6_IS_ADDR_UNSPECIFIED(group) &&
	    inet6_equal(src, &v->uv_linklocal->pa_addr)) {
		register struct listaddr *g;

		IF_DEBUG(DEBUG_MLD)
			log(LOG_DEBUG, 0,
			    "%s for %s from %s on mif %d, timer %d",
			    "Group-specific membership query",
			    inet6_fmt(group),
			    inet6_fmt(&src->sin6_addr), mifi, tmo);

		group_sa.sin6_addr = *group;
		group_sa.sin6_scope_id = inet6_uvif2scopeid(&group_sa, v);
		for (g = v->uv_groups; g != NULL; g = g->al_next) {
			if (inet6_equal(&group_sa, &g->al_addr)
			    && g->al_query == 0) {
				/* setup a timeout to remove the group membership */
				if (g->al_timerid)
					g->al_timerid = DeleteTimer(g->al_timerid);
				g->al_timer = MLD6_LAST_LISTENER_QUERY_COUNT *
		                        tmo / MLD6_TIMER_SCALE;
				/*
				 * use al_query to record our presence
				 * in last-member state
				 */
				g->al_query = -1;
				g->al_timerid = SetTimer(mifi, g);
				IF_DEBUG(DEBUG_MLD)
					log(LOG_DEBUG, 0,
					    "timer for grp %s on mif %d "
					    "set to %d",
					    inet6_fmt(group),
					    mifi, g->al_timer);
				break;
			}
		}
	}
}


/*
 * Process an incoming group membership report.
 */
void
accept_listener_report(src, dst, group)
	struct sockaddr_in6 *src;
	struct in6_addr *dst, *group;
{
	register mifi_t mifi;
	register struct uvif *v;
	register struct listaddr *g;
	struct sockaddr_in6 group_sa = {sizeof(group_sa), AF_INET6};

	if (IN6_IS_ADDR_MC_LINKLOCAL(group)) {
		IF_DEBUG(DEBUG_MLD)
			log(LOG_DEBUG, 0,
			    "accept_listener_report: group(%s) has the "
			    "link-local scope. discard", inet6_fmt(group));
		return;
	}

	if ((mifi = find_vif_direct_local(src)) == NO_VIF) {
		IF_DEBUG(DEBUG_MLD)
			log(LOG_INFO, 0,
			    "accept_listener_report: can't find a mif");
		return;
	}
    
	IF_DEBUG(DEBUG_MLD)
		log(LOG_INFO, 0,
		    "accepting multicast listener report: "
		    "src %s, dst% s, grp %s",
		    inet6_fmt(&src->sin6_addr), inet6_fmt(dst),
		    inet6_fmt(group));

	v = &uvifs[mifi];
    
	/*
	 * Look for the group in our group list; if found, reset its timer.
	 */
	group_sa.sin6_addr = *group;
	group_sa.sin6_scope_id = inet6_uvif2scopeid(&group_sa, v);
	for (g = v->uv_groups; g != NULL; g = g->al_next) {
		if (inet6_equal(&group_sa, &g->al_addr)) {
			g->al_reporter = *src;

			/* delete old timers, set a timer for expiration */
			g->al_timer = MLD6_LISTENER_INTERVAL;
			if (g->al_query)
				g->al_query = DeleteTimer(g->al_query);
			if (g->al_timerid)
				g->al_timerid = DeleteTimer(g->al_timerid);
			g->al_timerid = SetTimer(mifi, g);
			add_leaf(mifi, NULL, &group_sa);
			break;
		}
	}

	/*
	 * If not found, add it to the list and update kernel cache.
	 */
	if (g == NULL) {
		g = (struct listaddr *)malloc(sizeof(struct listaddr));
		if (g == NULL)
			log(LOG_ERR, 0, "ran out of memory");    /* fatal */

		g->al_addr   = group_sa;
		g->al_old = 0;

		/** set a timer for expiration **/
		g->al_query     = 0;
		g->al_timer     = MLD6_LISTENER_INTERVAL;
		g->al_reporter  = *src;
		g->al_timerid   = SetTimer(mifi, g);
		g->al_next      = v->uv_groups;
		v->uv_groups    = g;
		time(&g->al_ctime);

		add_leaf(mifi, NULL, &group_sa);
	}
}


/* TODO: send PIM prune message if the last member? */
void
accept_listener_done(src, dst, group)
	struct sockaddr_in6 *src;
	struct in6_addr *dst, *group;
{
	register mifi_t mifi;
	register struct uvif *v;
	register struct listaddr *g;
	struct sockaddr_in6 group_sa = {sizeof(group_sa), AF_INET6};

	if ((mifi = find_vif_direct_local(src)) == NO_VIF) {
		IF_DEBUG(DEBUG_MLD)
			log(LOG_INFO, 0,
			    "accept_listener_done: can't find a mif");
		return;
	}

	IF_DEBUG(DEBUG_MLD)
		log(LOG_INFO, 0,
		    "accepting listener done message: src %s, dst% s, grp %s",
		    inet6_fmt(&src->sin6_addr),
		    inet6_fmt(dst), inet6_fmt(group));

	v = &uvifs[mifi];
    
	if (!(v->uv_flags & (VIFF_QUERIER | VIFF_DR)))
		return;

	/*
	 * Look for the group in our group list in order to set up a
	 * short-timeout query.
	 */
	group_sa.sin6_addr = *group;
	group_sa.sin6_scope_id = inet6_uvif2scopeid(&group_sa, v);
	for (g = v->uv_groups; g != NULL; g = g->al_next) {
		if (inet6_equal(&group_sa, &g->al_addr)) {
			IF_DEBUG(DEBUG_MLD)
				log(LOG_DEBUG, 0,
				    "[accept_done_message] %d %d \n",
				    g->al_old, g->al_query);

			/*
			 * Ignore the done message if there are old
			 * hosts present
			 */
			if (g->al_old)
				return;
	    
			/*
			 * still waiting for a reply to a query,
			 * ignore the done
			 */
			if (g->al_query)
				return;
	    
			/** delete old timer set a timer for expiration **/
			if (g->al_timerid)
				g->al_timerid = DeleteTimer(g->al_timerid);

			/** send a group specific querry **/
			g->al_timer = (MLD6_LAST_LISTENER_QUERY_INTERVAL/MLD6_TIMER_SCALE) *
				(MLD6_LAST_LISTENER_QUERY_COUNT + 1);
			if (v->uv_flags & VIFF_QUERIER &&
			    (v->uv_flags & VIFF_NOLISTENER) == 0)
				send_mld6(MLD6_LISTENER_QUERY, 0,
					  &v->uv_linklocal->pa_addr, NULL,
					  &g->al_addr.sin6_addr,
					  v->uv_ifindex,
					  MLD6_LAST_LISTENER_QUERY_INTERVAL, 0, 1);
			g->al_query = SetQueryTimer(g, mifi,
						    MLD6_LAST_LISTENER_QUERY_INTERVAL/MLD6_TIMER_SCALE,
						    MLD6_LAST_LISTENER_QUERY_INTERVAL);
			g->al_timerid = SetTimer(mifi, g);
			break;
		}
	}
}


/*
 * Time out record of a group membership on a vif
 */
static void
DelVif(arg)
	void *arg;
{
	cbk_t *cbk = (cbk_t *)arg;
	mifi_t mifi = cbk->mifi;
	struct uvif *v = &uvifs[mifi];
	struct listaddr *a, **anp, *g = cbk->g;

	/*
	 * Group has expired
	 * delete all kernel cache entries with this group
	 */
	if (g->al_query)
		DeleteTimer(g->al_query);

	delete_leaf(mifi, NULL, &g->al_addr);

	anp = &(v->uv_groups);
	while ((a = *anp) != NULL) {
		if (a == g) {
			*anp = a->al_next;
			free((char *)a);
		} else {
			anp = &a->al_next;
		}
	}

	free(cbk);
}


/*
 * Set a timer to delete the record of a group membership on a vif.
 */
static int
SetTimer(mifi, g)
	mifi_t mifi;
	struct listaddr *g;
{
	cbk_t *cbk;
    
	cbk = (cbk_t *) malloc(sizeof(cbk_t));
	cbk->mifi = mifi;
	cbk->g = g;
	return timer_setTimer(g->al_timer, DelVif, cbk);
}


/*
 * Delete a timer that was set above.
 */
static int
DeleteTimer(id)
	int id;
{
	timer_clearTimer(id);
	return 0;
}


/*
 * Send a group-specific query.
 */
static void
SendQuery(arg)
	void *arg;
{
	cbk_t *cbk = (cbk_t *)arg;
	register struct uvif *v = &uvifs[cbk->mifi];

	if (v->uv_flags & VIFF_QUERIER && (v->uv_flags & VIFF_NOLISTENER) == 0)
		send_mld6(MLD6_LISTENER_QUERY, 0, &v->uv_linklocal->pa_addr,
			  NULL, &cbk->g->al_addr.sin6_addr,
			  v->uv_ifindex, cbk->q_time, 0, 1);
	cbk->g->al_query = 0;
	free(cbk);
}


/*
 * Set a timer to send a group-specific query.
 */
static int
SetQueryTimer(g, mifi, to_expire, q_time)
	struct listaddr *g;
	mifi_t mifi;
	int to_expire;
	int q_time;
{
	cbk_t *cbk;

	cbk = (cbk_t *) malloc(sizeof(cbk_t));
	cbk->g = g;
	cbk->q_time = q_time;
	cbk->mifi = mifi;
	return timer_setTimer(to_expire, SendQuery, cbk);
}

/* Checks for MLD listener: returns TRUE if there is a receiver for the
 * group on the given uvif, or returns FALSE otherwise.
 */
int
check_multicast_listener(v, group)
	struct uvif *v;
	struct sockaddr_in6 *group;
{
	register struct listaddr *g;

	/*
	 * Look for the group in our listener list;
	 */
	for (g = v->uv_groups; g != NULL; g = g->al_next) {
		if (inet6_equal(group, &g->al_addr)) 
			return TRUE;
	}
	return FALSE;
}

/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/timer.c,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */

#include <stdlib.h>
#include <syslog.h>
#include "pimd.h"
#include "mrt.h"
#include "vif.h"
#include <netinet6/ip6_mroute.h>
#include "timer.h"
#include "debug.h"
#include "rp.h"
#include "pim6_proto.h"
#include "mld6.h"
#include "mld6_proto.h"
#include "route.h"
#include "kern.h"
#include "debug.h"
#include "inet6.h"

/*
 * Global variables
 */
/*
 * XXX: The RATE is in bits/s. To include the header overhead, the
 * approximation is 1 byte/s = 10 bits/s `whatever_bytes` is the maximum
 * number of bytes within the test interval.
 */

u_int32 		timer_interval=DEFAULT_TIMER_INTERVAL;
u_int32         pim_reg_rate_bytes =
(PIM_DEFAULT_REG_RATE * PIM_DEFAULT_REG_RATE_INTERVAL) / 10;
u_int32         pim_reg_rate_check_interval = PIM_DEFAULT_REG_RATE_INTERVAL;
u_int32         pim_data_rate_bytes =
(PIM_DEFAULT_DATA_RATE * PIM_DEFAULT_DATA_RATE_INTERVAL) / 10;
u_int32         pim_data_rate_check_interval = PIM_DEFAULT_DATA_RATE_INTERVAL;
u_int32			pim_hello_period = PIM_TIMER_HELLO_PERIOD;
u_int32			pim_hello_holdtime = PIM_TIMER_HELLO_HOLDTIME;
u_int32			pim_join_prune_period = PIM_JOIN_PRUNE_PERIOD;
u_int32			pim_join_prune_holdtime = PIM_JOIN_PRUNE_HOLDTIME;
u_int32			pim_data_timeout=PIM_DATA_TIMEOUT;
u_int32			pim_register_suppression_timeout=PIM_REGISTER_SUPPRESSION_TIMEOUT;
u_int32			pim_register_probe_time=PIM_REGISTER_PROBE_TIME;
u_int32			pim_assert_timeout=PIM_ASSERT_TIMEOUT;


/*
 * Local functions definitions.
 */


/*
 * Local variables
 */
u_int16         unicast_routing_timer;	/* Used to check periodically for any
					 * change in the unicast routing. */
u_int16         unicast_routing_check_interval;
u_int8          ucast_flag;		/* Used to indicate there was a timeout */

u_int16         pim_data_rate_timer;	/* Used to check periodically the
					 * datarate of the active sources and
					 * eventually switch to the shortest
					 * path (if forwarder) */
u_int8          pim_data_rate_flag;	/* Used to indicate there was a
					 * timeout */

u_int16         pim_reg_rate_timer;	/* The same as above, but used by the
					 * RP to switch to the shortest path
					 * and avoid the PIM registers. */
u_int8          pim_reg_rate_flag;
u_int8          rate_flag;

/*
 * TODO: XXX: the timers below are not used. Instead, the data rate timer is
 * used.
 */
u_int16         kernel_cache_timer;	/* Used to timeout the kernel cache
					 * entries for idle sources */
u_int16         kernel_cache_check_interval;

/* to request and compare any route changes */

srcentry_t      srcentry_save;
rpentry_t       rpentry_save;

/*
 * Init some timers
 */
void
init_timers()
{
    unicast_routing_check_interval = UCAST_ROUTING_CHECK_INTERVAL;
    SET_TIMER(unicast_routing_timer, unicast_routing_check_interval);

    /*
     * The routing_check and the rate_check timers are interleaved to reduce
     * the amount of work that has to be done at once.
     */
    /* XXX: for simplicity, both the intervals are the same */

    if (pim_data_rate_check_interval < pim_reg_rate_check_interval)
	pim_reg_rate_check_interval = pim_data_rate_check_interval;

    SET_TIMER(pim_data_rate_timer, 3 * pim_data_rate_check_interval / 2);
    SET_TIMER(pim_reg_rate_timer, 3 * pim_reg_rate_check_interval / 2);

    /*
     * Initialize the srcentry and rpentry used to save the old routes during
     * unicast routing change discovery process.
     */

    srcentry_save.prev = (srcentry_t *) NULL;
    srcentry_save.next = (srcentry_t *) NULL;
    memset(&srcentry_save.address, 0, sizeof(struct sockaddr_in6));
    srcentry_save.address.sin6_len   = sizeof(struct sockaddr_in6);
    srcentry_save.address.sin6_family= AF_INET6; 
    srcentry_save.mrtlink = (mrtentry_t *) NULL;
    srcentry_save.incoming = NO_VIF;
    srcentry_save.upstream = (pim_nbr_entry_t *) NULL;
    srcentry_save.metric = ~0;
    srcentry_save.preference = ~0;
    RESET_TIMER(srcentry_save.timer);
    srcentry_save.cand_rp = (cand_rp_t *) NULL;

    rpentry_save.prev = (rpentry_t *) NULL;
    rpentry_save.next = (rpentry_t *) NULL;
    memset(&rpentry_save.address, 0, sizeof(struct sockaddr_in6));
    rpentry_save.address.sin6_len   = sizeof(struct sockaddr_in6);
    rpentry_save.address.sin6_family= AF_INET6; 
    rpentry_save.mrtlink = (mrtentry_t *) NULL;
    rpentry_save.incoming = NO_VIF;
    rpentry_save.upstream = (pim_nbr_entry_t *) NULL;
    rpentry_save.metric = ~0;
    rpentry_save.preference = ~0;
    RESET_TIMER(rpentry_save.timer);
    rpentry_save.cand_rp = (cand_rp_t *) NULL;

}


/*
 * On every timer interrupt, advance (i.e. decrease) the timer for each
 * neighbor and group entry for each vif.
 */
void
age_vifs()
{
    vifi_t			vifi;
    register struct uvif 	*v;
    register pim_nbr_entry_t 	*next_nbr,
                   		*curr_nbr;

    /*
     * XXX: TODO: currently, sending to qe* interface which is DOWN doesn't
     * return error (ENETDOWN) on my Solaris machine, so have to check
     * periodically the interfaces status. If this is fixed, just remove the
     * defs around the "if (vifs_down)" line.
     */

#if (!((defined SunOS) && (SunOS >= 50)))
    if (vifs_down)
#endif				/* Solaris */
	check_vif_state();

    /* Age many things */
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v)
    {
	if (v->uv_flags & (VIFF_DISABLED | VIFF_DOWN | MIFF_REGISTER))
	    continue;

	/* Timeout the MLD querier (unless we re the querier) */
	if ((v->uv_flags & VIFF_QUERIER) == 0 &&
	    v->uv_querier) { /* this must be non-NULL, but check for safety. */
	    IF_TIMEOUT(v->uv_querier->al_timer) {
		v->uv_querier_timo++; /* count statistics */

		/* act as a querier by myself */
		v->uv_flags |= VIFF_QUERIER;
		v->uv_querier->al_addr = v->uv_linklocal->pa_addr;
		v->uv_querier->al_timer = MLD6_OTHER_QUERIER_PRESENT_INTERVAL;
		time(&v->uv_querier->al_ctime); /* reset timestamp */
		query_groups(v);
	    }
	}

	/* Timeout neighbors */
	for (curr_nbr = v->uv_pim_neighbors; curr_nbr != NULL;
	     curr_nbr = next_nbr)
	{
	    next_nbr = curr_nbr->next;

	    /*
	     * Never timeout neighbors with holdtime = 0xffff. This may be
	     * used with ISDN lines to avoid keeping the link up with
	     * periodic Hello messages.
	     */
	    /* TODO: XXX: TIMER implem. dependency! */

	    if (PIM_MESSAGE_HELLO_HOLDTIME_FOREVER == curr_nbr->timer)
		continue;
	    IF_NOT_TIMEOUT(curr_nbr->timer)
		continue;

	    v->uv_pim6_nbr_timo++;
	    IF_DEBUG(DEBUG_PIM_HELLO)
		log(LOG_DEBUG, 0,
		    "%s on %s is dead , delete it",
		    inet6_fmt(&curr_nbr->address.sin6_addr),
		    uvifs[curr_nbr->vifi].uv_name);
	    delete_pim6_nbr(curr_nbr);
	}

	/* PIM_HELLO periodic */
	IF_TIMEOUT(v->uv_pim_hello_timer)
	    send_pim6_hello(v, pim_hello_holdtime);

	/* MLD6 query periodic */
	IF_TIMEOUT(v->uv_gq_timer)
	    query_groups(v);
    }

    IF_DEBUG(DEBUG_IF) {
	dump_vifs(log_fp);
	;
    }
}

/*
 * Scan the whole routing table and timeout a bunch of timers:
 *  - oifs timers
 *  - Join/Prune timer
 *  - routing entry
 *  - Assert timer
 *  - Register-Suppression timer
 *
 *  - If the global timer for checking the unicast routing has expired, perform
 *  also iif/upstream router change verification
 *  - If the global timer for checking the data rate has expired, check the
 *  number of bytes forwarded after the lastest timeout. If bigger than
 *  a given threshold, then switch to the shortest path.
 *  If `number_of_bytes == 0`, then delete the kernel cache entry.
 *
 * Only the entries which have the Join/Prune timer expired are sent.
 * In the special case when we have ~(S,G)RPbit Prune entry, we must
 * include any (*,G) or (*,*,RP) XXX: ???? what and why?
 *
 * Below is a table which summarizes the segmantic rules.
 *
 * On the left side is "if A must be included in the J/P message".
 * On the top is "shall/must include B?"
 * "Y" means "MUST include"
 * "SY" means "SHOULD include"
 * "N" means  "NO NEED to include"
 * (G is a group that matches to RP)
 *
 *              -----------||-----------||-----------
 *            ||  (*,*,RP) ||   (*,G)   ||   (S,G)   ||
 *            ||-----------||-----------||-----------||
 *            ||  J  |  P  ||  J  |  P  ||  J  |  P  ||
 * ==================================================||
 *          J || n/a | n/a ||  N  |  Y  ||  N  |  Y  ||
 * (*,*,RP) -----------------------------------------||
 *          P || n/a | n/a ||  SY |  N  ||  SY |  N  ||
 * ==================================================||
 *          J ||  N  |  N  || n/a | n/a ||  N  |  Y  ||
 *   (*,G)  -----------------------------------------||
 *          P ||  N  |  N  || n/a | n/a ||  SY |  N  ||
 * ==================================================||
 *          J ||  N  |  N  ||  N  |  N  || n/a | n/a ||
 *   (S,G)  -----------------------------------------||
 *          P ||  N  |  N  ||  N  |  N  || n/a | n/a ||
 * ==================================================
 *
 */

void
age_routes()
{
    cand_rp_t      	*cand_rp_ptr;
    grpentry_t     	*grpentry_ptr;
    grpentry_t     	*grpentry_ptr_next;
    mrtentry_t     	*mrtentry_grp;
    mrtentry_t    	*mrtentry_rp;
    mrtentry_t     	*mrtentry_wide;
    mrtentry_t     	*mrtentry_srcs;
    mrtentry_t     	*mrtentry_srcs_next;
    struct uvif    	*v;
    vifi_t          	vifi;
    pim_nbr_entry_t 	*pim_nbr_ptr;
    int             	change_flag;
    int             	rp_action,
                    	grp_action,
                    	src_action=0,
                    	src_action_rp=0;
    int             	dont_calc_action;
    int             	did_switch_flag;
    rp_grp_entry_t 	*rp_grp_entry_ptr;
    kernel_cache_t 	*kernel_cache_ptr;
    kernel_cache_t 	*kernel_cache_next;
    u_long          	curr_bytecnt;
    rpentry_t      	*rpentry_ptr;
    int             	update_rp_iif;
    int             	update_src_iif;
    if_set     	    	new_pruned_oifs;

    /*
     * Timing out of the global `unicast_routing_timer` and `data_rate_timer`
     */

    IF_TIMEOUT(unicast_routing_timer)
    {
	ucast_flag = TRUE;
	SET_TIMER(unicast_routing_timer, unicast_routing_check_interval);
    }
    ELSE
    {
	ucast_flag = FALSE;
    }

    IF_TIMEOUT(pim_data_rate_timer)
    {
	pim_data_rate_flag = TRUE;
	SET_TIMER(pim_data_rate_timer, pim_data_rate_check_interval);
    }
    ELSE
    {
	pim_data_rate_flag = FALSE;
    }

    IF_TIMEOUT(pim_reg_rate_timer)
    {
	pim_reg_rate_flag = TRUE;
	SET_TIMER(pim_reg_rate_timer, pim_reg_rate_check_interval);
    }
    ELSE
    {
	pim_reg_rate_flag = FALSE;
    }

    rate_flag = pim_data_rate_flag | pim_reg_rate_flag;

    /* Scan the (*,*,RP) entries */

    for (cand_rp_ptr = cand_rp_list; cand_rp_ptr != (cand_rp_t *) NULL;
	 cand_rp_ptr = cand_rp_ptr->next)
    {
	
	rpentry_ptr = cand_rp_ptr->rpentry;

	/*
	 * Need to save only `incoming` and `upstream` to discover unicast
	 * route changes. `metric` and `preference` are not interesting for
	 * us.
	 */

	rpentry_save.incoming = rpentry_ptr->incoming;
	rpentry_save.upstream = rpentry_ptr->upstream;

	update_rp_iif = FALSE;
	if ((ucast_flag == TRUE) &&
	    (!inet6_equal(&rpentry_ptr->address ,&my_cand_rp_address)))
	{
	    /*
	     * I am not the RP. If I was the RP, then the iif is register_vif
	     * and no need to reset it.
	     */

	    if (set_incoming(rpentry_ptr, PIM_IIF_RP) != TRUE)
	    {
		/*
		 * TODO: XXX: no route to that RP. Panic? There is a high
		 * probability the network is partitioning so immediately
		 * remapping to other RP is not a good idea. Better wait the
		 * Bootstrap mechanism to take care of it and provide me with
		 * correct Cand-RP-Set.
		 */
		;
	    }
	    else
	    {
		if ((rpentry_save.upstream != rpentry_ptr->upstream)
		    || (rpentry_save.incoming != rpentry_ptr->incoming))
		{
		    /*
		     * Routing change has occur. Update all (*,G) and
		     * (S,G)RPbit iifs mapping to that RP
		     */
		    update_rp_iif = TRUE;
		}
	    }
	}

	rp_action = PIM_ACTION_NOTHING;
	mrtentry_rp = cand_rp_ptr->rpentry->mrtlink;

	if (mrtentry_rp != (mrtentry_t *) NULL)
	{
	    /* outgoing interfaces timers */

	    change_flag = FALSE;
	    for (vifi = 0; vifi < numvifs; vifi++)
	    {
		if (IF_ISSET(vifi, &mrtentry_rp->joined_oifs))
		    IF_TIMEOUT(mrtentry_rp->vif_timers[vifi])
		    {
			    uvifs[vifi].uv_outif_timo++;
			    IF_CLR(vifi, &mrtentry_rp->joined_oifs);
			    change_flag = TRUE;
		    }
	    }
	    if ((change_flag == TRUE) || (update_rp_iif == TRUE))
	    {
		change_interfaces(mrtentry_rp,
				  rpentry_ptr->incoming,
				  &mrtentry_rp->joined_oifs,
				  &mrtentry_rp->pruned_oifs,
				  &mrtentry_rp->leaves,
				  &mrtentry_rp->asserted_oifs, 0);
		mrtentry_rp->upstream = rpentry_ptr->upstream;
	    }

	    if (rate_flag == TRUE)
	    {
		/* Check the activity for this entry */

		/*
		 * XXX: the spec says to start monitoring first the total
		 * traffic for all senders for particular (*,*,RP) or (*,G)
		 * and if the total traffic exceeds some predefined
		 * threshold, then start monitoring the data traffic for each
		 * particular sender for this group: (*,G) or (*,*,RP).
		 * However, because the kernel cache/traffic info is of the
		 * form (S,G), it is easier if we are simply collecting (S,G)
		 * traffic all the time.
		 * 
		 * For (*,*,RP) if the number of bytes received between the last
		 * check and now exceeds some precalculated value (based on
		 * interchecking period and datarate threshold AND if there
		 * are directly connected members (i.e. we are their last
		 * hop(e) router), then create (S,G) and start initiating
		 * (S,G) Join toward the source. The same applies for (*,G).
		 * The spec does not say that if the datarate goes below a
		 * given threshold, then will switch back to the shared tree,
		 * hence after a switch to the source-specific tree occurs, a
		 * source with low datarate, but periodically sending will
		 * keep the (S,G) states.
		 * 
		 * If a source with kernel cache entry has been idle after the
		 * last time a check of the datarate for the whole routing
		 * table, then delete its kernel cache entry.
		 */
		for (kernel_cache_ptr = mrtentry_rp->kernel_cache;
		     kernel_cache_ptr != (kernel_cache_t *) NULL;
		     kernel_cache_ptr = kernel_cache_next)
		{
		    kernel_cache_next = kernel_cache_ptr->next;
		    curr_bytecnt = kernel_cache_ptr->sg_count.bytecnt;
		    if (k_get_sg_cnt(udp_socket, &kernel_cache_ptr->source,
				     &kernel_cache_ptr->group,
				     &kernel_cache_ptr->sg_count)
			|| (curr_bytecnt ==
			    kernel_cache_ptr->sg_count.bytecnt))
		    {
			/*
			 * Either for some reason there is no such routing
			 * entry or that particular (s,g) was idle. Delete
			 * the routing entry from the kernel.
			 */

			delete_single_kernel_cache(mrtentry_rp,
						   kernel_cache_ptr);
			continue;
		    }
		    /*
		     * Check if the datarate was high enough to switch to
		     * source specific tree.
		     */
		    /* Forwarder initiated switch */

		    did_switch_flag = FALSE;
		    if (curr_bytecnt + pim_data_rate_bytes
			< kernel_cache_ptr->sg_count.bytecnt)
		    {
			if (vif_forwarder(&mrtentry_rp->leaves,
					  &mrtentry_rp->oifs) == TRUE)
			{
#ifdef KERNEL_MFC_WC_G
// TODO (one day... :))
			    if (kernel_cache_ptr->source == IN6ADDR_ANY_N)
			    {
				delete_single_kernel_cache(mrtentry_rp,
							   kernel_cache_ptr);
				mrtentry_rp->flags |= MRTF_MFC_CLONE_SG;
				continue;
			    }
#endif				/* KERNEL_MFC_WC_G */
			    pim6dstat.pim6_trans_spt_forward++;
			    switch_shortest_path(&kernel_cache_ptr->source,
						 &kernel_cache_ptr->group);
			    did_switch_flag = TRUE;
			}
		    }

		    /* RP initiated switch */

		    if ((did_switch_flag == FALSE)
			&& (curr_bytecnt + pim_reg_rate_bytes
			    < kernel_cache_ptr->sg_count.bytecnt))
		    {
			if (mrtentry_rp->incoming == reg_vif_num)

#ifdef KERNEL_MFC_WC_G
// TODO (one day :))
			    if (kernel_cache_ptr->source == IN6ADDR_ANY_N)
			    {
				delete_single_kernel_cache(mrtentry_rp,
							   kernel_cache_ptr);
				mrtentry_rp->flags |= MRTF_MFC_CLONE_SG;
				continue;
			    }
#endif				/* KERNEL_MFC_WC_G */
			pim6dstat.pim6_trans_spt_rp++;
			switch_shortest_path(&kernel_cache_ptr->source,
					     &kernel_cache_ptr->group);
		    }
		}
	    }

	    /* Join/Prune timer */
	    IF_TIMEOUT(mrtentry_rp->jp_timer)
	    {

		IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
			log(LOG_DEBUG,0,"Join/Prune timer expired");

		rp_action = join_or_prune(mrtentry_rp,
					  mrtentry_rp->upstream);

		IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
			log(LOG_DEBUG,0,"rp_action = %d",rp_action);

		if (rp_action != PIM_ACTION_NOTHING)
		    add_jp_entry(mrtentry_rp->upstream,
				 pim_join_prune_holdtime,
				 &sockaddr6_d,
				 STAR_STAR_RP_MSK6LEN,
				 &mrtentry_rp->source->address,
				 SINGLE_SRC_MSK6LEN,
				 MRTF_RP | MRTF_WC,
				 rp_action);
		SET_TIMER(mrtentry_rp->jp_timer, pim_join_prune_period);
	    }

	    /* Assert timer */
	    if (mrtentry_rp->flags & MRTF_ASSERTED)
	    {
		IF_TIMEOUT(mrtentry_rp->assert_timer)
		{
		    /* TODO: XXX: reset the upstream router now */
		    mrtentry_rp->flags &= ~MRTF_ASSERTED;
		}
	    }
	    /* Register-Suppression timer */
	    /*
	     * TODO: to reduce the kernel calls, if the timer is running,
	     * install a negative cache entry in the kernel?
	     */
	    /*
	     * TODO: can we have Register-Suppression timer for (*,*,RP)?
	     * Currently no...
	     */

	    IF_TIMEOUT(mrtentry_rp->rs_timer)
		;
	    /* routing entry */

	    if ((TIMEOUT(mrtentry_rp->timer))
		&& (IF_ISEMPTY(&mrtentry_rp->leaves)))
	    {
		pim6dstat.pim6_rtentry_timo++;
		delete_mrtentry(mrtentry_rp);
	    }
	}			/* mrtentry_rp != NULL */

	/* Just in case if that (*,*,RP) was deleted */

	mrtentry_rp = cand_rp_ptr->rpentry->mrtlink;

	/* Check the (*,G) and (S,G) entries */

	for (rp_grp_entry_ptr = cand_rp_ptr->rp_grp_next;
	     rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
	     rp_grp_entry_ptr = rp_grp_entry_ptr->rp_grp_next)
	{

	    for (grpentry_ptr = rp_grp_entry_ptr->grplink;
		 grpentry_ptr != (grpentry_t *) NULL;
		 grpentry_ptr = grpentry_ptr_next)
	    {
		grpentry_ptr_next = grpentry_ptr->rpnext;
		mrtentry_grp = grpentry_ptr->grp_route;
		mrtentry_srcs = grpentry_ptr->mrtlink;

		grp_action = PIM_ACTION_NOTHING;
		if (mrtentry_grp != (mrtentry_t *) NULL)
		{
		    /* The (*,G) entry */
		    /* outgoing interfaces timers */

		    change_flag = FALSE;
		    for (vifi = 0; vifi < numvifs; vifi++)
		    {
			if (IF_ISSET(vifi, &mrtentry_grp->joined_oifs))
			    IF_TIMEOUT(mrtentry_grp->vif_timers[vifi])
			{
			    IF_CLR(vifi, &mrtentry_grp->joined_oifs);
			    uvifs[vifi].uv_outif_timo++;
			    change_flag = TRUE;
			}
		    }

		    if ((change_flag == TRUE) || (update_rp_iif == TRUE))
		    {
			change_interfaces(mrtentry_grp,
					  rpentry_ptr->incoming,
					  &mrtentry_grp->joined_oifs,
					  &mrtentry_grp->pruned_oifs,
					  &mrtentry_grp->leaves,
					  &mrtentry_grp->asserted_oifs, 0);
			mrtentry_grp->upstream = rpentry_ptr->upstream;
		    }

		    /* Check the sources activity */

		    if (rate_flag == TRUE)
		    {
			for (kernel_cache_ptr = mrtentry_grp->kernel_cache;
			     kernel_cache_ptr != (kernel_cache_t *) NULL;
			     kernel_cache_ptr = kernel_cache_next)
			{
			    kernel_cache_next = kernel_cache_ptr->next;
			    curr_bytecnt =
				kernel_cache_ptr->sg_count.bytecnt;
			    if (k_get_sg_cnt(udp_socket,
					     &kernel_cache_ptr->source,
					     &kernel_cache_ptr->group,
					     &kernel_cache_ptr->sg_count)
				|| (curr_bytecnt ==
				    kernel_cache_ptr->sg_count.bytecnt))
			    {
				/*
				 * Either for whatever reason there is no
				 * such routing entry or that particular
				 * (s,g) was idle. Delete the routing entry
				 * from the kernel.
				 */

				delete_single_kernel_cache(mrtentry_grp,
							   kernel_cache_ptr);
				continue;
			    }

			    /*
			     * Check if the datarate was high enough to
			     * switch to source specific tree.
			     */
			    /* Forwarder initiated switch */

			    did_switch_flag = FALSE;
			    if (curr_bytecnt + pim_data_rate_bytes
				< kernel_cache_ptr->sg_count.bytecnt)
			    {
				if (vif_forwarder(&mrtentry_grp->leaves,
						  &mrtentry_grp->oifs) == TRUE)
				{
#ifdef KERNEL_MFC_WC_G
// TODO 
				    if (kernel_cache_ptr->source
					== IN6ADDR_ANY_N)
				    {
					delete_single_kernel_cache(mrtentry_grp, kernel_cache_ptr);
					mrtentry_grp->flags
					    |= MRTF_MFC_CLONE_SG;
					continue;
				    }
#endif				/* KERNEL_MFC_WC_G */

				    pim6dstat.pim6_trans_spt_forward++;
				    switch_shortest_path(&kernel_cache_ptr->source, &kernel_cache_ptr->group);
				    did_switch_flag = TRUE;
				}
			    }

			    /* RP initiated switch */

			    if ((did_switch_flag == FALSE)
				&& (curr_bytecnt + pim_reg_rate_bytes
				    < kernel_cache_ptr->sg_count.bytecnt))
			    {
				if (mrtentry_grp->incoming == reg_vif_num)
#ifdef KERNEL_MFC_WC_G
// TODO
				    if (kernel_cache_ptr->source
					== IN6ADDR_ANY_N)
				    {
					delete_single_kernel_cache(mrtentry_grp, kernel_cache_ptr);
					mrtentry_grp->flags
					    |= MRTF_MFC_CLONE_SG;
					continue;
				    }
#endif				/* KERNEL_MFC_WC_G */
				pim6dstat.pim6_trans_spt_rp++;
				switch_shortest_path(&kernel_cache_ptr->source,
						   &kernel_cache_ptr->group);
			    }
			}
		    }

		    dont_calc_action = FALSE;
		    if (rp_action != PIM_ACTION_NOTHING)
		    {
			grp_action = join_or_prune(mrtentry_grp,
						   mrtentry_grp->upstream);
			dont_calc_action = TRUE;
			if (((rp_action == PIM_ACTION_JOIN)
			     && (grp_action == PIM_ACTION_PRUNE))
			    || ((rp_action == PIM_ACTION_PRUNE)
				&& (grp_action == PIM_ACTION_JOIN)))
			    FIRE_TIMER(mrtentry_grp->jp_timer);
		    }

		    /* Join/Prune timer */

		    IF_TIMEOUT(mrtentry_grp->jp_timer)
		    {
			IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
				log(LOG_DEBUG,0,"Join/Prune timer expired");

			if (dont_calc_action != TRUE)
			    grp_action = join_or_prune(mrtentry_grp,
						    mrtentry_grp->upstream);

			IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
				log(LOG_DEBUG,0,"grp_action = %d",grp_action);

			if (grp_action != PIM_ACTION_NOTHING)
			{
			        add_jp_entry(mrtentry_grp->upstream,
					 pim_join_prune_holdtime,
					 &mrtentry_grp->group->group,
					 SINGLE_GRP_MSK6LEN,
					 &cand_rp_ptr->rpentry->address,
					 SINGLE_SRC_MSK6LEN,
					 MRTF_RP | MRTF_WC,
					 grp_action);
			}

			SET_TIMER(mrtentry_grp->jp_timer, pim_join_prune_period);
		    }

		    /* Assert timer */
		    if (mrtentry_grp->flags & MRTF_ASSERTED)
		    {
			IF_TIMEOUT(mrtentry_grp->assert_timer)
			{
			    /* TODO: XXX: reset the upstream router now */
			    mrtentry_grp->flags &= ~MRTF_ASSERTED;
			}
		    }
		    /* Register-Suppression timer */
		    /*
		     * TODO: to reduce the kernel calls, if the timer is
		     * running, install a negative cache entry in the kernel?
		     */
		    /*
		     * TODO: currently cannot have Register-Suppression timer
		     * for (*,G) entry, but keep this around.
		     */
		    IF_TIMEOUT(mrtentry_grp->rs_timer)
			;

		    /* routing entry */

		    if ((TIMEOUT(mrtentry_grp->timer))
			&& (IF_ISEMPTY(&mrtentry_grp->leaves)))
		    {
			pim6dstat.pim6_rtentry_timo++;
			delete_mrtentry(mrtentry_grp);
		    }
		}		/* if (mrtentry_grp != NULL) */


		/* For all (S,G) for this group */
		/* XXX: mrtentry_srcs was set before */
		for (; mrtentry_srcs != (mrtentry_t *) NULL;
		     mrtentry_srcs = mrtentry_srcs_next)
		{
		    /* routing entry */
		    mrtentry_srcs_next = mrtentry_srcs->grpnext;

		    /* outgoing interfaces timers */

		    change_flag = FALSE;
		    for (vifi = 0; vifi < numvifs; vifi++)
		    {
			if (IF_ISSET(vifi, &mrtentry_srcs->joined_oifs))
			{
			    /* TODO: checking for reg_num_vif is slow! */

			    if (vifi != reg_vif_num)
			    {
				IF_TIMEOUT(mrtentry_srcs->vif_timers[vifi])
				{
				    IF_CLR(vifi,
					     &mrtentry_srcs->joined_oifs);
				    change_flag = TRUE;
				    uvifs[vifi].uv_outif_timo++;
				}
			    }
			}
		    }

		    update_src_iif = FALSE;
		    if (ucast_flag == TRUE)
		    {
			if (!(mrtentry_srcs->flags & MRTF_RP))
			{
			    /* iif toward the source */
			    srcentry_save.incoming =
				mrtentry_srcs->source->incoming;
			    srcentry_save.upstream =
				mrtentry_srcs->source->upstream;
			    if (set_incoming(mrtentry_srcs->source,
					     PIM_IIF_SOURCE) != TRUE)
			    {

				/*
				 * XXX: not in the spec! Cannot find route
				 * toward that source. This is bad. Delete
				 * the entry.
				 */

				delete_mrtentry(mrtentry_srcs);
				continue;
			    }
			    else
			    {
	
				/* iif info found */

				if ((srcentry_save.incoming !=
				     mrtentry_srcs->incoming)
				    || (srcentry_save.upstream !=
					mrtentry_srcs->upstream))
				{
				    /* Route change has occur */
				    update_src_iif = TRUE;
				    mrtentry_srcs->incoming =
					mrtentry_srcs->source->incoming;
				    mrtentry_srcs->upstream =
					mrtentry_srcs->source->upstream;
				}
			    }
			}
			else
			{
			    /* (S,G)RPBit with iif toward RP */
			    if ((rpentry_save.upstream !=
				 mrtentry_srcs->upstream)
				|| (rpentry_save.incoming !=
				    mrtentry_srcs->incoming))
			    {
				update_src_iif = TRUE;	/* XXX: a hack */
				/* XXX: setup the iif now! */
				mrtentry_srcs->incoming =
				    rpentry_ptr->incoming;
				mrtentry_srcs->upstream =
				    rpentry_ptr->upstream;
			    }
			}
		    }

		    if ((change_flag == TRUE) || (update_src_iif == TRUE))
			/* Flush the changes */
			change_interfaces(mrtentry_srcs,
					  mrtentry_srcs->incoming,
					  &mrtentry_srcs->joined_oifs,
					  &mrtentry_srcs->pruned_oifs,
					  &mrtentry_srcs->leaves,
					  &mrtentry_srcs->asserted_oifs, 0);

		    if (rate_flag == TRUE)
		    {
			for (kernel_cache_ptr = mrtentry_srcs->kernel_cache;
			     kernel_cache_ptr != (kernel_cache_t *) NULL;
			     kernel_cache_ptr = kernel_cache_next)
			{
			    kernel_cache_next = kernel_cache_ptr->next;
			    curr_bytecnt = kernel_cache_ptr->sg_count.bytecnt;
			    if (k_get_sg_cnt(udp_socket,
					     &kernel_cache_ptr->source,
					     &kernel_cache_ptr->group,
					     &kernel_cache_ptr->sg_count)
				|| (curr_bytecnt ==
				    kernel_cache_ptr->sg_count.bytecnt))
			    {
				/*
				 * Either for some reason there is no such
				 * routing entry or that particular (s,g) was
				 * idle. Delete the routing entry from the
				 * kernel.
				 */

				delete_single_kernel_cache(mrtentry_srcs,
							   kernel_cache_ptr);
				continue;
			    }
			    /*
			     * Check if the datarate was high enough to
			     * switch to source specific tree. Need to check
			     * only when we have (S,G)RPbit in the forwarder
			     * or the RP itself.
			     */

			    if (!(mrtentry_srcs->flags & MRTF_RP))
				continue;
	
			     /* Forwarder initiated switch */

			    did_switch_flag = FALSE;
			    if (curr_bytecnt + pim_data_rate_bytes
				< kernel_cache_ptr->sg_count.bytecnt)
			    {
				if (vif_forwarder(&mrtentry_srcs->leaves,
						  &mrtentry_srcs->oifs)
				    == TRUE)
				{
				    switch_shortest_path(&kernel_cache_ptr->source, &kernel_cache_ptr->group);
				    did_switch_flag = TRUE;
				}
			    }
			    /* RP initiated switch */
			    if ((did_switch_flag == FALSE)
				&& (curr_bytecnt + pim_reg_rate_bytes
				    < kernel_cache_ptr->sg_count.bytecnt))
			    {
				if (mrtentry_srcs->incoming == reg_vif_num)
				    switch_shortest_path(&kernel_cache_ptr->source, &kernel_cache_ptr->group);
			    }

			    /*
			     * XXX: currentry the spec doesn't say to switch
			     * back to the shared tree if low datarate, but
			     * if needed to implement, the check must be done
			     * here. Don't forget to check whether I am a
			     * forwarder for that source.
			     */
			}
		    }

		    mrtentry_wide = mrtentry_srcs->group->grp_route;
		    if (mrtentry_wide == (mrtentry_t *) NULL)
			mrtentry_wide = mrtentry_rp;

		    dont_calc_action = FALSE;
		    if ((rp_action != PIM_ACTION_NOTHING)
			|| (grp_action != PIM_ACTION_NOTHING))
		    {
			src_action_rp = join_or_prune(mrtentry_srcs,
						      rpentry_ptr->upstream);
			src_action = src_action_rp;
			dont_calc_action = TRUE;
			if (src_action_rp == PIM_ACTION_JOIN)
			{
			    if ((grp_action == PIM_ACTION_PRUNE)
				|| (rp_action == PIM_ACTION_PRUNE))
				FIRE_TIMER(mrtentry_srcs->jp_timer);
			}
			else
			    if (src_action_rp == PIM_ACTION_PRUNE)
			    {
				if ((grp_action == PIM_ACTION_JOIN)
				    || (rp_action == PIM_ACTION_JOIN))
				    FIRE_TIMER(mrtentry_srcs->jp_timer);
			    }
		    }

		    /* Join/Prune timer */

		    IF_TIMEOUT(mrtentry_srcs->jp_timer)
		    {
			if ((dont_calc_action != TRUE)
			    || (rpentry_ptr->upstream
				!= mrtentry_srcs->upstream))
			    src_action = join_or_prune(mrtentry_srcs,
						   mrtentry_srcs->upstream);
			IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
				log(LOG_DEBUG,0,"src_action = %d",src_action);

			if (src_action != PIM_ACTION_NOTHING)
			    add_jp_entry(mrtentry_srcs->upstream,
					 pim_join_prune_holdtime, 
					 &mrtentry_srcs->group->group,
					 SINGLE_GRP_MSK6LEN,
					 &mrtentry_srcs->source->address,
					 SINGLE_SRC_MSK6LEN,
					 mrtentry_srcs->flags & MRTF_RP,
					 src_action);
			if (mrtentry_wide != (mrtentry_t *) NULL)
			{
			    /*
			     * Have both (S,G) and (*,G) (or (*,*,RP)). Check
			     * if need to send (S,G) PRUNE toward RP
			     */

			    if (mrtentry_srcs->upstream
				!= mrtentry_wide->upstream)
			    {
				if (dont_calc_action != TRUE)
				    src_action_rp =
					join_or_prune(mrtentry_srcs,
						   mrtentry_wide->upstream);
				/*
				 * XXX: TODO: do error check if src_action ==
				 * PIM_ACTION_JOIN, which should be an error.
				 */

				if (src_action_rp == PIM_ACTION_PRUNE)
				{
					IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
						log(LOG_DEBUG,0,"src_action = %d",src_action);
				    add_jp_entry(mrtentry_wide->upstream,
						 pim_join_prune_holdtime,
						 &mrtentry_srcs->group->group,
						 SINGLE_GRP_MSK6LEN,
					     &mrtentry_srcs->source->address,
						 SINGLE_SRC_MSK6LEN,
						 MRTF_RP,
						 src_action_rp);
				}
			    }
			}
			SET_TIMER(mrtentry_srcs->jp_timer, pim_join_prune_period);
		    }
		    /* Assert timer */
		    if (mrtentry_srcs->flags & MRTF_ASSERTED)
		    {
			IF_TIMEOUT(mrtentry_srcs->assert_timer)
			{
			    /* TODO: XXX: reset the upstream router now */
			    mrtentry_srcs->flags &= ~MRTF_ASSERTED;
			}
		    }
		    /* Register-Suppression timer */
		    /*
		     * TODO: to reduce the kernel calls, if the timer is
		     * running, install a negative cache entry in the kernel?
		     */

		    IF_TIMER_SET(mrtentry_srcs->rs_timer)
		    {
			IF_TIMEOUT(mrtentry_srcs->rs_timer)
			{
			    /* Start encapsulating the packets */
			    IF_COPY(&mrtentry_srcs->pruned_oifs,
				      &new_pruned_oifs);
			    IF_CLR(reg_vif_num, &new_pruned_oifs);
			    change_interfaces(mrtentry_srcs,
					      mrtentry_srcs->incoming,
					      &mrtentry_srcs->joined_oifs,
					      &new_pruned_oifs,
					      &mrtentry_srcs->leaves,
					      &mrtentry_srcs->asserted_oifs, 0);
			}
			ELSE
			{
			    /*
			     * The register suppression timer is running.
			     * Check whether it is time to send
			     * PIM_NULL_REGISTER.
			     */
			    /* TODO: XXX: TIMER implem. dependency! */

			    if (mrtentry_srcs->rs_timer
				<= pim_register_probe_time)
			    {
				/* Time to send a PIM_NULL_REGISTER */
				/*
				 * XXX: a (bad) hack! This will be sending
				 * periodically NULL_REGISTERS between
				 * pim_register_probe_time and 0. Well,
				 * because PROBE_TIME is 5 secs , it will
				 * happen only once ( if granularity is 5 and prob 5!)
				 * , so it helps to avoid
				 * adding a flag to the routing entry whether
				 * a NULL_REGISTER was sent.
				 */
				send_pim6_null_register(mrtentry_srcs);
			    }
			}
		    }

		    /* routing entry */
		    if (TIMEOUT(mrtentry_srcs->timer))
		    {
			pim6dstat.pim6_rtentry_timo++;

			if (IF_ISEMPTY(&mrtentry_srcs->leaves))
			{
			    delete_mrtentry(mrtentry_srcs);
			    continue;
			}
			/*
			 * XXX: if DR, Register suppressed, and leaf oif
			 * inherited from (*,G), the directly connected
			 * source is not active anymore, this (S,G) entry
			 * won't timeout. Check if the leaf oifs are
			 * inherited from (*,G); if true. delete the (S,G)
			 * entry.
			 */

			if (mrtentry_srcs->group->grp_route
			    != (mrtentry_t *) NULL)
			{
				if_set r_and, r_xor;
				vif_and(&mrtentry_srcs->group->grp_route->leaves,
					&mrtentry_srcs->leaves,
					&r_and);
				vif_xor(&r_and ,&mrtentry_srcs->leaves,
					&r_xor);
			    if (IF_ISEMPTY(&r_xor))
			    {
				delete_mrtentry(mrtentry_srcs);
				continue;
			    }
			}
		    }
		}		/* End of (S,G) loop */
	    }			/* End of (*,G) loop */
	}
    }				/* For all cand RPs */

    /* TODO: check again! */
    for (vifi = 0, v = &uvifs[0]; vifi < numvifs; vifi++, v++)
    {
	/* Send all pending Join/Prune messages */
	for (pim_nbr_ptr = v->uv_pim_neighbors;
	     pim_nbr_ptr != (pim_nbr_entry_t *) NULL;
	     pim_nbr_ptr = pim_nbr_ptr->next)
	{
	    pack_and_send_jp6_message(pim_nbr_ptr);
	}
    }

    IF_DEBUG(DEBUG_PIM_MRT)
	dump_pim_mrt(log_fp);
    return;
}


/*
 * TODO: timeout the RP-group mapping entries during the scan of the whole
 * routing table?
 */
void
age_misc()
{
    rp_grp_entry_t *rp_grp_entry_ptr;
    rp_grp_entry_t *rp_grp_entry_next;
    grp_mask_t     *grp_mask_ptr;
    grp_mask_t     *grp_mask_next;

    /* Timeout the Cand-RP-set entries */
    for (grp_mask_ptr = grp_mask_list;
	 grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_next)
    {
	/*
	 * If we timeout an entry, the grp_mask_ptr entry might be removed.
	 */
	grp_mask_next = grp_mask_ptr->next;
	for (rp_grp_entry_ptr = grp_mask_ptr->grp_rp_next;
	     rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
	     rp_grp_entry_ptr = rp_grp_entry_next)
	{
	    rp_grp_entry_next = rp_grp_entry_ptr->grp_rp_next;
	    IF_TIMEOUT(rp_grp_entry_ptr->holdtime) {
		delete_rp_grp_entry(&cand_rp_list, &grp_mask_list,
				    rp_grp_entry_ptr);
		pim6dstat.pim6_rpgrp_timo++;
	    }
	}
    }

    /* Cand-RP-Adv timer */
    if (cand_rp_flag == TRUE)
    {
	IF_TIMEOUT(pim_cand_rp_adv_timer)
	{
	    send_pim6_cand_rp_adv();
	    SET_TIMER(pim_cand_rp_adv_timer, my_cand_rp_adv_period);
	}
    }

    /* bootstrap-timer */

    IF_TIMEOUT(pim_bootstrap_timer)
    {
	pim6dstat.pim6_bootstrap_timo++;

	if (cand_bsr_flag == FALSE)
	{
	    /*
	     * If I am not Cand-BSR, start accepting Bootstrap messages from
	     * anyone. XXX: Even if the BSR has timeout, the existing
	     * Cand-RP-Set is kept.
	     */
	    curr_bsr_fragment_tag = 0;
	    curr_bsr_priority = 0;	/* Lowest priority */
	    memset(&curr_bsr_address, 0, sizeof(struct sockaddr_in6));
		curr_bsr_address.sin6_len = sizeof(struct sockaddr_in6);
		curr_bsr_address.sin6_family = AF_INET6;
	    MASKLEN_TO_MASK6(RP_DEFAULT_IPV6_HASHMASKLEN, curr_bsr_hash_mask);
	    SET_TIMER(pim_bootstrap_timer, PIM_BOOTSTRAP_TIMEOUT);
	}
	else
	{
	    /* I am Cand-BSR, so set the current BSR to me */
	    if (inet6_equal(&curr_bsr_address, &my_bsr_address))
	    {
		SET_TIMER(pim_bootstrap_timer, my_bsr_period);
		send_pim6_bootstrap();
	    }
	    else
	    {
		/*
		 * Short delay before becoming the BSR and start sending of
		 * the Cand-RP set (to reduce the transient control
		 * overhead).
		 */
		SET_TIMER(pim_bootstrap_timer, bootstrap_initial_delay());
		curr_bsr_fragment_tag = RANDOM();
		curr_bsr_priority = my_bsr_priority;
		curr_bsr_address = my_bsr_address;
		memcpy(&curr_bsr_hash_mask , &my_bsr_hash_mask , sizeof(struct in6_addr));
	    }
	}
    }


    IF_DEBUG(DEBUG_PIM_BOOTSTRAP | DEBUG_PIM_CAND_RP)
	dump_rp_set(log_fp);
    /* TODO: XXX: anything else to timeout */
}

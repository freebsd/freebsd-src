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
 * $FreeBSD: src/usr.sbin/pim6sd/route.c,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */

#include <sys/types.h>
#include <syslog.h>
#include "pimd.h"
#include "vif.h"
#include "mrt.h"
#include "debug.h"
#include "pim6_proto.h"
#include "route.h"
#include "mld6.h"
#include "rp.h"
#include "kern.h"
#include "timer.h"
#include "inet6.h"
#include <netinet6/ip6_mroute.h>
#include <netinet/ip6.h>
#include "routesock.h"

static void process_cache_miss __P((struct mrt6msg * im));
static void process_wrong_iif __P((struct mrt6msg * im));
static void process_whole_pkt __P((char *buf));

u_int32         default_source_metric = UCAST_DEFAULT_SOURCE_METRIC;
u_int32         default_source_preference = UCAST_DEFAULT_SOURCE_PREFERENCE;


/* Return the iif for given address */

vifi_t
get_iif(address)
    struct sockaddr_in6	*address;
{
    struct rpfctl   rpfc;

    k_req_incoming(address, &rpfc);
    if (IN6_IS_ADDR_UNSPECIFIED(&rpfc.rpfneighbor.sin6_addr))
	return (NO_VIF);
    return (rpfc.iif);
}

/* Return the PIM neighbor toward a source */
/*
 * If route not found or if a local source or if a directly connected source,
 * but is not PIM router, or if the first hop router is not a PIM router,
 * then return NULL.
 */
pim_nbr_entry_t *
find_pim6_nbr(source)
    struct sockaddr_in6 *source;
{
    struct rpfctl   rpfc;
    pim_nbr_entry_t *pim_nbr;
    struct sockaddr_in6	next_hop_router_addr;

    if (local_address(source) != NO_VIF)
	return (pim_nbr_entry_t *) NULL;
    k_req_incoming(source, &rpfc);
    if((IN6_IS_ADDR_UNSPECIFIED(&rpfc.rpfneighbor.sin6_addr))
	|| (rpfc.iif == NO_VIF))
	return (pim_nbr_entry_t *) NULL;
    next_hop_router_addr = rpfc.rpfneighbor;
    for (pim_nbr = uvifs[rpfc.iif].uv_pim_neighbors;
	 pim_nbr != (pim_nbr_entry_t *) NULL;
	 pim_nbr = pim_nbr->next)
	if (inet6_equal(&pim_nbr->address,&next_hop_router_addr))
	    return (pim_nbr);
    return (pim_nbr_entry_t *) NULL;
}


/*
 * TODO: check again the exact setup if the source is local or directly
 * connected!!! Yes Really for Ipv6!!
 */
/*
 * TODO: XXX: change the metric and preference for all (S,G) entries per
 * source or RP?
 */
/*
 * TODO - If possible, this would be the place to correct set the source's
 * preference and metric to that obtained from the kernel and/or unicast
 * routing protocol.  For now, set it to the configured default for local
 * pref/metric.
 */

/*
 * Set the iif, upstream router, preference and metric for the route toward
 * the source. Return TRUE is the route was found, othewise FALSE. If
 * srctype==PIM_IIF_SOURCE and if the source is directly connected then the
 * "upstream" is set to NULL. If srcentry==PIM_IIF_RP, then "upstream" in
 * case of directly connected "source" will be that "source" (if it is also
 * PIM router).,
 */
int
set_incoming(srcentry_ptr, srctype)
    srcentry_t     *srcentry_ptr;
    int             srctype;
{
    struct rpfctl   	rpfc;
    struct sockaddr_in6	source = srcentry_ptr->address;
    struct sockaddr_in6	neighbor_addr;
    register struct uvif *v;
    register pim_nbr_entry_t *n;

    /* Preference will be 0 if directly connected */

    srcentry_ptr->metric = 0;
    srcentry_ptr->preference = 0;

    if ((srcentry_ptr->incoming = local_address(&source)) != NO_VIF)
    {
	/* The source is a local address */
	/* TODO: set the upstream to myself? */
	srcentry_ptr->upstream = (pim_nbr_entry_t *) NULL;
	return (TRUE);
    }

    if ((srcentry_ptr->incoming = find_vif_direct(&source)) != NO_VIF)
    {
	/*
	 * The source is directly connected. Check whether we are looking for
	 * real source or RP
	 */

	if (srctype == PIM_IIF_SOURCE)
	{
	    srcentry_ptr->upstream = (pim_nbr_entry_t *) NULL;
	    return (TRUE);
	}
	else
	{
	    /* PIM_IIF_RP */
	    neighbor_addr = source;
	}
    }
    else
    {
	/* TODO: probably need to check the case if the iif is disabled */
	/* Use the lastest resource: the kernel unicast routing table */
	k_req_incoming(&source, &rpfc);
	if ((rpfc.iif == NO_VIF) ||
	    IN6_IS_ADDR_UNSPECIFIED(&rpfc.rpfneighbor.sin6_addr))
	{
	    /* couldn't find a route */
	    IF_DEBUG(DEBUG_PIM_MRT | DEBUG_RPF)
		log(LOG_DEBUG, 0, "NO ROUTE found for %s",
		    inet6_fmt(&source.sin6_addr));
	    return (FALSE);
	}
	srcentry_ptr->incoming = rpfc.iif;
	neighbor_addr = rpfc.rpfneighbor;
	/* set the preference for sources that aren't directly connected. */
	v = &uvifs[srcentry_ptr->incoming];
	srcentry_ptr->preference = v->uv_local_pref;
	srcentry_ptr->metric = v->uv_local_metric;
    }

    /*
     * The upstream router must be a (PIM router) neighbor, otherwise we are
     * in big trouble ;-). 
     * Yes but the neighbors are link-local and the rp is global ipv6..
     */
/* WARNING WARNING WARNING WARNING */
/* If the router is directly connected to the RP and the RP is the BSR , the next hop is
 * the globally reachable addresse of the RP : NOT link local neighbor but
 * a ipv6 global neighbor... 
 * the upstream router is the globally reachable router...
 *
 */
/* WARNING WARNING WARNING WARNING */

    v = &uvifs[srcentry_ptr->incoming];
    if (inet6_equal(&source,&neighbor_addr))
    {
	srcentry_ptr->upstream=v->uv_pim_neighbors;
 	return (TRUE);
    }

    for (n = v->uv_pim_neighbors; n != NULL; n = n->next)
    {
	if (inet6_lessthan(&neighbor_addr,&n->address))
	    continue;
	if (inet6_equal(&neighbor_addr,&n->address))
	{
	    /*
	     * The upstream router is found in the list of neighbors. We are
	     * safe!
	     */

	    srcentry_ptr->upstream = n;
	    IF_DEBUG(DEBUG_RPF)
		log(LOG_DEBUG, 0,
		    "For src %s, iif is %d, next hop router is %s",
		    inet6_fmt(&source.sin6_addr), srcentry_ptr->incoming,
		    inet6_fmt(&neighbor_addr.sin6_addr));
	    return (TRUE);
	}
	else
	    break;
    }

    /* TODO: control the number of messages! */
    log(LOG_INFO, 0,
	"For src %s, iif is %d, next hop router is %s: NOT A PIM ROUTER",
	inet6_fmt(&source.sin6_addr), srcentry_ptr->incoming,
	inet6_fmt(&neighbor_addr.sin6_addr));

    srcentry_ptr->upstream = (pim_nbr_entry_t *) NULL;

    return (FALSE);
}


/*
 * TODO: XXX: currently `source` is not used. Will be used with IGMPv3 where
 * we have source-specific Join/Prune.
 */

void
add_leaf(vifi, source, group)
    vifi_t          vifi;
    struct sockaddr_in6	*source;
    struct sockaddr_in6	*group;
{
    mrtentry_t     *mrtentry_ptr;
    if_set     old_oifs;
    if_set     new_oifs;
    if_set     new_leaves;


    mrtentry_ptr = find_route(&sockaddr6_any, group, MRTF_WC, CREATE);

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return;

    if ((mrtentry_ptr->incoming == vifi)
	&& (!(uvifs[vifi].uv_flags & VIFF_DR)))
    {
	/*
	 * The report is received on the iif for this routing entry and I am
	 * not the DR for that subnet. Ignore it.
	 */

	if (mrtentry_ptr->flags & MRTF_NEW)
	    delete_mrtentry(mrtentry_ptr);
	return;
    }

    IF_DEBUG(DEBUG_MRT)
	log(LOG_DEBUG, 0, "Adding vif %d for group %s", vifi,
	    inet6_fmt(&group->sin6_addr));

    if (IF_ISSET(vifi, &mrtentry_ptr->leaves))
	return;			/* Already a leaf */
    calc_oifs(mrtentry_ptr, &old_oifs);
    IF_COPY(&mrtentry_ptr->leaves, &new_leaves);
    IF_SET(vifi, &new_leaves);	/* Add the leaf */
    change_interfaces(mrtentry_ptr,
		      mrtentry_ptr->incoming,
		      &mrtentry_ptr->joined_oifs,
		      &mrtentry_ptr->pruned_oifs,
		      &new_leaves,
		      &mrtentry_ptr->asserted_oifs, 0);
    calc_oifs(mrtentry_ptr, &new_oifs);

    if ((mrtentry_ptr->flags & MRTF_NEW)
	|| (IF_ISEMPTY(&old_oifs) && (!IF_ISEMPTY(&new_oifs))))
    {

	/*
	 * A new created entry or the oifs have changed from NULL to
	 * non-NULL.
	 */

	mrtentry_ptr->flags &= ~MRTF_NEW;
	FIRE_TIMER(mrtentry_ptr->jp_timer);	/* Timeout the Join/Prune
						 * timer */
	/*
	 * TODO: explicitly call the function below?
	 * send_pim6_join_prune(mrtentry_ptr->upstream->vifi,
	 * mrtentry_ptr->upstream, pim_join_prune_holdtime);
	 */
    }
}


/*
 * TODO: XXX: currently `source` is not used. To be used with IGMPv3 where we
 * have source-specific joins/prunes.
 */
void
delete_leaf(vifi, source, group)
    vifi_t          vifi;
    struct sockaddr_in6 *source;
    struct sockaddr_in6 *group;
{
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_srcs;
    if_set			new_oifs;
    if_set     old_oifs;
    if_set     new_leaves;

    mrtentry_ptr = find_route(&sockaddr6_any, group, MRTF_WC, DONT_CREATE);
    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return;

    if (!IF_ISSET(vifi, &mrtentry_ptr->leaves))
	return;			/* This interface wasn't leaf */

    calc_oifs(mrtentry_ptr, &old_oifs);
    IF_COPY(&mrtentry_ptr->leaves, &new_leaves);
    IF_CLR(vifi, &new_leaves);
    change_interfaces(mrtentry_ptr,
		      mrtentry_ptr->incoming,
		      &mrtentry_ptr->joined_oifs,
		      &mrtentry_ptr->pruned_oifs,
		      &new_leaves,
		      &mrtentry_ptr->asserted_oifs, 0);
    calc_oifs(mrtentry_ptr, &new_oifs);
    if ((!IF_ISEMPTY(&old_oifs)) && IF_ISEMPTY(&new_oifs))
    {
	/* The result oifs have changed from non-NULL to NULL */
	FIRE_TIMER(mrtentry_ptr->jp_timer);	/* Timeout the Join/Prune
						 * timer */
	/*
	 * TODO: explicitly call the function?
	 * send_pim6_join_prune(mrtentry_ptr->upstream->vifi,
	 * mrtentry_ptr->upstream, pim_join_prune_holdtime);
	 */
    }
    /*
     * Check all (S,G) entries and clear the inherited "leaf" flag. TODO:
     * XXX: This won't work for IGMPv3, because there we don't know whether
     * the (S,G) leaf oif was inherited from the (*,G) entry or was created
     * by source specific IGMP join.
     */
    for (mrtentry_srcs = mrtentry_ptr->group->mrtlink;
	 mrtentry_srcs != (mrtentry_t *) NULL;
	 mrtentry_srcs = mrtentry_srcs->grpnext)
    {
	IF_COPY(&mrtentry_srcs->leaves, &new_leaves);
	IF_CLR(vifi, &new_leaves);
	change_interfaces(mrtentry_srcs,
			  mrtentry_srcs->incoming,
			  &mrtentry_srcs->joined_oifs,
			  &mrtentry_srcs->pruned_oifs,
			  &new_leaves,
			  &mrtentry_srcs->asserted_oifs, 0);
    }

}


void
calc_oifs(mrtentry_ptr, oifs_ptr)
    mrtentry_t     *mrtentry_ptr;
    if_set    	   *oifs_ptr;
{
    if_set     oifs;
    mrtentry_t     *grp_route;
    mrtentry_t     *rp_route;

    /*
     * oifs = (((copied_outgoing + my_join) - my_prune) + my_leaves) -
     * my_asserted_oifs - incoming_interface, i.e. `leaves` have higher
     * priority than `prunes`, but lower priority than `asserted`. The
     * incoming interface is always deleted from the oifs
     */

    if (mrtentry_ptr == (mrtentry_t *) NULL)
    {
	IF_ZERO(oifs_ptr);
	return;
    }
    IF_ZERO(&oifs);
    if (!(mrtentry_ptr->flags & MRTF_PMBR))
    {
	/* Either (*,G) or (S,G). Merge with the oifs from the (*,*,RP) */
	if ((rp_route =
	     mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink)
	    != (mrtentry_t *) NULL)
	{
	    IF_MERGE(&oifs, &rp_route->joined_oifs, &oifs);
	    IF_CLR_MASK(&oifs, &rp_route->pruned_oifs);
	    IF_MERGE(&oifs, &rp_route->leaves, &oifs);
	    IF_CLR_MASK(&oifs, &rp_route->asserted_oifs);
	}
    }
    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/* (S,G) entry. Merge with the oifs from (*,G) */
	if ((grp_route = mrtentry_ptr->group->grp_route)
	    != (mrtentry_t *) NULL)
	{
	    IF_MERGE(&oifs, &grp_route->joined_oifs, &oifs);
	    IF_CLR_MASK(&oifs, &grp_route->pruned_oifs);
	    IF_MERGE(&oifs, &grp_route->leaves, &oifs);
	    IF_CLR_MASK(&oifs, &grp_route->asserted_oifs);
	}
    }

    /* Calculate my own stuff */
    IF_MERGE(&oifs, &mrtentry_ptr->joined_oifs, &oifs);
    IF_CLR_MASK(&oifs, &mrtentry_ptr->pruned_oifs);
    IF_MERGE(&oifs, &mrtentry_ptr->leaves, &oifs);
    IF_CLR_MASK(&oifs, &mrtentry_ptr->asserted_oifs);

    IF_CLR(mrtentry_ptr->incoming, &oifs);
    IF_COPY(&oifs, oifs_ptr);
}

/*
 * Set the iif, join/prune/leaves/asserted interfaces. Calculate and set the
 * oifs. Return 1 if oifs change from NULL to not-NULL. Return -1 if oifs
 * change from non-NULL to NULL else return 0 If the iif change or if the
 * oifs change from NULL to non-NULL or vice-versa, then schedule that
 * mrtentry join/prune timer to timeout immediately.
 */
int
change_interfaces(mrtentry_ptr, new_iif, new_joined_oifs_, new_pruned_oifs,
		  new_leaves_, new_asserted_oifs, flags)
    mrtentry_t     *mrtentry_ptr;
    vifi_t          new_iif;
    if_set      *new_joined_oifs_;
    if_set     *new_pruned_oifs;
    if_set     *new_leaves_;
    if_set     *new_asserted_oifs;
    u_int16         flags;
{
    if_set         new_joined_oifs;	/* The oifs for that particular
					 * mrtentry */
    if_set     old_joined_oifs;
    if_set     old_pruned_oifs;
    if_set     old_leaves;
    if_set     new_leaves;
    if_set     old_asserted_oifs;
    if_set     new_real_oifs;	/* The result oifs */
    if_set     old_real_oifs;
    vifi_t          old_iif;
    rpentry_t      *rpentry_ptr;
    cand_rp_t      *cand_rp_ptr;
    kernel_cache_t *kernel_cache_ptr;
    rp_grp_entry_t *rp_grp_entry_ptr;
    grpentry_t     *grpentry_ptr;
    mrtentry_t     *mrtentry_srcs;
    mrtentry_t     *mrtentry_wc;
    mrtentry_t     *mrtentry_rp;
    int             delete_mrtentry_flag;
    int             return_value;
    int             fire_timer_flag;

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return (0);

    IF_COPY(new_joined_oifs_, &new_joined_oifs);
    IF_COPY(new_leaves_, &new_leaves);

    old_iif = mrtentry_ptr->incoming;
    IF_COPY(&mrtentry_ptr->joined_oifs, &old_joined_oifs);
    IF_COPY(&mrtentry_ptr->leaves, &old_leaves);
    IF_COPY(&mrtentry_ptr->pruned_oifs, &old_pruned_oifs);
    IF_COPY(&mrtentry_ptr->asserted_oifs, &old_asserted_oifs);

    IF_COPY(&mrtentry_ptr->oifs, &old_real_oifs);

    mrtentry_ptr->incoming = new_iif;
    IF_COPY(&new_joined_oifs, &mrtentry_ptr->joined_oifs);
    IF_COPY(new_pruned_oifs, &mrtentry_ptr->pruned_oifs);
    IF_COPY(&new_leaves, &mrtentry_ptr->leaves);
    IF_COPY(new_asserted_oifs, &mrtentry_ptr->asserted_oifs);
    calc_oifs(mrtentry_ptr, &new_real_oifs);

    if (IF_ISEMPTY(&old_real_oifs))
    {
	if (IF_ISEMPTY(&new_real_oifs))
	    return_value = 0;
	else
	    return_value = 1;
    }
    else
    {
	if (IF_ISEMPTY(&new_real_oifs))
	    return_value = -1;
	else
	    return_value = 0;
    }

    if ((IF_SAME(&new_real_oifs, &old_real_oifs))
	&& (new_iif == old_iif)
	&& !(flags & MFC_UPDATE_FORCE))
	return 0;		/* Nothing to change */

    if ((return_value != 0) || (new_iif != old_iif)
	|| (flags & MFC_UPDATE_FORCE))
	FIRE_TIMER(mrtentry_ptr->jp_timer);

    IF_COPY(&new_real_oifs, &mrtentry_ptr->oifs);

    if (mrtentry_ptr->flags & MRTF_PMBR)
    {
	/* (*,*,RP) entry */
	rpentry_ptr = mrtentry_ptr->source;
	if (rpentry_ptr == (rpentry_t *) NULL)
	    return (0);		/* Shoudn't happen */
	rpentry_ptr->incoming = new_iif;
	cand_rp_ptr = rpentry_ptr->cand_rp;

	if (IF_ISEMPTY(&new_real_oifs))
	{
	    delete_mrtentry_flag = TRUE;
	}
	else
	{
	    delete_mrtentry_flag = FALSE;
#ifdef RSRR
	    rsrr_cache_send(mrtentry_ptr, RSRR_NOTIFICATION_OK);
#endif				/* RSRR */
	}

	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	{
	    /* Update the kernel MFC entries */
	    if (delete_mrtentry_flag == TRUE)
		/*
		 * XXX: no need to send RSRR message. Will do it when delete
		 * the mrtentry.
		 */
		for (kernel_cache_ptr = mrtentry_ptr->kernel_cache;
		     kernel_cache_ptr != (kernel_cache_t *) NULL;
		     kernel_cache_ptr = kernel_cache_ptr->next)
		    delete_mrtentry_all_kernel_cache(mrtentry_ptr);
	    else
	    {
		for (kernel_cache_ptr = mrtentry_ptr->kernel_cache;
		     kernel_cache_ptr != (kernel_cache_t *) NULL;
		     kernel_cache_ptr = kernel_cache_ptr->next)
		    /* here mrtentry_ptr->source->address is the RP address */
		    k_chg_mfc(mld6_socket, &kernel_cache_ptr->source,
			      &kernel_cache_ptr->group, new_iif,
			      &new_real_oifs, &mrtentry_ptr->source->address);
	    }
	}

	/*
	 * Update all (*,G) entries associated with this RP. The particular
	 * (*,G) outgoing are not changed, but the change in the (*,*,RP)
	 * oifs may have affect the real oifs.
	 */
	fire_timer_flag = FALSE;
	for (rp_grp_entry_ptr = cand_rp_ptr->rp_grp_next;
	     rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
	     rp_grp_entry_ptr = rp_grp_entry_ptr->rp_grp_next)
	{
	    for (grpentry_ptr = rp_grp_entry_ptr->grplink;
		 grpentry_ptr != (grpentry_t *) NULL;
		 grpentry_ptr = grpentry_ptr->rpnext)
	    {
		if (grpentry_ptr->grp_route != (mrtentry_t *) NULL)
		{
		    if (change_interfaces(grpentry_ptr->grp_route, new_iif,
				       &grpentry_ptr->grp_route->joined_oifs,
				       &grpentry_ptr->grp_route->pruned_oifs,
					  &grpentry_ptr->grp_route->leaves,
				     &grpentry_ptr->grp_route->asserted_oifs,
					  flags))
			fire_timer_flag = TRUE;
		}
		else
		{
		    /* Change all (S,G) entries if no (*,G) */
		    for (mrtentry_srcs = grpentry_ptr->mrtlink;
			 mrtentry_srcs != (mrtentry_t *) NULL;
			 mrtentry_srcs = mrtentry_srcs->grpnext)
		    {
			if (mrtentry_srcs->flags & MRTF_RP)
			{
			    if (change_interfaces(mrtentry_srcs, new_iif,
						  &mrtentry_srcs->joined_oifs,
						  &mrtentry_srcs->pruned_oifs,
						  &mrtentry_srcs->leaves,
					       &mrtentry_srcs->asserted_oifs,
						  flags))
				fire_timer_flag = TRUE;
			}
			else
			{
			    if (change_interfaces(mrtentry_srcs,
						  mrtentry_srcs->incoming,
						  &mrtentry_srcs->joined_oifs,
						  &mrtentry_srcs->pruned_oifs,
						  &mrtentry_srcs->leaves,
					       &mrtentry_srcs->asserted_oifs,
						  flags))
				fire_timer_flag = TRUE;
			}
		    }
		}
	    }
	}
	if (fire_timer_flag == TRUE)
	    FIRE_TIMER(mrtentry_ptr->jp_timer);
	if (delete_mrtentry_flag == TRUE)
	{
	    /*
	     * TODO: XXX: trigger a Prune message? Don't delete now, it will
	     * be automatically timed out. If want to delete now, don't
	     * reference to it anymore! delete_mrtentry(mrtentry_ptr);
	     */
	}
	return (return_value);	/* (*,*,RP) */
    }

    if (mrtentry_ptr->flags & MRTF_WC)
    {
	/* (*,G) entry */
	if (IF_ISEMPTY(&new_real_oifs))
	    delete_mrtentry_flag = TRUE;
	else
	{
	    delete_mrtentry_flag = FALSE;
#ifdef RSRR
	    rsrr_cache_send(mrtentry_ptr, RSRR_NOTIFICATION_OK);
#endif				/* RSRR */
	}
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	{
	    if (delete_mrtentry_flag == TRUE)
		delete_mrtentry_all_kernel_cache(mrtentry_ptr);
	    else
	    {
		for (kernel_cache_ptr = mrtentry_ptr->kernel_cache;
		     kernel_cache_ptr != (kernel_cache_t *) NULL;
		     kernel_cache_ptr = kernel_cache_ptr->next)
		    k_chg_mfc(mld6_socket, &kernel_cache_ptr->source,
			      &kernel_cache_ptr->group, new_iif,
			      &new_real_oifs, &mrtentry_ptr->group->rpaddr);
	    }
	}
	/*
	 * Update all (S,G) entries for this group. For the (S,G)RPbit
	 * entries the iif is the iif toward the RP; The particular (S,G)
	 * oifs are not changed, but the change in the (*,G) oifs may affect
	 * the real oifs.
	 */
	fire_timer_flag = FALSE;
	for (mrtentry_srcs = mrtentry_ptr->group->mrtlink;
	     mrtentry_srcs != (mrtentry_t *) NULL;
	     mrtentry_srcs = mrtentry_srcs->grpnext)
	{
	    if (mrtentry_srcs->flags & MRTF_RP)
	    {
		if (change_interfaces(mrtentry_srcs, new_iif,
				      &mrtentry_srcs->joined_oifs,
				      &mrtentry_srcs->pruned_oifs,
				      &mrtentry_srcs->leaves,
				      &mrtentry_srcs->asserted_oifs, flags))
		    fire_timer_flag = TRUE;
	    }
	    else
	    {
		if (change_interfaces(mrtentry_srcs, mrtentry_srcs->incoming,
				      &mrtentry_srcs->joined_oifs,
				      &mrtentry_srcs->pruned_oifs,
				      &mrtentry_srcs->leaves,
				      &mrtentry_srcs->asserted_oifs, flags))
		    fire_timer_flag = TRUE;
	    }
	}

	if (fire_timer_flag == TRUE)
	    FIRE_TIMER(mrtentry_ptr->jp_timer);
	if (delete_mrtentry_flag == TRUE)
	{
	    /* TODO: XXX: the oifs are NULL. Send a Prune message? */
	}
	return (return_value);	/* (*,G) */
    }

    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/* (S,G) entry */
#ifdef KERNEL_MFC_WC_G
	if_set     tmp_oifs;
	mrtentry_t     *mrtentry_tmp;
#endif				/* KERNEL_MFC_WC_G */

	mrtentry_rp = mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	mrtentry_wc = mrtentry_ptr->group->grp_route;
#ifdef KERNEL_MFC_WC_G
	/*
	 * Check whether (*,*,RP) or (*,G) have different (iif,oifs) from the
	 * (S,G). If "yes", then forbid creating (*,G) MFC.
	 */
	for (mrtentry_tmp = mrtentry_rp; 1; mrtentry_tmp = mrtentry_wc)
	{
	    for (; 1;)
	    {
		if (mrtentry_tmp == (mrtentry_t *) NULL)
		    break;
		if (mrtentry_tmp->flags & MRTF_MFC_CLONE_SG)
		    break;
		if (mrtentry_tmp->incoming != mrtentry_ptr->incoming)
		{
		    delete_single_kernel_cache_addr(mrtentry_tmp, IN6ADDR_ANY_N,
						mrtentry_ptr->group->group);
		    mrtentry_tmp->flags |= MRTF_MFC_CLONE_SG;
		    break;
		}
		calc_oifs(mrtentry_tmp, &tmp_oifs);
		if (!(IF_SAME(&new_real_oifs, &tmp_oifs)))
		    mrtentry_tmp->flags |= MRTF_MFC_CLONE_SG;
		break;
	    }
	    if (mrtentry_tmp == mrtentry_wc)
		break;
	}
#endif				/* KERNEL_MFC_WC_G */

	if (IF_ISEMPTY(&new_real_oifs))
	    delete_mrtentry_flag = TRUE;
	else
	{
	    delete_mrtentry_flag = FALSE;
#ifdef RSRR
	    rsrr_cache_send(mrtentry_ptr, RSRR_NOTIFICATION_OK);
#endif				/* RSRR */
	}
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	{
	    if (delete_mrtentry_flag == TRUE)
		delete_mrtentry_all_kernel_cache(mrtentry_ptr);
	    else
	    {
		k_chg_mfc(mld6_socket, &mrtentry_ptr->source->address,
			  &mrtentry_ptr->group->group, new_iif, &new_real_oifs,
			  &mrtentry_ptr->group->rpaddr);
	    }
	}
	if (old_iif != new_iif)
	{
	    if (new_iif == mrtentry_ptr->source->incoming)
	    {
		/*
		 * For example, if this was (S,G)RPbit with iif toward the
		 * RP, and now switch to the Shortest Path. The setup of
		 * MRTF_SPT flag must be done by the external calling
		 * function (triggered only by receiving of a data from the
		 * source.)
		 */
		mrtentry_ptr->flags &= ~MRTF_RP;
		/*
		 * TODO: XXX: delete? Check again where will be the best
		 * place to set it. mrtentry_ptr->flags |= MRTF_SPT;
		 */
	    }
	    if (((mrtentry_wc != (mrtentry_t *) NULL)
		 && (mrtentry_wc->incoming == new_iif))
		|| ((mrtentry_rp != (mrtentry_t *) NULL)
		    && (mrtentry_rp->incoming == new_iif)))
	    {
		/*
		 * If the new iif points toward the RP, reset the SPT flag.
		 * (PIM-SM-spec-10.ps pp. 11, 2.10, last sentence of first
		 * paragraph.
		 */
		/* TODO: XXX: check again! */

		mrtentry_ptr->flags &= ~MRTF_SPT;
		mrtentry_ptr->flags |= MRTF_RP;
	    }
	}
	/*
	 * TODO: XXX: if this is (S,G)RPbit entry and the oifs==(*,G)oifs,
	 * then delete the (S,G) entry?? The same if we have (*,*,RP) ?
	 */
	if (delete_mrtentry_flag == TRUE)
	{
	    /* TODO: XXX: the oifs are NULL. Send a Prune message ? */
	}
	/* TODO: XXX: have the feeling something is missing.... */
	return (return_value);	/* (S,G) */
    }
    return (return_value);
}


/*
 * TODO: implement it. Required to allow changing of the physical interfaces
 * configuration without need to restart pimd.
 */
int
delete_vif_from_mrt(vifi)
    vifi_t          vifi;
{
    return TRUE;
}


void 
process_kernel_call()
{
    register struct mrt6msg *im;	/* igmpmsg control struct */

    im = (struct mrt6msg *) mld6_recv_buf;

    switch (im->im6_msgtype)
    {
    case MRT6MSG_NOCACHE:
	process_cache_miss(im);
	break;
    case MRT6MSG_WRONGMIF:
	process_wrong_iif(im);
	break;
    case MRT6MSG_WHOLEPKT:
	process_whole_pkt(mld6_recv_buf);
	break;
    default:
	IF_DEBUG(DEBUG_KERN)
	    log(LOG_DEBUG, 0, "Unknown kernel_call code");
	break;
    }
}


/*
 * TODO: when cache miss, check the iif, because probably ASSERTS shoult take
 * place
 */

static void
process_cache_miss(im)
    struct mrt6msg *im;
{
    static struct sockaddr_in6 source = {sizeof(source) , AF_INET6 };
    static struct sockaddr_in6 mfc_source = {sizeof(source) , AF_INET6 };
    static struct sockaddr_in6 group = {sizeof(group) , AF_INET6 };

    static struct sockaddr_in6 rp_addr = {sizeof(source) , AF_INET6 };
    vifi_t          iif;
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_rp;

    /*
     * When there is a cache miss, we check only the header of the packet
     * (and only it should be sent up by the kernel.)
     */
    group.sin6_addr = im->im6_dst;
    group.sin6_scope_id = inet6_uvif2scopeid(&group, &uvifs[im->im6_mif]);
    source.sin6_addr = mfc_source.sin6_addr = im->im6_src;
    source.sin6_scope_id = inet6_uvif2scopeid(&source, &uvifs[im->im6_mif]);
    iif = im->im6_mif;

    uvifs[iif].uv_cache_miss++;
    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "Cache miss, src %s, dst %s, iif %d",
	    inet6_fmt(&source.sin6_addr), inet6_fmt(&group.sin6_addr), iif);

    /*
     * TODO: XXX: check whether the kernel generates cache miss for the LAN
     * scoped addresses
     */

    /* Don't create routing entries for the LAN scoped addresses */

    if (IN6_IS_ADDR_MC_NODELOCAL(&group.sin6_addr) ||/* sanity? */
    	IN6_IS_ADDR_MC_LINKLOCAL(&group.sin6_addr))
	    goto fail;

    /* TODO: check if correct in case the source is one of my addresses */
    /*
     * If I am the DR for this source, create (S,G) and add the register_vif
     * to the oifs.
     */
    if ((uvifs[iif].uv_flags & VIFF_DR)
	&& (find_vif_direct_local(&source) == iif))
    {
	mrtentry_ptr = find_route(&source, &group, MRTF_SG, CREATE);
	if (mrtentry_ptr == (mrtentry_t *) NULL)
	{
	    goto fail;
	}

	mrtentry_ptr->flags &= ~MRTF_NEW;

	/* set reg_vif_num as outgoing interface ONLY if I am not the RP */

	if (!inet6_equal(&mrtentry_ptr->group->rpaddr, &my_cand_rp_address))
	    IF_SET(reg_vif_num, &mrtentry_ptr->joined_oifs);
	change_interfaces(mrtentry_ptr,
			  mrtentry_ptr->incoming,
			  &mrtentry_ptr->joined_oifs,
			  &mrtentry_ptr->pruned_oifs,
			  &mrtentry_ptr->leaves,
			  &mrtentry_ptr->asserted_oifs, 0);
    }
    else
    {
	mrtentry_ptr = find_route(&source, &group,
				  MRTF_SG | MRTF_WC | MRTF_PMBR,
				  DONT_CREATE);
	if (mrtentry_ptr == (mrtentry_t *) NULL)
		goto fail;
    }

    /*
     * TODO: if there are too many cache miss for the same (S,G), install
     * negative cache entry in the kernel (oif==NULL) to prevent too many
     * upcalls.
     */
    if (mrtentry_ptr->incoming == iif)
    {
	if (!IF_ISEMPTY(&mrtentry_ptr->oifs))
	{
	    if (mrtentry_ptr->flags & MRTF_SG)
	    {
		/* TODO: check that the RPbit is not set? */
		/* TODO: XXX: TIMER implem. dependency! */

		if (mrtentry_ptr->timer < pim_data_timeout)
		    SET_TIMER(mrtentry_ptr->timer, pim_data_timeout);
		if (!(mrtentry_ptr->flags & MRTF_SPT))
		{
		    if ((mrtentry_rp = mrtentry_ptr->group->grp_route) ==
			(mrtentry_t *) NULL)
			mrtentry_rp = mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
		    if (mrtentry_rp != (mrtentry_t *) NULL)
		    {
			/*
			 * Check if the (S,G) iif is different from the (*,G)
			 * or (*,*,RP) iif
			 */
			if ((mrtentry_ptr->incoming != mrtentry_rp->incoming)
			|| (mrtentry_ptr->upstream != mrtentry_rp->upstream))
			{
			    mrtentry_ptr->flags |= MRTF_SPT;
			    mrtentry_ptr->flags &= ~MRTF_RP;
			}
		    }
		}
	    }
	    if (mrtentry_ptr->flags & MRTF_PMBR)
		rp_addr = mrtentry_ptr->source->address;
	    else
		rp_addr = mrtentry_ptr->group->rpaddr;
	    mfc_source = source;
// TODO 
#ifdef KERNEL_MFC_WC_G
	    if (mrtentry_ptr->flags & (MRTF_WC | MRTF_PMBR))
		if (!(mrtentry_ptr->flags & MRTF_MFC_CLONE_SG))
		    mfc_source = IN6ADDR_ANY_N;
#endif				/* KERNEL_MFC_WC_G */

	    add_kernel_cache(mrtentry_ptr, &mfc_source, &group, MFC_MOVE_FORCE);
	    k_chg_mfc(mld6_socket, &mfc_source, &group, iif, &mrtentry_ptr->oifs,
		      &rp_addr);
	    /*
	     * TODO: XXX: No need for RSRR message, because nothing has
	     * changed.
	     */
	}
	return;			/* iif match */
    }

    /* The iif doesn't match */
    if (mrtentry_ptr->flags & MRTF_SG)
    {
	if (mrtentry_ptr->flags & MRTF_SPT)
	    /* Arrived on wrong interface */
		goto fail;
	if ((mrtentry_rp = mrtentry_ptr->group->grp_route) ==
	    (mrtentry_t *) NULL)
	    mrtentry_rp =
		mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_rp != (mrtentry_t *) NULL)
	{
	    if (mrtentry_rp->incoming == iif)
	    {
		/* Forward on (*,G) or (*,*,RP) */

#ifdef KERNEL_MFC_WC_G
		if (!(mrtentry_rp->flags & MRTF_MFC_CLONE_SG))
		    mfc_source = IN6ADDR_ANY_N;
#endif				/* KERNEL_MFC_WC_G */

		add_kernel_cache(mrtentry_rp, &mfc_source, &group, 0);
		k_chg_mfc(mld6_socket, &mfc_source, &group, iif,
			  &mrtentry_rp->oifs, &mrtentry_ptr->group->rpaddr);
#ifdef RSRR
		rsrr_cache_send(mrtentry_rp, RSRR_NOTIFICATION_OK);
#endif				/* RSRR */

		return;
	    }
	}
	goto fail;
    }

  fail:
    uvifs[iif].uv_cache_notcreated++;
}


/*
 * A multicast packet has been received on wrong iif by the kernel. Check for
 * a matching entry. If there is (S,G) with reset SPTbit and the packet was
 * received on the iif toward the source, this completes the switch to the
 * shortest path and triggers (S,G) prune toward the RP (unless I am the RP).
 * Otherwise, if the packet's iif is in the oiflist of the routing entry,
 * trigger an Assert.
 */

static void
process_wrong_iif(im)
    struct mrt6msg *im;
{
    static struct sockaddr_in6 source= {sizeof(source) , AF_INET6};
    static struct sockaddr_in6 group = {sizeof(group) , AF_INET6};
    vifi_t          iif;
    mrtentry_t     *mrtentry_ptr;

    group.sin6_addr = im->im6_dst;
    source.sin6_addr = im->im6_src;
    iif = im->im6_mif;


    /* Don't create routing entries for the LAN scoped addresses */
    if (IN6_IS_ADDR_MC_NODELOCAL(&group.sin6_addr) ||/* sanity? */
    IN6_IS_ADDR_MC_LINKLOCAL(&group.sin6_addr))
    return;


    /*
     * Ignore if it comes on register vif. register vif is neither SPT iif,
     * neither is used to send asserts out.
     */
    if (uvifs[iif].uv_flags & MIFF_REGISTER)
	return;

    mrtentry_ptr = find_route(&source, &group, MRTF_SG | MRTF_WC | MRTF_PMBR,
			      DONT_CREATE);
    if (mrtentry_ptr == (mrtentry_t *)NULL)
	    return;

    /*
     * TODO: check again!
     */
    if (mrtentry_ptr->flags & MRTF_SG)
    {
	if (!(mrtentry_ptr->flags & MRTF_SPT))
	{
	    if (mrtentry_ptr->source->incoming == iif)
	    {
		/* Switch to the Shortest Path */
		mrtentry_ptr->flags |= MRTF_SPT;
		mrtentry_ptr->flags &= ~MRTF_RP;
		add_kernel_cache(mrtentry_ptr, &source, &group, MFC_MOVE_FORCE);
		k_chg_mfc(mld6_socket, &source, &group, iif,
			  &mrtentry_ptr->oifs, &mrtentry_ptr->group->rpaddr);
		FIRE_TIMER(mrtentry_ptr->jp_timer);
#ifdef RSRR
		rsrr_cache_send(mrtentry_ptr, RSRR_NOTIFICATION_OK);
#endif				/* RSRR */
		return;
	    }
	}
    }

    /* Trigger an Assert */
    if (IF_ISSET(iif, &mrtentry_ptr->oifs))
	send_pim6_assert(&source, &group, iif, mrtentry_ptr);
}

/*
 * Receives whole packets from the register vif entries in the kernel, and
 * calls the send_pim_register procedure to encapsulate the packets and
 * unicasts them to the RP.
 */
static void
process_whole_pkt(buf)
    char *buf;
{

   send_pim6_register((char *) (buf + sizeof(struct mrt6msg)));

}


mrtentry_t     *
switch_shortest_path(source, group)
    struct sockaddr_in6		*source;
    struct sockaddr_in6		*group;
{
    mrtentry_t     *mrtentry_ptr;

    /* TODO: XXX: prepare and send immediately the (S,G) join? */
    if ((mrtentry_ptr = find_route(source, group, MRTF_SG, CREATE)) !=
	(mrtentry_t *) NULL)
    {
	if (mrtentry_ptr->flags & MRTF_NEW)
	{
	    mrtentry_ptr->flags &= ~MRTF_NEW;
	}
	else
	{
	    if (mrtentry_ptr->flags & MRTF_RP)
	    {
		/*
		 * (S,G)RPbit with iif toward RP. Reset to (S,G) with iif
		 * toward S. Delete the kernel cache (if any), because
		 * change_interfaces() will reset it with iif toward S and no
		 * data will arrive from RP before the switch really occurs.
		 */
		mrtentry_ptr->flags &= ~MRTF_RP;
		mrtentry_ptr->incoming = mrtentry_ptr->source->incoming;
		mrtentry_ptr->upstream = mrtentry_ptr->source->upstream;
		delete_mrtentry_all_kernel_cache(mrtentry_ptr);
		change_interfaces(mrtentry_ptr,
				  mrtentry_ptr->incoming,
				  &mrtentry_ptr->joined_oifs,
				  &mrtentry_ptr->pruned_oifs,
				  &mrtentry_ptr->leaves,
				  &mrtentry_ptr->asserted_oifs, 0);
	    }
	}

	SET_TIMER(mrtentry_ptr->timer, pim_data_timeout);
	FIRE_TIMER(mrtentry_ptr->jp_timer);
    }
    return (mrtentry_ptr);
}

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
 *  $Id: mrt.c,v 1.3 2000/05/18 16:09:39 itojun Exp $
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
 * $FreeBSD: src/usr.sbin/pim6dd/mrt.c,v 1.1.2.1 2000/07/15 07:36:29 kris Exp $
 */

#include "defs.h"

srcentry_t		*srclist;
grpentry_t		*grplist;

/*
 * Local functions definition
 */
static srcentry_t *create_srcentry     __P((struct sockaddr_in6 *source));
static int        search_srclist       __P((struct sockaddr_in6 *source,
					    srcentry_t **sourceEntry));
static int        search_srcmrtlink    __P((srcentry_t *srcentry_ptr,
					    struct sockaddr_in6 *group,
					    mrtentry_t **mrtPtr));
static void       insert_srcmrtlink    __P((mrtentry_t *elementPtr,
					    mrtentry_t *insertPtr,
					    srcentry_t *srcListPtr));
static grpentry_t *create_grpentry     __P((struct sockaddr_in6 *group));
static int        search_grplist       __P((struct sockaddr_in6 *group,
					    grpentry_t **groupEntry));
static int        search_grpmrtlink    __P((grpentry_t *grpentry_ptr,
					    struct sockaddr_in6 *source,
					    mrtentry_t **mrtPtr));
static void       insert_grpmrtlink    __P((mrtentry_t *elementPtr,
					    mrtentry_t *insertPtr,
					    grpentry_t *grpListPtr));
static mrtentry_t *alloc_mrtentry      __P((srcentry_t *srcentry_ptr,
					    grpentry_t *grpentry_ptr));
static mrtentry_t *create_mrtentry     __P((srcentry_t *srcentry_ptr,
					    grpentry_t *grpentry_ptr,
					    u_int16 flags));


void 
init_pim6_mrt()
{

    /* TODO: delete any existing routing table */

    /* Initialize the source list */
    /* The first entry has the unspecified address and is not used */
    /* The order is the smallest address first. */
    srclist	        = (srcentry_t *)malloc(sizeof(srcentry_t));
    srclist->next       = (srcentry_t *)NULL;
    srclist->prev       = (srcentry_t *)NULL;
    memset(&srclist->address, 0, sizeof(struct sockaddr_in6));
    srclist->address.sin6_len = sizeof(struct sockaddr_in6);
    srclist->address.sin6_family = AF_INET6;
    srclist->mrtlink    = (mrtentry_t *)NULL;
    srclist->incoming   = NO_VIF;
    srclist->upstream   = (pim_nbr_entry_t *)NULL;
    srclist->metric     = 0;
    srclist->preference = 0;
    srclist->timer      = 0;
    
    /* Initialize the group list */
    /* The first entry has the unspecified address and is not used */
    /* The order is the smallest address first. */
    grplist	        = (grpentry_t *)malloc(sizeof(grpentry_t));
    grplist->next       = (grpentry_t *)NULL;
    grplist->prev       = (grpentry_t *)NULL;
    memset(&grplist->group, 0, sizeof(struct sockaddr_in6));
    grplist->group.sin6_len = sizeof(struct sockaddr_in6);
    grplist->group.sin6_family = AF_INET6;
    grplist->mrtlink    = (mrtentry_t *)NULL;
}


grpentry_t*
find_group(group)
    struct sockaddr_in6 *group;
{
    grpentry_t *grpentry_ptr;

    if (!IN6_IS_ADDR_MULTICAST(&group->sin6_addr))
	return (grpentry_t *)NULL;
    
    if (search_grplist(group, &grpentry_ptr) == TRUE) {
	/* Group found! */
	return (grpentry_ptr);
    }
    return (grpentry_t *)NULL;
}


srcentry_t *
find_source(source)
    struct sockaddr_in6 *source;
{
    srcentry_t *srcentry_ptr;

    if (!inet6_valid_host(source))
	return (srcentry_t *)NULL;
    
    if (search_srclist(source, &srcentry_ptr) == TRUE) {
	/* Source found! */
	return (srcentry_ptr);
    }
    return (srcentry_t *)NULL;
}


mrtentry_t *
find_route(source, group, flags, create)
    struct sockaddr_in6 *source, *group;
    u_int16 flags;
    char create;
{
    srcentry_t *srcentry_ptr;
    grpentry_t *grpentry_ptr;
    mrtentry_t *mrtentry_ptr;

    if (!IN6_IS_ADDR_MULTICAST(&group->sin6_addr))
	return (mrtentry_t *)NULL;
    
    if (!inet6_valid_host(source))
	return (mrtentry_t *)NULL;
    
    if (create == DONT_CREATE) {
	if (search_grplist(group, &grpentry_ptr) == FALSE) 
	    return (mrtentry_t *)NULL;
	/* Search for the source */
	if (search_grpmrtlink(grpentry_ptr, source,
			      &mrtentry_ptr) == TRUE) {
	    /* Exact (S,G) entry found */
	    return (mrtentry_ptr);
	}
	return (mrtentry_t *)NULL;
    }


    /* Creation allowed */

    grpentry_ptr = create_grpentry(group);
    if (grpentry_ptr == (grpentry_t *)NULL) {
	return (mrtentry_t *)NULL;
    }

    /* Setup the (S,G) routing entry */
    srcentry_ptr = create_srcentry(source);
    if (srcentry_ptr == (srcentry_t *)NULL) {
	if (grpentry_ptr->mrtlink == (mrtentry_t *)NULL) {
	    /* New created grpentry. Delete it. */
	    delete_grpentry(grpentry_ptr);
	}
	return (mrtentry_t *)NULL;
    }

    mrtentry_ptr = create_mrtentry(srcentry_ptr, grpentry_ptr, MRTF_SG);
    if (mrtentry_ptr == (mrtentry_t *)NULL) {
	if (grpentry_ptr->mrtlink == (mrtentry_t *)NULL) {
	    /* New created grpentry. Delete it. */
	    delete_grpentry(grpentry_ptr);
	}
	if (srcentry_ptr->mrtlink == (mrtentry_t *)NULL) {
	    /* New created srcentry. Delete it. */
	    delete_srcentry(srcentry_ptr);
	}
	return (mrtentry_t *)NULL;
    }
    
    if (mrtentry_ptr->flags & MRTF_NEW) {
	struct mrtfilter *f;
	/* The mrtentry pref/metric should be the pref/metric of the 
	 * _upstream_ assert winner. Since this isn't known now, 
	 * set it to the config'ed default
	 */
	mrtentry_ptr->incoming = srcentry_ptr->incoming;
	mrtentry_ptr->upstream = srcentry_ptr->upstream;
	mrtentry_ptr->metric   = srcentry_ptr->metric;
	mrtentry_ptr->preference = srcentry_ptr->preference;

	if ((f = search_filter(&group->sin6_addr)))
		IF_COPY(&f->ifset, &mrtentry_ptr->filter_oifs);
    }
    
    return (mrtentry_ptr);
}


void
delete_srcentry(srcentry_ptr)
    srcentry_t *srcentry_ptr;
{
    mrtentry_t *mrtentry_ptr;
    mrtentry_t *mrtentry_next;

    if (srcentry_ptr == (srcentry_t *)NULL)
	return;
    /* TODO: XXX: the first entry is unused and always there */
    srcentry_ptr->prev->next = 	srcentry_ptr->next;
    if (srcentry_ptr->next != (srcentry_t *)NULL)
	srcentry_ptr->next->prev = srcentry_ptr->prev;
    
    for (mrtentry_ptr = srcentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *)NULL;
	 mrtentry_ptr = mrtentry_next) {
	mrtentry_next = mrtentry_ptr->srcnext;
	if (mrtentry_ptr->grpprev != (mrtentry_t *)NULL)
	    mrtentry_ptr->grpprev->grpnext = mrtentry_ptr->grpnext;
	else {
	    mrtentry_ptr->group->mrtlink = mrtentry_ptr->grpnext;
	    if (mrtentry_ptr->grpnext == (mrtentry_t *)NULL) {
		/* Delete the group entry */
		delete_grpentry(mrtentry_ptr->group);
	    }
	}
	if (mrtentry_ptr->grpnext != (mrtentry_t *)NULL)
	    mrtentry_ptr->grpnext->grpprev = mrtentry_ptr->grpprev;
	FREE_MRTENTRY(mrtentry_ptr);
    }
    free((char *)srcentry_ptr);
}


void
delete_grpentry(grpentry_ptr)
    grpentry_t *grpentry_ptr;
{
    mrtentry_t *mrtentry_ptr;
    mrtentry_t *mrtentry_next;
    
    if (grpentry_ptr == (grpentry_t *)NULL)
	return;
    /* TODO: XXX: the first entry is unused and always there */
    grpentry_ptr->prev->next = grpentry_ptr->next;
    if (grpentry_ptr->next != (grpentry_t *)NULL)
	grpentry_ptr->next->prev = grpentry_ptr->prev;
    
    for (mrtentry_ptr = grpentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *)NULL;
	 mrtentry_ptr = mrtentry_next) {
	mrtentry_next = mrtentry_ptr->grpnext;
	if (mrtentry_ptr->srcprev != (mrtentry_t *)NULL)
	    mrtentry_ptr->srcprev->srcnext = mrtentry_ptr->srcnext;
	else {
	    mrtentry_ptr->source->mrtlink = mrtentry_ptr->srcnext;
	    if (mrtentry_ptr->srcnext == (mrtentry_t *)NULL) {
		/* Delete the srcentry if this was the last routing entry */
		delete_srcentry(mrtentry_ptr->source);
	    }
	}
	if (mrtentry_ptr->srcnext != (mrtentry_t *)NULL)
	    mrtentry_ptr->srcnext->srcprev = mrtentry_ptr->srcprev;
	FREE_MRTENTRY(mrtentry_ptr);
    }
    free((char *)grpentry_ptr);
}


void
delete_mrtentry(mrtentry_ptr)
    mrtentry_t *mrtentry_ptr;
{
    if (mrtentry_ptr == (mrtentry_t *)NULL)
	return;

    /* Delete the kernel cache first */
    k_del_mfc(mld6_socket, &mrtentry_ptr->source->address,
		  &mrtentry_ptr->group->group);

#ifdef RSRR
    /* Tell the reservation daemon */
    rsrr_cache_clean(mrtentry_ptr);
#endif /* RSRR */

   /* (S,G) mrtentry */
    /* Delete from the grpentry MRT chain */
    if (mrtentry_ptr->grpprev != (mrtentry_t *)NULL)
	mrtentry_ptr->grpprev->grpnext = mrtentry_ptr->grpnext;
    else {
	mrtentry_ptr->group->mrtlink = mrtentry_ptr->grpnext;
	if (mrtentry_ptr->grpnext == (mrtentry_t *)NULL) {
	    /* Delete the group entry */
	    delete_grpentry(mrtentry_ptr->group);
	}
    }
	
    if (mrtentry_ptr->grpnext != (mrtentry_t *)NULL)
	mrtentry_ptr->grpnext->grpprev = mrtentry_ptr->grpprev;
    
    /* Delete from the srcentry MRT chain */
    if (mrtentry_ptr->srcprev != (mrtentry_t *)NULL)
	mrtentry_ptr->srcprev->srcnext = mrtentry_ptr->srcnext;
    else {
	mrtentry_ptr->source->mrtlink = mrtentry_ptr->srcnext;
	if (mrtentry_ptr->srcnext == (mrtentry_t *)NULL) {
	    /* Delete the srcentry if this was the last routing entry */
	    delete_srcentry(mrtentry_ptr->source);
	}
    }
    if (mrtentry_ptr->srcnext != (mrtentry_t *)NULL)
	mrtentry_ptr->srcnext->srcprev = mrtentry_ptr->srcprev;

    FREE_MRTENTRY(mrtentry_ptr);
}


static int
search_srclist(source, sourceEntry)
    struct sockaddr_in6 *source;
    register srcentry_t **sourceEntry;
{
    register srcentry_t *s_prev,*s;
    
    for (s_prev = srclist, s = s_prev->next; s != (srcentry_t *)NULL;
	 s_prev = s, s = s->next) {
	/* The srclist is ordered with the smallest addresses first.
	 * The first entry is not used.
	 */
	if (inet6_lessthan(&s->address, source))
	    continue;
	if (inet6_equal(&s->address, source)) {
	    *sourceEntry = s;
	    return(TRUE);
	}
	break;  
    }
    *sourceEntry = s_prev;   /* The insertion point is between s_prev and s */
    return(FALSE);
}


static int
search_grplist(group, groupEntry)
    struct sockaddr_in6 *group;
    register grpentry_t **groupEntry;
{
    register grpentry_t *g_prev, *g;
    
    for (g_prev = grplist, g = g_prev->next; g != (grpentry_t *)NULL;
	 g_prev = g, g = g->next) {
	/* The grplist is ordered with the smallest address first.
	 * The first entry is not used.
	 */
	if (inet6_lessthan(&g->group, group))
	    continue;
	if (inet6_equal(&g->group, group)) {
	    *groupEntry = g;
	    return(TRUE);
	}
	break;
    }
    *groupEntry = g_prev;    /* The insertion point is between g_prev and g */
    return(FALSE);
}

static srcentry_t *
create_srcentry(source)
    struct sockaddr_in6 *source;
{
    register srcentry_t *srcentry_ptr;
    srcentry_t *srcentry_prev;

    if (search_srclist(source, &srcentry_prev) == TRUE)
	return (srcentry_prev);
    
    srcentry_ptr = (srcentry_t *)malloc(sizeof(srcentry_t));
    if (srcentry_ptr == (srcentry_t *)NULL) {
	log(LOG_WARNING, 0, "Memory allocation error for srcentry %s",
	    inet6_fmt(&source->sin6_addr));
	return (srcentry_t *)NULL;
    }

    srcentry_ptr->address = *source;
    /*
     * Free the memory if there is error getting the iif and
     * the next hop (upstream) router.
     */ 
    if (set_incoming(srcentry_ptr, PIM_IIF_SOURCE) == FALSE) {
	free((char *)srcentry_ptr);
	return (srcentry_t *)NULL;
    }
    srcentry_ptr->mrtlink = (mrtentry_t *)NULL;
    srcentry_ptr->timer = 0;
    srcentry_ptr->next	= srcentry_prev->next;
    srcentry_prev->next = srcentry_ptr;
    srcentry_ptr->prev = srcentry_prev;
    if (srcentry_ptr->next != (srcentry_t *)NULL)
	srcentry_ptr->next->prev = srcentry_ptr;
    
    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "create source entry, source %s",
	    inet6_fmt(&source->sin6_addr));
    return (srcentry_ptr);
}


static grpentry_t *
create_grpentry(group)
    struct sockaddr_in6 *group;
{
    register grpentry_t *grpentry_ptr;
    grpentry_t *grpentry_prev;

    if (search_grplist(group, &grpentry_prev) == TRUE)
	return (grpentry_prev);
    
    grpentry_ptr = (grpentry_t *)malloc(sizeof(grpentry_t));
    if (grpentry_ptr == (grpentry_t *)NULL) {
	log(LOG_WARNING, 0, "Memory allocation error for grpentry %s",
	    inet6_fmt(&group->sin6_addr));
	return (grpentry_t *)NULL;
    }

    grpentry_ptr->group	        = *group;
    grpentry_ptr->mrtlink       = (mrtentry_t *)NULL;

    /* Now it is safe to include the new group entry */
    grpentry_ptr->next		= grpentry_prev->next;
    grpentry_prev->next         = grpentry_ptr;
    grpentry_ptr->prev          = grpentry_prev;
    if (grpentry_ptr->next != (grpentry_t *)NULL)
	grpentry_ptr->next->prev = grpentry_ptr;
    
    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "create group entry, group %s",
	    inet6_fmt(&group->sin6_addr));
    return(grpentry_ptr);
}


/*
 * Return TRUE if the entry is found and then *mrtPtr is set to point to that
 * entry. Otherwise return FALSE and *mrtPtr points the previous entry
 * (or NULL if first in the chain.
 */
static int
search_srcmrtlink(srcentry_ptr, group, mrtPtr)
    srcentry_t *srcentry_ptr;	
    struct sockaddr_in6 *group;
    mrtentry_t **mrtPtr;
{
    register mrtentry_t *mrtentry_ptr;
    register mrtentry_t *m_prev = (mrtentry_t *)NULL;
    
    for(mrtentry_ptr = srcentry_ptr->mrtlink;
	mrtentry_ptr != (mrtentry_t *)NULL;
	m_prev = mrtentry_ptr, mrtentry_ptr = mrtentry_ptr->srcnext) {
	/* The entries are ordered with the smaller group address first.
	 * The addresses are in network order.
	 */
	if (inet6_lessthan(&mrtentry_ptr->group->group, group))
	    continue;
	if (inet6_equal(&mrtentry_ptr->group->group, group)) {
	    *mrtPtr = mrtentry_ptr;
	    return(TRUE);
	}
	break;
    }
    *mrtPtr = m_prev;
    return(FALSE);
}


/*
 * Return TRUE if the entry is found and then *mrtPtr is set to point to that
 * entry. Otherwise return FALSE and *mrtPtr points the previous entry
 * (or NULL if first in the chain.
 */
static int
search_grpmrtlink(grpentry_ptr, source, mrtPtr)
    grpentry_t *grpentry_ptr;
    struct sockaddr_in6 *source;
    mrtentry_t **mrtPtr;
{
    register mrtentry_t *mrtentry_ptr;
    register mrtentry_t *m_prev = (mrtentry_t *)NULL;
    
    for (mrtentry_ptr = grpentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *)NULL;
	 m_prev = mrtentry_ptr, mrtentry_ptr = mrtentry_ptr->grpnext) {
	/* The entries are ordered with the smaller source address first.
	 * The addresses are in network order.
	 */
	if (inet6_lessthan(&mrtentry_ptr->source->address, source))
	    continue;
	if (inet6_equal(source, &mrtentry_ptr->source->address)) {
	    *mrtPtr = mrtentry_ptr;
	    return(TRUE);
	}
	break;
    }
    *mrtPtr = m_prev;
    return(FALSE);
}


static void
insert_srcmrtlink(mrtentry_new, mrtentry_prev, srcentry_ptr)
    mrtentry_t *mrtentry_new;
    mrtentry_t *mrtentry_prev;
    srcentry_t *srcentry_ptr;
{
    if (mrtentry_prev == (mrtentry_t *)NULL) {
	/* Has to be insert as the head entry for this source */
	mrtentry_new->srcnext = srcentry_ptr->mrtlink;
	mrtentry_new->srcprev = (mrtentry_t *)NULL;
	srcentry_ptr->mrtlink = mrtentry_new;
    }
    else {
	/* Insert right after the mrtentry_prev */
	mrtentry_new->srcnext = mrtentry_prev->srcnext;
	mrtentry_new->srcprev = mrtentry_prev;
	mrtentry_prev->srcnext = mrtentry_new;
    }
    if (mrtentry_new->srcnext != (mrtentry_t *)NULL)
	mrtentry_new->srcnext->srcprev = mrtentry_new;
}


static void
insert_grpmrtlink(mrtentry_new, mrtentry_prev, grpentry_ptr)
    mrtentry_t *mrtentry_new;
    mrtentry_t *mrtentry_prev;
    grpentry_t *grpentry_ptr;
{
    if (mrtentry_prev == (mrtentry_t *)NULL) {
	/* Has to be insert as the head entry for this group */
	mrtentry_new->grpnext = grpentry_ptr->mrtlink;
	mrtentry_new->grpprev = (mrtentry_t *)NULL;
	grpentry_ptr->mrtlink = mrtentry_new;
    }
    else {
	/* Insert right after the mrtentry_prev */
	mrtentry_new->grpnext = mrtentry_prev->grpnext;
	mrtentry_new->grpprev = mrtentry_prev;
	mrtentry_prev->grpnext = mrtentry_new;
    }
    if (mrtentry_new->grpnext != (mrtentry_t *)NULL)
	mrtentry_new->grpnext->grpprev = mrtentry_new;
}


static mrtentry_t *
alloc_mrtentry(srcentry_ptr, grpentry_ptr)
    srcentry_t *srcentry_ptr;
    grpentry_t *grpentry_ptr;
{
    register mrtentry_t *mrtentry_ptr;
    u_int16 i, *i_ptr;
    u_long  *il_ptr;
    u_int8  vif_numbers;
    
    mrtentry_ptr = (mrtentry_t *)malloc(sizeof(mrtentry_t));
    if (mrtentry_ptr == (mrtentry_t *)NULL) {
	log(LOG_WARNING, 0, "alloc_mrtentry(): out of memory");
	return (mrtentry_t *)NULL;
    }
    
    /*
     * grpnext, grpprev, srcnext, srcprev will be setup when we link the
     * mrtentry to the source and group chains
     */
    mrtentry_ptr->source  = srcentry_ptr;
    mrtentry_ptr->group   = grpentry_ptr;
    mrtentry_ptr->incoming = NO_VIF;
    IF_ZERO(&mrtentry_ptr->leaves);
    IF_ZERO(&mrtentry_ptr->pruned_oifs);
    IF_ZERO(&mrtentry_ptr->oifs);
    IF_ZERO(&mrtentry_ptr->filter_oifs);
    IF_ZERO(&mrtentry_ptr->asserted_oifs);
    mrtentry_ptr->upstream = (pim_nbr_entry_t *)NULL;
    mrtentry_ptr->metric = 0;
    mrtentry_ptr->preference = 0;
#ifdef RSRR
    mrtentry_ptr->rsrr_cache = (struct rsrr_cache *)NULL;
#endif /* RSRR */

/*
 * XXX: TODO: if we are short in memory, we can reserve as few as possible
 * space for vif timers (per group and/or routing entry), but then everytime
 * when a new interfaces is configured, the router will be restarted and
 * will delete the whole routing table. The "memory is cheap" solution is
 * to reserve timer space for all potential vifs in advance and then no
 * need to delete the routing table and disturb the forwarding.
 */
#ifdef SAVE_MEMORY
    mrtentry_ptr->prune_timers = (u_int16 *)malloc(sizeof(u_int16) * numvifs);
    mrtentry_ptr->prune_delay_timerids = 
	(u_long *)malloc(sizeof(u_long) * numvifs);
    mrtentry_ptr->last_assert = (u_long *)malloc(sizeof(u_long) * numvifs);
    mrtentry_ptr->last_prune = (u_long *)malloc(sizeof(u_long) * numvifs);
    vif_numbers = numvifs;
#else
    mrtentry_ptr->prune_timers =
	(u_int16 *)malloc(sizeof(u_int16) * total_interfaces);
    mrtentry_ptr->prune_delay_timerids =
	(u_long *)malloc(sizeof(u_long) * total_interfaces);
    mrtentry_ptr->last_assert =
	(u_long *)malloc(sizeof(u_long) * total_interfaces);
    mrtentry_ptr->last_prune =
	(u_long *)malloc(sizeof(u_long) * total_interfaces);
    vif_numbers = total_interfaces;
#endif /* SAVE_MEMORY */
    if ((mrtentry_ptr->prune_timers == (u_int16 *)NULL) ||
	(mrtentry_ptr->prune_delay_timerids == (u_long *)NULL) ||
	(mrtentry_ptr->last_assert == (u_long *)NULL) ||
	(mrtentry_ptr->last_prune == (u_long *)NULL)) {
	log(LOG_WARNING, 0, "alloc_mrtentry(): out of memory");
	FREE_MRTENTRY(mrtentry_ptr);
	return (mrtentry_t *)NULL;
    }
    /* Reset the timers */
    for (i = 0, i_ptr = mrtentry_ptr->prune_timers; i < vif_numbers;
	 i++, i_ptr++)
	*i_ptr = 0;
    for (i = 0, il_ptr = mrtentry_ptr->prune_delay_timerids; i < vif_numbers;
	 i++, il_ptr++)
	*il_ptr = 0;
    for (i = 0, il_ptr = mrtentry_ptr->last_assert; i < vif_numbers;
	 i++, il_ptr++)
	*il_ptr = 0;
    for (i = 0, il_ptr = mrtentry_ptr->last_prune; i < vif_numbers;
	 i++, il_ptr++)
	*il_ptr = 0;

    mrtentry_ptr->flags = MRTF_NEW;
    mrtentry_ptr->timer = 0;
    mrtentry_ptr->join_delay_timerid = 0;
    mrtentry_ptr->assert_timer = 0;
    mrtentry_ptr->graft = (pim_graft_entry_t *)NULL;

    return(mrtentry_ptr);
}


static mrtentry_t *
create_mrtentry(srcentry_ptr, grpentry_ptr, flags)
    srcentry_t *srcentry_ptr;
    grpentry_t *grpentry_ptr;
    u_int16 flags;
{
    mrtentry_t *r_new;
    mrtentry_t *r_grp_insert, *r_src_insert; /* pointers to insert */
    struct sockaddr_in6 *source;
    struct sockaddr_in6 *group;

    /* (S,G) entry */
    source = &srcentry_ptr->address;
    group  = &grpentry_ptr->group;
    
    if (search_grpmrtlink(grpentry_ptr, source, &r_grp_insert) == TRUE) {
	return(r_grp_insert);
    }
    if (search_srcmrtlink(srcentry_ptr, group, &r_src_insert) == TRUE) {
	/*
	 * Hmmm, search_grpmrtlink() didn't find the entry, but
	 * search_srcmrtlink() did find it! Shoudn't happen. Panic!
	 */
	log(LOG_ERR, 0, "MRT inconsistency for src %s and grp %s\n",
	    inet6_fmt(&source->sin6_addr), inet6_fmt(&group->sin6_addr));
	/* not reached but to make lint happy */
	return (mrtentry_t *)NULL;
    }
    /*
     * Create and insert in group mrtlink and source mrtlink chains.
     */
    r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
    if (r_new == (mrtentry_t *)NULL)
	return (mrtentry_t *)NULL;
    /*
     * r_new has to be insert right after r_grp_insert in the
     * grp mrtlink chain and right after r_src_insert in the 
     * src mrtlink chain
     */
    insert_grpmrtlink(r_new, r_grp_insert, grpentry_ptr);
    insert_srcmrtlink(r_new, r_src_insert, srcentry_ptr);
    r_new->flags |= MRTF_SG;
    return (r_new);
}

/* ======================== */
/* filter related functions */
struct mrtfilter *filterlist;

/*
 * Search for a filter entry in the filter list.
 */
struct mrtfilter *
search_filter(maddr)
	struct in6_addr *maddr;
{
	struct mrtfilter *f;
	struct sockaddr_in6 msa6;

	for (f = filterlist; f; f = f->next) {
		switch(f->type) {
		 case FILTER_RANGE:
			 msa6.sin6_scope_id = 0; /* XXX: scope consideration */
			 msa6.sin6_addr = *maddr;
			 if (inet6_greateroreq(&msa6, &f->mrtf_from) &&
			     inet6_lessoreq(&msa6, &f->mrtf_to))
				 return(f);
			 break;
		 case FILTER_PREFIX:
			 msa6.sin6_scope_id = 0; /* XXX: scope consideration */
			 if (inet6_match_prefix(&msa6, &f->mrtf_prefix,
						&f->mrtf_mask))
				 return(f);
			 break;
		}
	}

	return(NULL);
}

/*
 * Make a new filter entry.
 * This function assumes 
 */
struct mrtfilter *
add_filter(type, maddr1, maddr2, plen)
	struct in6_addr *maddr1, *maddr2;
	int type, plen;
{
	struct mrtfilter *f;
	struct sockaddr_in6 from, to;

	if ((f = malloc(sizeof(*f))) == NULL)
		log(LOG_ERR, 0, "add_filter: malloc failed"); /* assert */
	memset((void *)f, 0, sizeof(*f));

	f->type = type;
	switch(type) {
	 case FILTER_RANGE:
		 memset((void *)&from, 0, sizeof(from));
		 memset((void *)&to, 0, sizeof(to));
		 from.sin6_addr = *maddr1;
		 to.sin6_addr = *maddr2;
		 if (inet6_lessthan(&from, &to)) {
			 f->mrtf_from = from;
			 f->mrtf_to = to;
		 }
		 else {
			 f->mrtf_from = to;
			 f->mrtf_to = from;
		 }
		 break;
	 case FILTER_PREFIX:
		 f->mrtf_prefix.sin6_addr = *maddr1;
		 MASKLEN_TO_MASK6(plen, f->mrtf_mask);
		 break;
	}
	f->next = filterlist;
	filterlist = f;

	return(f);
}

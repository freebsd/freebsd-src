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
 * $FreeBSD: src/usr.sbin/pim6sd/mrt.c,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */

#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include "mrt.h"
#include "vif.h"
#include "rp.h"
#include "pimd.h"
#include "debug.h"
#include "mld6.h"
#include "inet6.h"
#include "timer.h"
#include "route.h"
#include "kern.h"

srcentry_t     *srclist;
grpentry_t     *grplist;

/*
 * Local functions definition
 */
static srcentry_t *create_srcentry 	__P((struct sockaddr_in6 *source));
static int search_srclist 			__P((struct sockaddr_in6 *source ,	
     								srcentry_t ** sourceEntry));

static int search_srcmrtlink 		__P((srcentry_t * srcentry_ptr,
				                  	struct sockaddr_in6 *group,
				                    mrtentry_t ** mrtPtr));

static void insert_srcmrtlink 		__P((mrtentry_t * elementPtr,
				                    mrtentry_t * insertPtr,
				                  	srcentry_t * srcListPtr));

static grpentry_t *create_grpentry  __P((struct sockaddr_in6 *group));

static int search_grplist 			__P((struct sockaddr_in6 *group,
				                 		grpentry_t ** groupEntry));

static int search_grpmrtlink 		__P((grpentry_t * grpentry_ptr,
				                      	 struct sockaddr_in6 *source,
				                      	 mrtentry_t ** mrtPtr));

static void insert_grpmrtlink 		__P((mrtentry_t * elementPtr,
				                     	 mrtentry_t * insertPtr,
				                  		 grpentry_t * grpListPtr));

static mrtentry_t *alloc_mrtentry 	__P((srcentry_t * srcentry_ptr,
				                		grpentry_t * grpentry_ptr));

static mrtentry_t *create_mrtentry 	__P((srcentry_t * srcentry_ptr,
				                  		grpentry_t * grpentry_ptr,
					                    u_int16 flags));

static void move_kernel_cache 		__P((mrtentry_t * mrtentry_ptr,
				                        u_int16 flags));

void
init_pim6_mrt()
{

    /* TODO: delete any existing routing table */

    /* Initialize the source list */
    /* The first entry has address 'IN6ADDR_ANY' and is not used */
    /* The order is the smallest address first. */

    srclist = (srcentry_t *) malloc(sizeof(srcentry_t));
    srclist->next = (srcentry_t *) NULL;
    srclist->prev = (srcentry_t *) NULL;
	memset(&srclist->address, 0, sizeof(struct sockaddr_in6));
   	srclist->address.sin6_len = sizeof(struct sockaddr_in6);
	srclist->address.sin6_family = AF_INET6;
	srclist->mrtlink = (mrtentry_t *) NULL;
    srclist->incoming = NO_VIF;
    srclist->upstream = (pim_nbr_entry_t *) NULL;
    srclist->metric = 0;
    srclist->preference = 0;
    RESET_TIMER(srclist->timer);
    srclist->cand_rp = (cand_rp_t *) NULL;

    /* Initialize the group list */
    /* The first entry has address 'IN6ADDR_ANY' and is not used */
    /* The order is the smallest address first. */

    grplist = (grpentry_t *) malloc(sizeof(grpentry_t));
    grplist->next = (grpentry_t *) NULL;
    grplist->prev = (grpentry_t *) NULL;
    grplist->rpnext = (grpentry_t *) NULL;
    grplist->rpprev = (grpentry_t *) NULL;
    memset(&grplist->group, 0, sizeof(struct sockaddr_in6));
    grplist->group.sin6_len = sizeof(struct sockaddr_in6);
    grplist->group.sin6_family = AF_INET6;
    memset(&grplist->rpaddr, 0, sizeof(struct sockaddr_in6));
    grplist->rpaddr.sin6_len = sizeof(struct sockaddr_in6);
    grplist->rpaddr.sin6_family = AF_INET6;
    grplist->mrtlink = (mrtentry_t *) NULL;
    grplist->active_rp_grp = (rp_grp_entry_t *) NULL;
    grplist->grp_route = (mrtentry_t *) NULL;
}


grpentry_t     *
find_group(group)
    struct sockaddr_in6		*group;
{
    grpentry_t     *grpentry_ptr;

	if (!IN6_IS_ADDR_MULTICAST(&group->sin6_addr))
	return (grpentry_t *) NULL;

    if (search_grplist(group, &grpentry_ptr) == TRUE)
    {
	/* Group found! */
	return (grpentry_ptr);
    }
    return (grpentry_t *) NULL;
}


srcentry_t     *
find_source(source)
    struct sockaddr_in6		*source;
{
    srcentry_t     *srcentry_ptr;

    if (!inet6_valid_host(source))
	return (srcentry_t *) NULL;

    if (search_srclist(source, &srcentry_ptr) == TRUE)
    {
	/* Source found! */
	return (srcentry_ptr);
    }
    return (srcentry_t *) NULL;
}


mrtentry_t     *
find_route(source, group, flags, create)
    struct sockaddr_in6		*source,
                    		*group;
    u_int16         		flags;
    char            		create;
{
    srcentry_t     *srcentry_ptr;
    grpentry_t     *grpentry_ptr;
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_ptr_wc;
    mrtentry_t     *mrtentry_ptr_pmbr;
    mrtentry_t     *mrtentry_ptr_2;
    rpentry_t      *rpentry_ptr=NULL;
    rp_grp_entry_t *rp_grp_entry_ptr;

    if (flags & (MRTF_SG | MRTF_WC))
    {
	if (!IN6_IS_ADDR_MULTICAST(&group->sin6_addr))
	    return (mrtentry_t *) NULL;
    }

    if (flags & MRTF_SG)
	if (!inet6_valid_host(source))
	    return (mrtentry_t *) NULL;

    if (create == DONT_CREATE)
    {
	if (flags & (MRTF_SG | MRTF_WC))
	{
	    if (search_grplist(group, &grpentry_ptr) == FALSE)
	    {
		/* Group not found. Return the (*,*,RP) entry */
		if (flags & MRTF_PMBR)
		{
		    rpentry_ptr = rp_match(group);
		    if (rpentry_ptr != (rpentry_t *) NULL)
			return (rpentry_ptr->mrtlink);
		}
		return (mrtentry_t *) NULL;
	    }
	    /* Search for the source */
	    if (flags & MRTF_SG)
	    {
		if (search_grpmrtlink(grpentry_ptr, source,
				      &mrtentry_ptr) == TRUE)
		{
		    /* Exact (S,G) entry found */
		    return (mrtentry_ptr);
		}
	    }
	    /* No (S,G) entry. Return the (*,G) entry (if exist) */
	    if ((flags & MRTF_WC) &&
		(grpentry_ptr->grp_route != (mrtentry_t *) NULL))
		return (grpentry_ptr->grp_route);
	}

	/* Return the (*,*,RP) entry */

	if (flags & MRTF_PMBR)
	{
	    rpentry_ptr = (rpentry_t *) NULL;
	    if (group != NULL)
		rpentry_ptr = rp_match(group);
	    else
		if (source != NULL)
		    rpentry_ptr = rp_find(source);
	    if (rpentry_ptr != (rpentry_t *) NULL)
		return (rpentry_ptr->mrtlink);
	}
	return (mrtentry_t *) NULL;
    }


    /* Creation allowed */

    if (flags & (MRTF_SG | MRTF_WC))
    {

	grpentry_ptr = create_grpentry(group);
	if (grpentry_ptr == (grpentry_t *) NULL)
	{
	    return (mrtentry_t *) NULL;
	}

	if (grpentry_ptr->active_rp_grp == (rp_grp_entry_t *) NULL)
	{
	    rp_grp_entry_ptr = rp_grp_match(group);

	    if (rp_grp_entry_ptr == (rp_grp_entry_t *) NULL)
	    {
		if ((grpentry_ptr->mrtlink == (mrtentry_t *) NULL)
		    && (grpentry_ptr->grp_route == (mrtentry_t *) NULL))
		{
		    /* New created grpentry. Delete it. */

		    delete_grpentry(grpentry_ptr);
		}

		return (mrtentry_t *) NULL;
	    }

	    rpentry_ptr = rp_grp_entry_ptr->rp->rpentry;
	    grpentry_ptr->active_rp_grp = rp_grp_entry_ptr;
	    grpentry_ptr->rpaddr = rpentry_ptr->address;

	    /* Link to the top of the rp_grp_chain */

	    grpentry_ptr->rpnext = rp_grp_entry_ptr->grplink;
	    rp_grp_entry_ptr->grplink = grpentry_ptr;
	    if (grpentry_ptr->rpnext != (grpentry_t *) NULL)
		grpentry_ptr->rpnext->rpprev = grpentry_ptr;
	}
	else
	    rpentry_ptr = grpentry_ptr->active_rp_grp->rp->rpentry;
    }

    mrtentry_ptr_wc = mrtentry_ptr_pmbr = (mrtentry_t *) NULL;

    if (flags & MRTF_WC)
    {
	/* Setup the (*,G) routing entry */

	mrtentry_ptr_wc = create_mrtentry((srcentry_t *) NULL, grpentry_ptr,
					  MRTF_WC);

	if (mrtentry_ptr_wc == (mrtentry_t *) NULL)
	{
	    if (grpentry_ptr->mrtlink == (mrtentry_t *) NULL)
	    {
		/* New created grpentry. Delete it. */

		delete_grpentry(grpentry_ptr);
	    }
	    return (mrtentry_t *) NULL;
	}

	if (mrtentry_ptr_wc->flags & MRTF_NEW)
	{
	    mrtentry_ptr_pmbr = rpentry_ptr->mrtlink;

	    /* Copy the oif list from the (*,*,RP) entry */

	    if (mrtentry_ptr_pmbr != (mrtentry_t *) NULL)
	    {
		VOIF_COPY(mrtentry_ptr_pmbr, mrtentry_ptr_wc);
	    }

	    mrtentry_ptr_wc->incoming = rpentry_ptr->incoming;
	    mrtentry_ptr_wc->upstream = rpentry_ptr->upstream;
	    mrtentry_ptr_wc->metric = rpentry_ptr->metric;
	    mrtentry_ptr_wc->preference = rpentry_ptr->preference;
	    move_kernel_cache(mrtentry_ptr_wc, 0);

#ifdef RSRR
	    rsrr_cache_bring_up(mrtentry_ptr_wc);
#endif				/* RSRR */

	}

	if (!(flags & MRTF_SG))
	{
	    return (mrtentry_ptr_wc);
	}
    }

    if (flags & MRTF_SG)
    {
	/* Setup the (S,G) routing entry */
	srcentry_ptr = create_srcentry(source);
	if (srcentry_ptr == (srcentry_t *) NULL)
	{
	    /* TODO: XXX: The MRTF_NEW flag check may be misleading?? check */

	    if (((grpentry_ptr->grp_route == (mrtentry_t *) NULL)
		 || ((grpentry_ptr->grp_route != (mrtentry_t *) NULL)
		     && (grpentry_ptr->grp_route->flags & MRTF_NEW)))
		&& (grpentry_ptr->mrtlink == (mrtentry_t *) NULL))
	    {
		/* New created grpentry. Delete it. */
		delete_grpentry(grpentry_ptr);
	    }
	    return (mrtentry_t *) NULL;
	}

	mrtentry_ptr = create_mrtentry(srcentry_ptr, grpentry_ptr, MRTF_SG);
	if (mrtentry_ptr == (mrtentry_t *) NULL)
	{
	    if (((grpentry_ptr->grp_route == (mrtentry_t *) NULL)
		 || ((grpentry_ptr->grp_route != (mrtentry_t *) NULL)
		     && (grpentry_ptr->grp_route->flags & MRTF_NEW)))
		&& (grpentry_ptr->mrtlink == (mrtentry_t *) NULL))
	    {
		/* New created grpentry. Delete it. */
		delete_grpentry(grpentry_ptr);
	    }
	    if (srcentry_ptr->mrtlink == (mrtentry_t *) NULL)
	    {
		/* New created srcentry. Delete it. */
		delete_srcentry(srcentry_ptr);
	    }
	    return (mrtentry_t *) NULL;
	}

	if (mrtentry_ptr->flags & MRTF_NEW)
	{
	    if ((mrtentry_ptr_2 = grpentry_ptr->grp_route)
		== (mrtentry_t *) NULL)
	    {
		mrtentry_ptr_2 = rpentry_ptr->mrtlink;
	    }
	    /* Copy the oif list from the existing (*,G) or (*,*,RP) entry */
	    if (mrtentry_ptr_2 != (mrtentry_t *) NULL)
	    {
		VOIF_COPY(mrtentry_ptr_2, mrtentry_ptr);
		if (flags & MRTF_RP)
		{
		    /* ~(S,G) prune entry */
		    mrtentry_ptr->incoming = mrtentry_ptr_2->incoming;
		    mrtentry_ptr->upstream = mrtentry_ptr_2->upstream;
		    mrtentry_ptr->metric = mrtentry_ptr_2->metric;
		    mrtentry_ptr->preference = mrtentry_ptr_2->preference;
		    mrtentry_ptr->flags |= MRTF_RP;
		}
	    }
	    if (!(mrtentry_ptr->flags & MRTF_RP))
	    {
		mrtentry_ptr->incoming = srcentry_ptr->incoming;
		mrtentry_ptr->upstream = srcentry_ptr->upstream;
		mrtentry_ptr->metric = srcentry_ptr->metric;
		mrtentry_ptr->preference = srcentry_ptr->preference;
	    }
	    move_kernel_cache(mrtentry_ptr, 0);
#ifdef RSRR
	    rsrr_cache_bring_up(mrtentry_ptr);
#endif				/* RSRR */
	}
	return (mrtentry_ptr);
    }

    if (flags & MRTF_PMBR)
    {
	/* Get/return the (*,*,RP) routing entry */

	if (group != NULL)
	    rpentry_ptr = rp_match(group);
	else
	    if (source != NULL)
	    {
		rpentry_ptr = rp_find(source);
		if (rpentry_ptr == (rpentry_t *) NULL)
		{
		    return (mrtentry_t *) NULL;
		}
	    }
	    else
		return (mrtentry_t *) NULL;	/* source == group ==
						 * IN6ADDR_ANY */

	if (rpentry_ptr->mrtlink != (mrtentry_t *) NULL)
	    return (rpentry_ptr->mrtlink);
	mrtentry_ptr = create_mrtentry(rpentry_ptr, (grpentry_t *) NULL,
				       MRTF_PMBR);
	if (mrtentry_ptr == (mrtentry_t *) NULL)
	    return (mrtentry_t *) NULL;
	mrtentry_ptr->incoming = rpentry_ptr->incoming;
	mrtentry_ptr->upstream = rpentry_ptr->upstream;
	mrtentry_ptr->metric = rpentry_ptr->metric;
	mrtentry_ptr->preference = rpentry_ptr->preference;
	return (mrtentry_ptr);
    }

    return (mrtentry_t *) NULL;
}


void
delete_srcentry(srcentry_ptr)
    srcentry_t     *srcentry_ptr;
{
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_next;

    if (srcentry_ptr == (srcentry_t *) NULL)
	return;

    /* TODO: XXX: the first entry is unused and always there */

    srcentry_ptr->prev->next = srcentry_ptr->next;
    if (srcentry_ptr->next != (srcentry_t *) NULL)
	srcentry_ptr->next->prev = srcentry_ptr->prev;

    for (mrtentry_ptr = srcentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *) NULL;
	 mrtentry_ptr = mrtentry_next)
    {
	mrtentry_next = mrtentry_ptr->srcnext;
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	    /* Delete the kernel cache first */
	    delete_mrtentry_all_kernel_cache(mrtentry_ptr);
	if (mrtentry_ptr->grpprev != (mrtentry_t *) NULL)
	    mrtentry_ptr->grpprev->grpnext = mrtentry_ptr->grpnext;
	else
	{
	    mrtentry_ptr->group->mrtlink = mrtentry_ptr->grpnext;
	    if ((mrtentry_ptr->grpnext == (mrtentry_t *) NULL)
		&& (mrtentry_ptr->group->grp_route == (mrtentry_t *) NULL))
	    {
		/* Delete the group entry if it has no (*,G) routing entry */
		delete_grpentry(mrtentry_ptr->group);
	    }
	}
	if (mrtentry_ptr->grpnext != (mrtentry_t *) NULL)
	    mrtentry_ptr->grpnext->grpprev = mrtentry_ptr->grpprev;
	FREE_MRTENTRY(mrtentry_ptr);
    }
    free((char *) srcentry_ptr);
}


void
delete_grpentry(grpentry_ptr)
    grpentry_t     *grpentry_ptr;
{
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_next;

    if (grpentry_ptr == (grpentry_t *) NULL)
	return;

    /* TODO: XXX: the first entry is unused and always there */

    grpentry_ptr->prev->next = grpentry_ptr->next;
    if (grpentry_ptr->next != (grpentry_t *) NULL)
	grpentry_ptr->next->prev = grpentry_ptr->prev;

    if (grpentry_ptr->grp_route != (mrtentry_t *) NULL)
    {
	if (grpentry_ptr->grp_route->flags & MRTF_KERNEL_CACHE)
	    delete_mrtentry_all_kernel_cache(grpentry_ptr->grp_route);
	FREE_MRTENTRY(grpentry_ptr->grp_route);
    }

    /* Delete from the rp_grp_entry chain */
    if (grpentry_ptr->active_rp_grp != (rp_grp_entry_t *) NULL)
    {
	if (grpentry_ptr->rpnext != (grpentry_t *) NULL)
	    grpentry_ptr->rpnext->rpprev = grpentry_ptr->rpprev;
	if (grpentry_ptr->rpprev != (grpentry_t *) NULL)
	    grpentry_ptr->rpprev->rpnext = grpentry_ptr->rpnext;
	else
	    grpentry_ptr->active_rp_grp->grplink = grpentry_ptr->rpnext;
    }

    for (mrtentry_ptr = grpentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *) NULL;
	 mrtentry_ptr = mrtentry_next)
    {
	mrtentry_next = mrtentry_ptr->grpnext;
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	    /* Delete the kernel cache first */
	    delete_mrtentry_all_kernel_cache(mrtentry_ptr);
	if (mrtentry_ptr->srcprev != (mrtentry_t *) NULL)
	    mrtentry_ptr->srcprev->srcnext = mrtentry_ptr->srcnext;
	else
	{
	    mrtentry_ptr->source->mrtlink = mrtentry_ptr->srcnext;
	    if (mrtentry_ptr->srcnext == (mrtentry_t *) NULL)
	    {
		/* Delete the srcentry if this was the last routing entry */
		delete_srcentry(mrtentry_ptr->source);
	    }
	}
	if (mrtentry_ptr->srcnext != (mrtentry_t *) NULL)
	    mrtentry_ptr->srcnext->srcprev = mrtentry_ptr->srcprev;
	FREE_MRTENTRY(mrtentry_ptr);
    }
    free((char *) grpentry_ptr);
}


void
delete_mrtentry(mrtentry_ptr)
    mrtentry_t     *mrtentry_ptr;
{
    grpentry_t     *grpentry_ptr;
    mrtentry_t     *mrtentry_wc;
    mrtentry_t     *mrtentry_rp;

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return;

    /* Delete the kernel cache first */
    if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	delete_mrtentry_all_kernel_cache(mrtentry_ptr);

#ifdef RSRR
    /* Tell the reservation daemon */
    rsrr_cache_clean(mrtentry_ptr);
#endif				/* RSRR */

    if (mrtentry_ptr->flags & MRTF_PMBR)
    {
	/* (*,*,RP) mrtentry */
	mrtentry_ptr->source->mrtlink = (mrtentry_t *) NULL;
    }
    else
	if (mrtentry_ptr->flags & MRTF_SG)
	{
	    /* (S,G) mrtentry */
	    /* Delete from the grpentry MRT chain */
	    if (mrtentry_ptr->grpprev != (mrtentry_t *) NULL)
		mrtentry_ptr->grpprev->grpnext = mrtentry_ptr->grpnext;
	    else
	    {
		mrtentry_ptr->group->mrtlink = mrtentry_ptr->grpnext;
		if (mrtentry_ptr->grpnext == (mrtentry_t *) NULL)
		{
		    /*
		     * All (S,G) MRT entries are gone. Allow creating (*,G)
		     * MFC entries.
		     */
		    mrtentry_rp
			= mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
		    mrtentry_wc = mrtentry_ptr->group->grp_route;
		    if (mrtentry_rp != (mrtentry_t *) NULL)
			mrtentry_rp->flags &= ~MRTF_MFC_CLONE_SG;
		    if (mrtentry_wc != (mrtentry_t *) NULL)
			mrtentry_wc->flags &= ~MRTF_MFC_CLONE_SG;
		    else
		    {
			/*
			 * Delete the group entry if it has no (*,G) routing
			 * entry
			 */
			delete_grpentry(mrtentry_ptr->group);
		    }
		}
	    }
	    if (mrtentry_ptr->grpnext != (mrtentry_t *) NULL)
		mrtentry_ptr->grpnext->grpprev = mrtentry_ptr->grpprev;

	    /* Delete from the srcentry MRT chain */
	    if (mrtentry_ptr->srcprev != (mrtentry_t *) NULL)
		mrtentry_ptr->srcprev->srcnext = mrtentry_ptr->srcnext;
	    else
	    {
		mrtentry_ptr->source->mrtlink = mrtentry_ptr->srcnext;
		if (mrtentry_ptr->srcnext == (mrtentry_t *) NULL)
		{
		    /* Delete the srcentry if this was the last routing entry */
		    delete_srcentry(mrtentry_ptr->source);
		}
	    }
	    if (mrtentry_ptr->srcnext != (mrtentry_t *) NULL)
		mrtentry_ptr->srcnext->srcprev = mrtentry_ptr->srcprev;
	}
	else
	{
	    /* This mrtentry should be (*,G) */
	    grpentry_ptr = mrtentry_ptr->group;
	    grpentry_ptr->grp_route = (mrtentry_t *) NULL;

	    if (grpentry_ptr->mrtlink == (mrtentry_t *) NULL)
		/* Delete the group entry if it has no (S,G) entries */
		delete_grpentry(grpentry_ptr);
	}

    FREE_MRTENTRY(mrtentry_ptr);
}


static int
search_srclist(source, sourceEntry)
    struct sockaddr_in6         *source;
    register srcentry_t 	**sourceEntry;
{
    register srcentry_t 	*s_prev,
                   		*s;

    for (s_prev = srclist, s = s_prev->next; s != (srcentry_t *) NULL;
	 s_prev = s, s = s->next)
    {
	/*
	 * The srclist is ordered with the smallest addresses first. The
	 * first entry is not used.
	 */
	if (inet6_lessthan(&s->address, source))
	    continue;
	if (inet6_equal(&s->address, source))
	{
	    *sourceEntry = s;
	    return (TRUE);
	}
	break;
    }
    *sourceEntry = s_prev;	/* The insertion point is between s_prev and
				 * s */
    return (FALSE);
}


static int
search_grplist(group, groupEntry)
    struct sockaddr_in6      	*group;
    register grpentry_t 	**groupEntry;
{
    register grpentry_t *g_prev,
                   *g;

    for (g_prev = grplist, g = g_prev->next; g != (grpentry_t *) NULL;
	 g_prev = g, g = g->next)
    {
	/*
	 * The grplist is ordered with the smallest address first. The first
	 * entry is not used.
	 */

	if (inet6_lessthan(&g->group, group))
	    continue;
	if (inet6_equal(&g->group, group))	
	{
	    *groupEntry = g;
	    return (TRUE);
	}
	break;
    }
    *groupEntry = g_prev;	/* The insertion point is between g_prev and
				 			* g */
    return (FALSE);
}


static srcentry_t *
create_srcentry(source)
    struct sockaddr_in6		*source;
{
    register srcentry_t *srcentry_ptr;
    srcentry_t     *srcentry_prev;

    if (search_srclist(source, &srcentry_prev) == TRUE)
	return (srcentry_prev);

    srcentry_ptr = (srcentry_t *) malloc(sizeof(srcentry_t));
    if (srcentry_ptr == (srcentry_t *) NULL)
    {
	log(LOG_WARNING, 0, "Memory allocation error for srcentry %s",
	    inet6_fmt(&source->sin6_addr));
	return (srcentry_t *) NULL;
    }

    srcentry_ptr->address = *source;
    /*
     * Free the memory if there is error getting the iif and the next hop
     * (upstream) router.
     */
  
     if (set_incoming(srcentry_ptr, PIM_IIF_SOURCE) == FALSE)
    {
	free((char *) srcentry_ptr);
	return (srcentry_t *) NULL;
    }
    srcentry_ptr->mrtlink = (mrtentry_t *) NULL;
    RESET_TIMER(srcentry_ptr->timer);
    srcentry_ptr->cand_rp = (cand_rp_t *) NULL;
    srcentry_ptr->next = srcentry_prev->next;
    srcentry_prev->next = srcentry_ptr;
    srcentry_ptr->prev = srcentry_prev;
    if (srcentry_ptr->next != (srcentry_t *) NULL)
	srcentry_ptr->next->prev = srcentry_ptr;

    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "create source entry, source %s",
	    inet6_fmt(&source->sin6_addr));
    return (srcentry_ptr);
}


static grpentry_t *
create_grpentry(group)
    struct sockaddr_in6		*group;
{
    register grpentry_t *grpentry_ptr;
    grpentry_t     *grpentry_prev;

    if (search_grplist(group, &grpentry_prev) == TRUE)
	return (grpentry_prev);

    grpentry_ptr = (grpentry_t *) malloc(sizeof(grpentry_t));

    if (grpentry_ptr == (grpentry_t *) NULL)
    {
	log(LOG_WARNING, 0, "Memory allocation error for grpentry %s",
	    inet6_fmt(&group->sin6_addr));
	return (grpentry_t *) NULL;
    }

    /*
     * TODO: XXX: Note that this is NOT a (*,G) routing entry, but simply a
     * group entry, probably used to search the routing table (to find (S,G)
     * entries for example.) To become (*,G) routing entry, we must setup
     * grpentry_ptr->grp_route
     */

    grpentry_ptr->group = *group;
    memset(&grpentry_ptr->rpaddr, 0, sizeof(struct sockaddr_in6));
    grpentry_ptr->rpaddr.sin6_len = sizeof(struct sockaddr_in6);
    grpentry_ptr->rpaddr.sin6_family = AF_INET6;
    grpentry_ptr->mrtlink = (mrtentry_t *) NULL;
    grpentry_ptr->active_rp_grp = (rp_grp_entry_t *) NULL;
    grpentry_ptr->grp_route = (mrtentry_t *) NULL;
    grpentry_ptr->rpnext = (grpentry_t *) NULL;
    grpentry_ptr->rpprev = (grpentry_t *) NULL;

    /* Now it is safe to include the new group entry */

    grpentry_ptr->next = grpentry_prev->next;
    grpentry_prev->next = grpentry_ptr;
    grpentry_ptr->prev = grpentry_prev;
    if (grpentry_ptr->next != (grpentry_t *) NULL)
	grpentry_ptr->next->prev = grpentry_ptr;

    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "create group entry, group %s", inet6_fmt(&group->sin6_addr));
    return (grpentry_ptr);
}


/*
 * Return TRUE if the entry is found and then *mrtPtr is set to point to that
 * entry. Otherwise return FALSE and *mrtPtr points the previous entry
 * (or NULL if first in the chain.
 */
static int
search_srcmrtlink(srcentry_ptr, group, mrtPtr)
    srcentry_t     		*srcentry_ptr;
    struct sockaddr_in6		*group;
    mrtentry_t    		**mrtPtr;
{
    register mrtentry_t *mrtentry_ptr;
    register mrtentry_t *m_prev = (mrtentry_t *) NULL;

    for (mrtentry_ptr = srcentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *) NULL;
	 m_prev = mrtentry_ptr, mrtentry_ptr = mrtentry_ptr->srcnext)
    {
	/*
	 * The entries are ordered with the smaller group address first. The
	 * addresses are in network order.
	 */
	
	if (inet6_lessthan(&mrtentry_ptr->group->group, group))
	    continue;
	if (inet6_equal(&mrtentry_ptr->group->group, group))
	{
	    *mrtPtr = mrtentry_ptr;
	    return (TRUE);
	}
	break;
    }
    *mrtPtr = m_prev;
    return (FALSE);
}


/*
 * Return TRUE if the entry is found and then *mrtPtr is set to point to that
 * entry. Otherwise return FALSE and *mrtPtr points the previous entry
 * (or NULL if first in the chain.
 */
static int
search_grpmrtlink(grpentry_ptr, source, mrtPtr)
    grpentry_t     		*grpentry_ptr;
    struct sockaddr_in6		*source;
    mrtentry_t    		**mrtPtr;
{
    register mrtentry_t *mrtentry_ptr;
    register mrtentry_t *m_prev = (mrtentry_t *) NULL;

    for (mrtentry_ptr = grpentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *) NULL;
	 m_prev = mrtentry_ptr, mrtentry_ptr = mrtentry_ptr->grpnext)
    {
	/*
	 * The entries are ordered with the smaller source address first. The
	 * addresses are in network order.
	 */

	if (inet6_lessthan(&mrtentry_ptr->source->address, source))
	    continue;


	if (inet6_equal(source, &mrtentry_ptr->source->address))
	{
	    *mrtPtr = mrtentry_ptr;
	    return (TRUE);
	}
	break;
    }
    *mrtPtr = m_prev;
    return (FALSE);
}


static void
insert_srcmrtlink(mrtentry_new, mrtentry_prev, srcentry_ptr)
    mrtentry_t     *mrtentry_new;
    mrtentry_t     *mrtentry_prev;
    srcentry_t     *srcentry_ptr;
{
    if (mrtentry_prev == (mrtentry_t *) NULL)
    {
	/* Has to be insert as the head entry for this source */

	mrtentry_new->srcnext = srcentry_ptr->mrtlink;
	mrtentry_new->srcprev = (mrtentry_t *) NULL;
	srcentry_ptr->mrtlink = mrtentry_new;
    }
    else
    {
	/* Insert right after the mrtentry_prev */

	mrtentry_new->srcnext = mrtentry_prev->srcnext;
	mrtentry_new->srcprev = mrtentry_prev;
	mrtentry_prev->srcnext = mrtentry_new;
    }
    if (mrtentry_new->srcnext != (mrtentry_t *) NULL)
	mrtentry_new->srcnext->srcprev = mrtentry_new;
}


static void
insert_grpmrtlink(mrtentry_new, mrtentry_prev, grpentry_ptr)
    mrtentry_t     *mrtentry_new;
    mrtentry_t     *mrtentry_prev;
    grpentry_t     *grpentry_ptr;
{
    if (mrtentry_prev == (mrtentry_t *) NULL)
    {
	/* Has to be insert as the head entry for this group */

	mrtentry_new->grpnext = grpentry_ptr->mrtlink;
	mrtentry_new->grpprev = (mrtentry_t *) NULL;
	grpentry_ptr->mrtlink = mrtentry_new;
    }
    else
    {
	/* Insert right after the mrtentry_prev */

	mrtentry_new->grpnext = mrtentry_prev->grpnext;
	mrtentry_new->grpprev = mrtentry_prev;
	mrtentry_prev->grpnext = mrtentry_new;
    }
    if (mrtentry_new->grpnext != (mrtentry_t *) NULL)
	mrtentry_new->grpnext->grpprev = mrtentry_new;
}


static mrtentry_t *
alloc_mrtentry(srcentry_ptr, grpentry_ptr)
    srcentry_t     *srcentry_ptr;
    grpentry_t     *grpentry_ptr;
{
    register mrtentry_t *mrtentry_ptr;
    u_int16         	i,
                   	*i_ptr;
    u_int8          	vif_numbers;

    mrtentry_ptr = (mrtentry_t *) malloc(sizeof(mrtentry_t));
    if (mrtentry_ptr == (mrtentry_t *) NULL)
    {
	log(LOG_WARNING, 0, "alloc_mrtentry(): out of memory");
	return (mrtentry_t *) NULL;
    }

    /*
     * grpnext, grpprev, srcnext, srcprev will be setup when we link the
     * mrtentry to the source and group chains
     */
    mrtentry_ptr->source = srcentry_ptr;
    mrtentry_ptr->group = grpentry_ptr;
    mrtentry_ptr->incoming = NO_VIF;
    IF_ZERO(&mrtentry_ptr->joined_oifs);
    IF_ZERO(&mrtentry_ptr->leaves);
    IF_ZERO(&mrtentry_ptr->pruned_oifs);
    IF_ZERO(&mrtentry_ptr->asserted_oifs);
    IF_ZERO(&mrtentry_ptr->oifs);
    mrtentry_ptr->upstream = (pim_nbr_entry_t *) NULL;
    mrtentry_ptr->metric = 0;
    mrtentry_ptr->preference = 0;
    mrtentry_ptr->pmbr_addr.sin6_addr = in6addr_any;
    mrtentry_ptr->pmbr_addr.sin6_len = sizeof(struct sockaddr_in6);
    mrtentry_ptr->pmbr_addr.sin6_family = AF_INET6;
	
#ifdef RSRR
    mrtentry_ptr->rsrr_cache = (struct rsrr_cache *) NULL;
#endif				/* RSRR */

    /*
     * XXX: TODO: if we are short in memory, we can reserve as few as
     * possible space for vif timers (per group and/or routing entry), but
     * then everytime when a new interfaces is configured, the router will be
     * restarted and will delete the whole routing table. The "memory is
     * cheap" solution is to reserve timer space for all potential vifs in
     * advance and then no need to delete the routing table and disturb the
     * forwarding.
     */

#ifdef SAVE_MEMORY
    mrtentry_ptr->vif_timers = (u_int16 *) malloc(sizeof(u_int16) * numvifs);
    mrtentry_ptr->vif_deletion_delay =
	(u_int16 *) malloc(sizeof(u_int16) * numvifs);
    vif_numbers = numvifs;
#else
    mrtentry_ptr->vif_timers =
	(u_int16 *) malloc(sizeof(u_int16) * total_interfaces);
    mrtentry_ptr->vif_deletion_delay =
	(u_int16 *) malloc(sizeof(u_int16) * total_interfaces);
    vif_numbers = total_interfaces;
#endif				/* SAVE_MEMORY */
    if ((mrtentry_ptr->vif_timers == (u_int16 *) NULL) ||
	(mrtentry_ptr->vif_deletion_delay == (u_int16 *) NULL))
    {
	log(LOG_WARNING, 0, "alloc_mrtentry(): out of memory");
	FREE_MRTENTRY(mrtentry_ptr);
	return (mrtentry_t *) NULL;
    }
    /* Reset the timers */
    for (i = 0, i_ptr = mrtentry_ptr->vif_timers; i < vif_numbers;
	 i++, i_ptr++)
	RESET_TIMER(*i_ptr);
    for (i = 0, i_ptr = mrtentry_ptr->vif_deletion_delay; i < vif_numbers;
	 i++, i_ptr++)
	RESET_TIMER(*i_ptr);

    mrtentry_ptr->flags = MRTF_NEW;
    RESET_TIMER(mrtentry_ptr->timer);
    RESET_TIMER(mrtentry_ptr->jp_timer);
    RESET_TIMER(mrtentry_ptr->rs_timer);
    RESET_TIMER(mrtentry_ptr->assert_timer);
    RESET_TIMER(mrtentry_ptr->assert_rate_timer);
    mrtentry_ptr->kernel_cache = (kernel_cache_t *) NULL;

    return (mrtentry_ptr);
}


static mrtentry_t *
create_mrtentry(srcentry_ptr, grpentry_ptr, flags)
    srcentry_t     		*srcentry_ptr;
    grpentry_t     		*grpentry_ptr;
    u_int16         		flags;
{
    mrtentry_t     		*r_new;
    mrtentry_t     		*r_grp_insert,
                   		*r_src_insert;	/* pointers to insert */
    struct sockaddr_in6		*source;
    struct sockaddr_in6		*group;


    if (flags & MRTF_SG)
    {
	/* (S,G) entry */

	source = &srcentry_ptr->address;
	group = &grpentry_ptr->group;

	if (search_grpmrtlink(grpentry_ptr, source, &r_grp_insert) == TRUE)
	{
	    return (r_grp_insert);
	}
	if (search_srcmrtlink(srcentry_ptr, group, &r_src_insert) == TRUE)
	{
	    /*
	     * Hmmm, search_grpmrtlink() didn't find the entry, but
	     * search_srcmrtlink() did find it! Shoudn't happen. Panic!
	     */

	    log(LOG_ERR, 0, "MRT inconsistency for src %s and grp %s\n",
		inet6_fmt(&source->sin6_addr), inet6_fmt(&group->sin6_addr));
	    /* not reached but to make lint happy */
	    return (mrtentry_t *) NULL;
	}
	/*
	 * Create and insert in group mrtlink and source mrtlink chains.
	 */
	r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
	if (r_new == (mrtentry_t *) NULL)
	    return (mrtentry_t *) NULL;
	/*
	 * r_new has to be insert right after r_grp_insert in the grp mrtlink
	 * chain and right after r_src_insert in the src mrtlink chain
	 */
	insert_grpmrtlink(r_new, r_grp_insert, grpentry_ptr);
	insert_srcmrtlink(r_new, r_src_insert, srcentry_ptr);
	r_new->flags |= MRTF_SG;

	IF_DEBUG(DEBUG_MFC)
	    log(LOG_DEBUG, 0, "create SG entry, source %s, group %s",
		inet6_fmt(&source->sin6_addr),
		inet6_fmt(&group->sin6_addr));

	return (r_new);
    }

    if (flags & MRTF_WC)
    {
	/* (*,G) entry */

	if (grpentry_ptr->grp_route != (mrtentry_t *) NULL)
	    return (grpentry_ptr->grp_route);
	r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
	if (r_new == (mrtentry_t *) NULL)
	    return (mrtentry_t *) NULL;
	grpentry_ptr->grp_route = r_new;
	r_new->flags |= (MRTF_WC | MRTF_RP);
	return (r_new);
    }

    if (flags & MRTF_PMBR)
    {
	/* (*,*,RP) entry */

	if (srcentry_ptr->mrtlink != (mrtentry_t *) NULL)
	    return (srcentry_ptr->mrtlink);
	r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
	if (r_new == (mrtentry_t *) NULL)
	    return (mrtentry_t *) NULL;
	srcentry_ptr->mrtlink = r_new;
	r_new->flags |= (MRTF_PMBR | MRTF_RP);
	return (r_new);
    }

    return (mrtentry_t *) NULL;
}


/*
 * Delete all kernel cache for this mrtentry
 */
void
delete_mrtentry_all_kernel_cache(mrtentry_ptr)
    mrtentry_t     *mrtentry_ptr;
{
    kernel_cache_t *kernel_cache_prev;
    kernel_cache_t *kernel_cache_ptr;

    if (!(mrtentry_ptr->flags & MRTF_KERNEL_CACHE))
    {
	return;
    }

    /* Free all kernel_cache entries */
    for (kernel_cache_ptr = mrtentry_ptr->kernel_cache;
	 kernel_cache_ptr != (kernel_cache_t *) NULL;)
    {
	kernel_cache_prev = kernel_cache_ptr;
	kernel_cache_ptr = kernel_cache_ptr->next;
	k_del_mfc(mld6_socket, &kernel_cache_prev->source,
		  &kernel_cache_prev->group);
	free((char *) kernel_cache_prev);
    }
    mrtentry_ptr->kernel_cache = (kernel_cache_t *) NULL;

    /* turn off the cache flag(s) */
    mrtentry_ptr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
}


void
delete_single_kernel_cache(mrtentry_ptr, kernel_cache_ptr)
    mrtentry_t     *mrtentry_ptr;
    kernel_cache_t *kernel_cache_ptr;
{
    if (kernel_cache_ptr->prev == (kernel_cache_t *) NULL)
    {
	mrtentry_ptr->kernel_cache = kernel_cache_ptr->next;
	if (mrtentry_ptr->kernel_cache == (kernel_cache_t *) NULL)
	    mrtentry_ptr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
    }
    else
	kernel_cache_ptr->prev->next = kernel_cache_ptr->next;
    if (kernel_cache_ptr->next != (kernel_cache_t *) NULL)
	kernel_cache_ptr->next->prev = kernel_cache_ptr->prev;
    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "Deleting MFC entry for source %s and group %s",
	    inet6_fmt(&kernel_cache_ptr->source.sin6_addr),
	    inet6_fmt(&kernel_cache_ptr->source.sin6_addr));
    k_del_mfc(mld6_socket, &kernel_cache_ptr->source,
	      &kernel_cache_ptr->group);
    free((char *) kernel_cache_ptr);
}


void
delete_single_kernel_cache_addr(mrtentry_ptr, source, group)
    mrtentry_t     	*mrtentry_ptr;
    struct sockaddr_in6 *source;
    struct sockaddr_in6 *group;
{
    kernel_cache_t *kernel_cache_ptr;

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return;


    /* Find the exact (S,G) kernel_cache entry */
    for (kernel_cache_ptr = mrtentry_ptr->kernel_cache;
	 kernel_cache_ptr != (kernel_cache_t *) NULL;
	 kernel_cache_ptr = kernel_cache_ptr->next)
    {
	if (inet6_lessthan(&kernel_cache_ptr->group, group))
	    continue;
	if (inet6_greaterthan(&kernel_cache_ptr->group, group))
	    return;		/* Not found */
	if (inet6_lessthan(&kernel_cache_ptr->source, source))
	    continue;
	if (inet6_greaterthan(&kernel_cache_ptr->source, source))
	    return;		/* Not found */
	/* Found exact match */
	break;
    }

    if (kernel_cache_ptr == (kernel_cache_t *) NULL)
	return;

    /* Found. Delete it */
    if (kernel_cache_ptr->prev == (kernel_cache_t *) NULL)
    {
	mrtentry_ptr->kernel_cache = kernel_cache_ptr->next;
	if (mrtentry_ptr->kernel_cache == (kernel_cache_t *) NULL)
	    mrtentry_ptr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
    }
    else
	kernel_cache_ptr->prev->next = kernel_cache_ptr->next;
    if (kernel_cache_ptr->next != (kernel_cache_t *) NULL)
	kernel_cache_ptr->next->prev = kernel_cache_ptr->prev;
    IF_DEBUG(DEBUG_MFC)
	log(LOG_DEBUG, 0, "Deleting MFC entry for source %s and group %s",
	    inet6_fmt(&kernel_cache_ptr->source.sin6_addr),
	    inet6_fmt(&kernel_cache_ptr->group.sin6_addr));
    k_del_mfc(mld6_socket, &kernel_cache_ptr->source,
	      &kernel_cache_ptr->group);
    free((char *) kernel_cache_ptr);
}


/*
 * Installs kernel cache for (source, group). Assumes mrtentry_ptr is the
 * correct entry.
 */
void
add_kernel_cache(mrtentry_ptr, source, group, flags)
    mrtentry_t     		*mrtentry_ptr;
    struct sockaddr_in6 	*source;
    struct sockaddr_in6 	*group;
    u_int16         		flags;
{
    kernel_cache_t *kernel_cache_next;
    kernel_cache_t *kernel_cache_prev;
    kernel_cache_t *kernel_cache_new;

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return;

    move_kernel_cache(mrtentry_ptr, flags);

    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/* (S,G) */
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	    return;
	kernel_cache_new = (kernel_cache_t *) malloc(sizeof(kernel_cache_t));
	kernel_cache_new->next = (kernel_cache_t *) NULL;
	kernel_cache_new->prev = (kernel_cache_t *) NULL;
	kernel_cache_new->source = *source;
	kernel_cache_new->group = *group;
	kernel_cache_new->sg_count.pktcnt = 0;
	kernel_cache_new->sg_count.bytecnt = 0;
	kernel_cache_new->sg_count.wrong_if = 0;
	mrtentry_ptr->kernel_cache = kernel_cache_new;
	mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;
	return;
    }

    kernel_cache_prev = (kernel_cache_t *) NULL;

    for (kernel_cache_next = mrtentry_ptr->kernel_cache;
	 kernel_cache_next != (kernel_cache_t *) NULL;
	 kernel_cache_prev = kernel_cache_next,
	 kernel_cache_next = kernel_cache_next->next)
    {
	if (inet6_lessthan(&kernel_cache_next->group , group))
	    continue;
	if (inet6_greaterthan(&kernel_cache_next->group , group))
	    break;
	if (inet6_lessthan(&kernel_cache_next->source , source))
	    continue;
	if (inet6_greaterthan(&kernel_cache_next->source , source))
	    break;
	/* Found exact match. Nothing to change. */
	return;
    }

    /*
     * The new entry must be placed between kernel_cache_prev and
     * kernel_cache_next
     */
    kernel_cache_new = (kernel_cache_t *) malloc(sizeof(kernel_cache_t));
    if (kernel_cache_prev != (kernel_cache_t *) NULL)
	kernel_cache_prev->next = kernel_cache_new;
    else
	mrtentry_ptr->kernel_cache = kernel_cache_new;
    if (kernel_cache_next != (kernel_cache_t *) NULL)
	kernel_cache_next->prev = kernel_cache_new;
    kernel_cache_new->prev = kernel_cache_prev;
    kernel_cache_new->next = kernel_cache_next;
    kernel_cache_new->source = *source;
    kernel_cache_new->group = *group;
    kernel_cache_new->sg_count.pktcnt = 0;
    kernel_cache_new->sg_count.bytecnt = 0;
    kernel_cache_new->sg_count.wrong_if = 0;
    mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;
}

/*
 * Bring the kernel cache "UP": from the (*,*,RP) to (*,G) or (S,G)
 */
static void
move_kernel_cache(mrtentry_ptr, flags)
    mrtentry_t     *mrtentry_ptr;
    u_int16         flags;
{
    kernel_cache_t *kernel_cache_ptr;
    kernel_cache_t *insert_kernel_cache_ptr;
    kernel_cache_t *first_kernel_cache_ptr;
    kernel_cache_t *last_kernel_cache_ptr;
    kernel_cache_t *prev_kernel_cache_ptr;
    mrtentry_t     *mrtentry_pmbr;
    mrtentry_t     *mrtentry_rp;
    int             found;

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return;

    if (mrtentry_ptr->flags & MRTF_PMBR)
	return;

    if (mrtentry_ptr->flags & MRTF_WC)
    {
	/* Move the cache info from (*,*,RP) to (*,G) */
	mrtentry_pmbr =
	    mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_pmbr == (mrtentry_t *) NULL)
	    return;		/* Nothing to move */

	first_kernel_cache_ptr = last_kernel_cache_ptr =
	    (kernel_cache_t *) NULL;
	for (kernel_cache_ptr = mrtentry_pmbr->kernel_cache;
	     kernel_cache_ptr != (kernel_cache_t *) NULL;
	     kernel_cache_ptr = kernel_cache_ptr->next)
	{
	    /*
	     * The order is: (1) smaller group; (2) smaller source within
	     * group
	     */
	    if (inet6_lessthan(&kernel_cache_ptr->group, &mrtentry_ptr->group->group))
		continue;
	    if (!inet6_equal(&kernel_cache_ptr->group, &mrtentry_ptr->group->group))
		break;
	    /* Select the kernel_cache entries to move  */
	    if (first_kernel_cache_ptr == (kernel_cache_t *) NULL)
	    {
		first_kernel_cache_ptr = last_kernel_cache_ptr =
		    kernel_cache_ptr;
	    }
	    else
		last_kernel_cache_ptr = kernel_cache_ptr;
	}

	if (first_kernel_cache_ptr != (kernel_cache_t *) NULL)
	{
	    /* Fix the old chain */
	    if (first_kernel_cache_ptr->prev != (kernel_cache_t *) NULL)
	    {
		first_kernel_cache_ptr->prev->next =
		    last_kernel_cache_ptr->next;
	    }
	    else
		mrtentry_pmbr->kernel_cache = last_kernel_cache_ptr->next;
	    if (last_kernel_cache_ptr->next != (kernel_cache_t *) NULL)
		last_kernel_cache_ptr->next->prev =
		    first_kernel_cache_ptr->prev;
	    if (mrtentry_pmbr->kernel_cache == (kernel_cache_t *) NULL)
		mrtentry_pmbr->flags
		    &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);

	    /* Insert in the new place */
	    prev_kernel_cache_ptr = (kernel_cache_t *) NULL;
	    last_kernel_cache_ptr->next = (kernel_cache_t *) NULL;
	    mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;

	    for (kernel_cache_ptr = mrtentry_ptr->kernel_cache;
		 kernel_cache_ptr != (kernel_cache_t *) NULL;)
	    {
		if (first_kernel_cache_ptr == (kernel_cache_t *) NULL)
		    break;	/* All entries have been inserted */
		if (inet6_greaterthan(&kernel_cache_ptr->source,&first_kernel_cache_ptr->source))
		{
		    /* Insert the entry before kernel_cache_ptr */
		    insert_kernel_cache_ptr = first_kernel_cache_ptr;
		    first_kernel_cache_ptr = first_kernel_cache_ptr->next;
		    if (kernel_cache_ptr->prev != (kernel_cache_t *) NULL)
			kernel_cache_ptr->prev->next =
			    insert_kernel_cache_ptr;
		    else
			mrtentry_ptr->kernel_cache =
			    insert_kernel_cache_ptr;
		    insert_kernel_cache_ptr->prev =
			kernel_cache_ptr->prev;
		    insert_kernel_cache_ptr->next = kernel_cache_ptr;
		    kernel_cache_ptr->prev = insert_kernel_cache_ptr;
		}
		prev_kernel_cache_ptr = kernel_cache_ptr;
		kernel_cache_ptr = kernel_cache_ptr->next;
	    }
	    if (first_kernel_cache_ptr != (kernel_cache_t *) NULL)
	    {
		/* Place all at the end after prev_kernel_cache_ptr */
		if (prev_kernel_cache_ptr != (kernel_cache_t *) NULL)
		    prev_kernel_cache_ptr->next = first_kernel_cache_ptr;
		else
		    mrtentry_ptr->kernel_cache = first_kernel_cache_ptr;
		first_kernel_cache_ptr->prev = prev_kernel_cache_ptr;
	    }
	}
	return;
    }

    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/*
	 * (S,G) entry. Move the whole group cache from (*,*,RP) to (*,G) and
	 * then get the necessary entry from (*,G). TODO: Not optimized! The
	 * particular entry is moved first to (*,G), then we have to search
	 * again (*,G) to find it and move to (S,G).
	 */
	/* TODO: XXX: No need for this? Thinking.... */
	/* move_kernel_cache(mrtentry_ptr->group->grp_route, flags); */

	if ((mrtentry_rp = mrtentry_ptr->group->grp_route) ==
	    (mrtentry_t *) NULL)
	    mrtentry_rp =
		mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_rp == (mrtentry_t *) NULL)
	    return;

	if (mrtentry_rp->incoming != mrtentry_ptr->incoming)
	{
	    /*
	     * XXX: the (*,*,RP) (or (*,G)) iif is different from the (S,G)
	     * iif. No need to move the cache, because (S,G) don't need it.
	     * After the first packet arrives on the shortest path, the
	     * correct cache entry will be created. If (flags &
	     * MFC_MOVE_FORCE) then we must move the cache. This usually
	     * happens when switching to the shortest path. The calling
	     * function will immediately call k_chg_mfc() to modify the
	     * kernel cache.
	     */
	    if (!(flags & MFC_MOVE_FORCE))
		return;
	}

	/* Find the exact entry */

	found = FALSE;
	for (kernel_cache_ptr = mrtentry_rp->kernel_cache;
	     kernel_cache_ptr != (kernel_cache_t *) NULL;
	     kernel_cache_ptr = kernel_cache_ptr->next)
	{
	    if (inet6_lessthan(&kernel_cache_ptr->group, &mrtentry_ptr->group->group))
		continue;
	    if (inet6_greaterthan(&kernel_cache_ptr->group, &mrtentry_ptr->group->group))
		break;
	    if (inet6_lessthan(&kernel_cache_ptr->source, &mrtentry_ptr->source->address))
		continue;
	    if (inet6_greaterthan(&kernel_cache_ptr->source, &mrtentry_ptr->source->address))
		break;
	    /* We found it! */
	    if (kernel_cache_ptr->prev != (kernel_cache_t *) NULL)
		kernel_cache_ptr->prev->next = kernel_cache_ptr->next;
	    else
	    {
		mrtentry_rp->kernel_cache = kernel_cache_ptr->next;
	    }
	    if (kernel_cache_ptr->next != (kernel_cache_t *) NULL)
		kernel_cache_ptr->next->prev = kernel_cache_ptr->prev;
	    found = TRUE;
	    break;
	}

	if (found == TRUE)
	{
	    if (mrtentry_rp->kernel_cache == (kernel_cache_t *) NULL)
		mrtentry_rp->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
	    if (mrtentry_ptr->kernel_cache != (kernel_cache_t *) NULL)
		free((char *) mrtentry_ptr->kernel_cache);
	    mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;
	    mrtentry_ptr->kernel_cache = kernel_cache_ptr;
	    kernel_cache_ptr->prev = (kernel_cache_t *) NULL;
	    kernel_cache_ptr->next = (kernel_cache_t *) NULL;
	}
    }
}

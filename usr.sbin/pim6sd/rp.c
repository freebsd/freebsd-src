/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 * $FreeBSD: src/usr.sbin/pim6sd/rp.c,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */

#include <stdlib.h>
#include <syslog.h>
#include "rp.h"
#include "pim6_proto.h"
#include "pimd.h"
#include <netinet6/pim6.h>
#include "timer.h"
#include "inet6.h"
#include "route.h"
#include "pimd.h"
#include "debug.h"
#include "crc.h"

/*
 * The hash function. Stollen from Eddy's (eddy@isi.edu) implementation (for
 * compatibility ;)
 */

#define SEED1   1103515245
#define SEED2   12345
#define RP_HASH_VALUE(G, M, C) (((SEED1) * (((SEED1) * ((G) & (M)) + (SEED2)) ^ (C)) + (SEED2)) % 0x80000000)
#define RP_HASH_VALUE2(P, C) (((SEED1) * (((SEED1) * (P) + (SEED2)) ^ (C)) + (SEED2)) % 0x80000000)

cand_rp_t      			*cand_rp_list;
grp_mask_t     			*grp_mask_list;
cand_rp_t      			*segmented_cand_rp_list;
grp_mask_t     			*segmented_grp_mask_list;
u_int16         		curr_bsr_fragment_tag;
u_int8          		curr_bsr_priority;
struct sockaddr_in6		curr_bsr_address;
struct in6_addr			curr_bsr_hash_mask;
u_int16         		pim_bootstrap_timer;	/* For electing the BSR and sending
					 		 * Cand-RP-set msgs */
u_int8          		my_bsr_priority;
struct sockaddr_in6		my_bsr_address;
struct in6_addr         	my_bsr_hash_mask;
u_int8          		cand_bsr_flag = FALSE;	/* Set to TRUE if I am a candidate
						 	 * BSR */
struct sockaddr_in6     	my_cand_rp_address;
u_int8          		my_cand_rp_priority;
u_int16         		my_cand_rp_holdtime;
u_int16         		my_cand_rp_adv_period;	/* The locally configured Cand-RP
					 		 * adv. period. */
u_int16         		my_bsr_period;		/* The locally configured BSR	
							   period */
u_int16         		pim_cand_rp_adv_timer;
u_int8          		cand_rp_flag = FALSE;	/* Candidate RP flag */
struct cand_rp_adv_message_ 	cand_rp_adv_message;
struct in6_addr         	rp_my_ipv6_hashmask;


/*
 * Local functions definition.
 */
static cand_rp_t *add_cand_rp __P((cand_rp_t **used_cand_rp_list ,
     				   struct sockaddr_in6 *address));

static grp_mask_t *add_grp_mask __P((grp_mask_t ** used_grp_mask_list,
		                     struct sockaddr_in6 *group_addr,
				     struct in6_addr group_mask,
				     struct in6_addr hash_mask));

static void delete_grp_mask_entry __P((cand_rp_t ** used_cand_rp_list,
			               grp_mask_t ** used_grp_mask_list,
			               grp_mask_t * grp_mask_delete));

static void delete_rp_entry __P((cand_rp_t ** used_cand_rp_list,
	                         grp_mask_t ** used_grp_mask_list,
	                         cand_rp_t * cand_rp_ptr));


void
init_rp6_and_bsr6()
{
    /* TODO: if the grplist is not NULL, remap all groups ASAP! */

    delete_rp_list(&cand_rp_list, &grp_mask_list);
    delete_rp_list(&segmented_cand_rp_list, &segmented_grp_mask_list);

    if (cand_bsr_flag == FALSE)
    {
	/*
	 * If I am not candidat BSR, initialize the "current BSR" as having
	 * the lowest priority.
	 */

	curr_bsr_fragment_tag = 0;
	curr_bsr_priority = 0;	/* Lowest priority */
	curr_bsr_address = sockaddr6_any;	/* Lowest priority */
	MASKLEN_TO_MASK6(RP_DEFAULT_IPV6_HASHMASKLEN, curr_bsr_hash_mask);
	SET_TIMER(pim_bootstrap_timer, PIM_BOOTSTRAP_TIMEOUT);
    }
    else
    {
	curr_bsr_fragment_tag = RANDOM();
	curr_bsr_priority = my_bsr_priority;
	curr_bsr_address = my_bsr_address;
	curr_bsr_hash_mask = my_bsr_hash_mask;
	SET_TIMER(pim_bootstrap_timer, bootstrap_initial_delay());
    }

    if (cand_rp_flag != FALSE)
    {
	MASKLEN_TO_MASK6(RP_DEFAULT_IPV6_HASHMASKLEN, rp_my_ipv6_hashmask);
	/* Setup the Cand-RP-Adv-Timer */
	SET_TIMER(pim_cand_rp_adv_timer, RANDOM() % my_cand_rp_adv_period);
    }
}


/*
 * XXX: This implementation is based on section 6.2 of RFC 2362, which
 * is highly dependent on IPv4.
 * We'll have to rewrite the function...
 */
u_int16
bootstrap_initial_delay()
{
//    long            AddrDelay;
//    long            Delay;
//    long            log_mask;
//    int             log_of_2;
//    u_int8          bestPriority;

    /*
     * The bootstrap timer initial value (if Cand-BSR). It depends of the
     * bootstrap router priority: higher priority has shorter value:
     * 
     * Delay = 5 + 2*log_2(1 + bestPriority - myPriority) + AddrDelay;
     * 
     * bestPriority = Max(storedPriority, myPriority); if (bestPriority ==
     * myPriority) AddrDelay = log_2(bestAddr - myAddr)/16; else AddrDelay =
     * 2 - (myAddr/2^31);
     */

//    bestPriority = max(curr_bsr_priority, my_bsr_priority);
//    if (bestPriority == my_bsr_priority)
 //   {
//	AddrDelay = ntohl(curr_bsr_address) - ntohl(my_bsr_address);
	/* Calculate the integer part of log_2 of (bestAddr - myAddr) */
	/*
	 * To do so, have to find the position number of the first bit from
	 * left which is `1`
	 */
//	log_mask = sizeof(AddrDelay) << 3;
//	log_mask = (1 << (log_mask - 1));	/* Set the leftmost bit to
//						 * `1` */
/*	for (log_of_2 = (sizeof(AddrDelay) << 3) - 1; log_of_2; log_of_2--)
	{
	    if (AddrDelay & log_mask)
		break;
	    else
*/
//		log_mask >>= 1;	/* Start shifting `1` on right */
/* }
	AddrDelay = log_of_2 / 16;
    }
    else
	AddrDelay = 2 - (ntohl(my_bsr_address) / (1 << 31));

    Delay = 1 + bestPriority - my_bsr_priority;
 */
   /* Calculate log_2(Delay) */
//    log_mask = sizeof(Delay) << 3;
//    log_mask = (1 << (log_mask - 1)); 	
/* Set the leftmost bit to `1` 
*/

 /*   for (log_of_2 = (sizeof(Delay) << 3) - 1; log_of_2; log_of_2--)
    {
	if (Delay & log_mask)
	    break;
	else
*/
//	    log_mask >>= 1;	/* Start shifting `1` on right */

/* }

    Delay = 5 + 2 * Delay + AddrDelay;
    return (u_int16) Delay;
*/

	/* Temporary implementation */
	return (RANDOM()%my_bsr_period);
}


static cand_rp_t *
add_cand_rp(used_cand_rp_list, address)
    cand_rp_t     		**used_cand_rp_list;
    struct sockaddr_in6		*address;
{
    cand_rp_t      *cand_rp_prev = (cand_rp_t *) NULL;
    cand_rp_t      *cand_rp;
    cand_rp_t      *cand_rp_new;
    rpentry_t      *rpentry_ptr;

    /* The ordering is the bigger first */
    for (cand_rp = *used_cand_rp_list; cand_rp != (cand_rp_t *) NULL;
	 cand_rp_prev = cand_rp, cand_rp = cand_rp->next)
    {

	if (inet6_greaterthan(&cand_rp->rpentry->address, address))
	    continue;
	if (inet6_equal(&cand_rp->rpentry->address , address))
	    return (cand_rp);
	else
	    break;
    }

    /* Create and insert the new entry between cand_rp_prev and cand_rp */
    cand_rp_new = (cand_rp_t *) malloc(sizeof(cand_rp_t));
    cand_rp_new->rp_grp_next = (rp_grp_entry_t *) NULL;
    cand_rp_new->next = cand_rp;
    cand_rp_new->prev = cand_rp_prev;
    if (cand_rp != (cand_rp_t *) NULL)
	cand_rp->prev = cand_rp_new;
    if (cand_rp_prev == (cand_rp_t *) NULL)
    {
	*used_cand_rp_list = cand_rp_new;
    }
    else
    {
	cand_rp_prev->next = cand_rp_new;
    }

    rpentry_ptr = (rpentry_t *) malloc(sizeof(rpentry_t));
    cand_rp_new->rpentry = rpentry_ptr;
    rpentry_ptr->next = (srcentry_t *) NULL;
    rpentry_ptr->prev = (srcentry_t *) NULL;
    rpentry_ptr->address = *address;
    rpentry_ptr->mrtlink = (mrtentry_t *) NULL;
    rpentry_ptr->incoming = NO_VIF;
    rpentry_ptr->upstream = (pim_nbr_entry_t *) NULL;

    /* TODO: setup the metric and the preference as ~0 (the lowest)? */

    rpentry_ptr->metric = ~0;
    rpentry_ptr->preference = ~0;
    RESET_TIMER(rpentry_ptr->timer);
    rpentry_ptr->cand_rp = cand_rp_new;

    /*
     * TODO: XXX: check whether there is a route to that RP: if return value
     * is FALSE, then no route.
     */

    if (local_address(&rpentry_ptr->address) == NO_VIF)
	{
		/* TODO: check for error and delete */
		set_incoming(rpentry_ptr, PIM_IIF_RP);
	}
    else
    {
	/* TODO: XXX: CHECK!!! */
	rpentry_ptr->incoming = reg_vif_num;
    }

    return (cand_rp_new);
}


static grp_mask_t *
add_grp_mask(used_grp_mask_list, group_addr, group_mask, hash_mask)
    grp_mask_t    **used_grp_mask_list;
    struct sockaddr_in6      *group_addr;
    struct in6_addr          group_mask;
    struct in6_addr          hash_mask;
{
    grp_mask_t     	*grp_mask_prev = (grp_mask_t *) NULL;
    grp_mask_t     	*grp_mask;
    grp_mask_t     	*grp_mask_tmp;
    struct sockaddr_in6	prefix_h;
    struct sockaddr_in6	prefix_h2;
    int 		i;

    /* I compare on the adresses, inet6_equal use the scope, too */
    prefix_h.sin6_scope_id = prefix_h2.sin6_scope_id = 0;

    for (i = 0; i < sizeof(struct in6_addr); i++)
	prefix_h.sin6_addr.s6_addr[i] =
		group_addr->sin6_addr.s6_addr[i] & group_mask.s6_addr[i];

    /* The ordering is: bigger first */
    for (grp_mask = *used_grp_mask_list; grp_mask != (grp_mask_t *) NULL;
	 grp_mask_prev = grp_mask, grp_mask = grp_mask->next)
    {
	for (i = 0; i < sizeof(struct in6_addr); i++)
	    prefix_h2.sin6_addr.s6_addr[i] =
		    (grp_mask->group_addr.sin6_addr.s6_addr[i] &
		     grp_mask->group_mask.s6_addr[i]);
	if (inet6_greaterthan(&prefix_h2, &prefix_h) )
	    continue;
	if (inet6_equal(&prefix_h2, &prefix_h))
	    return (grp_mask);
	else
	    break;
    }

    grp_mask_tmp = (grp_mask_t *) malloc(sizeof(grp_mask_t));
    grp_mask_tmp->grp_rp_next = (rp_grp_entry_t *) NULL;
    grp_mask_tmp->next = grp_mask;
    grp_mask_tmp->prev = grp_mask_prev;
    if (grp_mask != (grp_mask_t *) NULL)
	grp_mask->prev = grp_mask_tmp;
    if (grp_mask_prev == (grp_mask_t *) NULL)
    {
	*used_grp_mask_list = grp_mask_tmp;
    }
    else
    {
	grp_mask_prev->next = grp_mask_tmp;
    }

    grp_mask_tmp->group_addr = *group_addr;
    grp_mask_tmp->group_mask = group_mask;
    grp_mask_tmp->hash_mask = hash_mask;
    grp_mask_tmp->group_rp_number = 0;
    grp_mask_tmp->fragment_tag = 0;
    return (grp_mask_tmp);
}


/*
 * TODO: XXX: BUG: a remapping for some groups currently using some other
 * grp_mask may be required by the addition of the new entry!!! Remapping all
 * groups might be a costly process...
 */
rp_grp_entry_t *
add_rp_grp_entry(used_cand_rp_list, used_grp_mask_list,
		 rp_addr, rp_priority, rp_holdtime, group_addr, group_mask,
		 bsr_hash_mask,
		 fragment_tag)
    cand_rp_t     	**used_cand_rp_list;
    grp_mask_t    	**used_grp_mask_list;
    struct sockaddr_in6	*rp_addr;
    u_int8          	rp_priority;
    u_int16         	rp_holdtime;
    struct sockaddr_in6 *group_addr;
    struct in6_addr     group_mask;
    struct in6_addr bsr_hash_mask;
    u_int16         fragment_tag;
{
    cand_rp_t      *cand_rp_ptr;
    grp_mask_t     *grp_mask_ptr;
    rpentry_t      *rpentry_ptr;
    rp_grp_entry_t *grp_rp_entry_next;
    rp_grp_entry_t *grp_rp_entry_new;
    rp_grp_entry_t *grp_rp_entry_prev = (rp_grp_entry_t *) NULL;
    grpentry_t     *grpentry_ptr_prev;
    grpentry_t     *grpentry_ptr_next;
    u_int8          old_highest_priority = ~0;	/* Smaller value means
						 * "higher" */

    /* Input data verification */
    if (!inet6_valid_host(rp_addr))
	return (rp_grp_entry_t *) NULL;

    if (!IN6_IS_ADDR_MULTICAST(&group_addr->sin6_addr))
    {
		return (rp_grp_entry_t *) NULL;
    }
    grp_mask_ptr = add_grp_mask(used_grp_mask_list, group_addr, group_mask,
				bsr_hash_mask);
    if (grp_mask_ptr == (grp_mask_t *) NULL)
	return (rp_grp_entry_t *) NULL;

    /* TODO: delete */
#if 0
    if (grp_mask_ptr->grp_rp_next != (rp_grp_entry_t *) NULL)
    {
	/* Check for obsolete grp_rp chain */
	if ((my_bsr_address != curr_bsr_address)
	    && (grp_mask_ptr->grp_rp_next->fragment_tag != fragment_tag))
	{
	    /* This grp_rp chain is obsolete. Delete it. */
	    delete_grp_mask(used_cand_rp_list, used_grp_mask_list,
			    group_addr, group_mask);
	    grp_mask_ptr = add_grp_mask(used_grp_mask_list, group_addr,
					group_mask, bsr_hash_mask);
	    if (grp_mask_ptr == (grp_mask_t *) NULL)
		return (rp_grp_entry_t *) NULL;
	}
    }
#endif				/* 0 */

    cand_rp_ptr = add_cand_rp(used_cand_rp_list, rp_addr);
    if (cand_rp_ptr == (cand_rp_t *) NULL)
    {
	if (grp_mask_ptr->grp_rp_next == (rp_grp_entry_t *) NULL)
	    delete_grp_mask(used_cand_rp_list, used_grp_mask_list,
			    group_addr, group_mask);
	return (rp_grp_entry_t *) NULL;
    }

    rpentry_ptr = cand_rp_ptr->rpentry;
    SET_TIMER(rpentry_ptr->timer, rp_holdtime);
    grp_mask_ptr->fragment_tag = fragment_tag;	/* For garbage collection */

    grp_rp_entry_prev = (rp_grp_entry_t *) NULL;
    grp_rp_entry_next = grp_mask_ptr->grp_rp_next;

    /* TODO: improve it */

    if (grp_rp_entry_next != (rp_grp_entry_t *) NULL)
	old_highest_priority = grp_rp_entry_next->priority;
    for (; grp_rp_entry_next != (rp_grp_entry_t *) NULL;
	 grp_rp_entry_prev = grp_rp_entry_next,
	 grp_rp_entry_next = grp_rp_entry_next->grp_rp_next)
    {
	/*
	 * Smaller value means higher priority. The entries are sorted with
	 * the highest priority first.
	 */
	if (grp_rp_entry_next->priority < rp_priority)
	    continue;
	if (grp_rp_entry_next->priority > rp_priority)
	    break;

	/*
	 * Here we don't care about higher/lower addresses, because higher
	 * address does not guarantee higher hash_value, but anyway we do
	 * order with the higher address first, so it will be easier to find
	 * an existing entry and update the holdtime.
	 */

	if (inet6_greaterthan(&grp_rp_entry_next->rp->rpentry->address , rp_addr))
	    continue;
	if (inet6_lessthan(&grp_rp_entry_next->rp->rpentry->address , rp_addr))
	    break;

	/* We already have this entry. Update the holdtime */
	/*
	 * TODO: We shoudn't have old existing entry, because with the
	 * current implementation all of them will be deleted (different
	 * fragment_tag). Debug and check and eventually delete.
	 */

	grp_rp_entry_next->holdtime = rp_holdtime;
	grp_rp_entry_next->advholdtime = rp_holdtime;
	grp_rp_entry_next->fragment_tag = fragment_tag;
	return (grp_rp_entry_next);
    }

    /* Create and link the new entry */

    grp_rp_entry_new = (rp_grp_entry_t *) malloc(sizeof(rp_grp_entry_t));
    grp_rp_entry_new->grp_rp_next = grp_rp_entry_next;
    grp_rp_entry_new->grp_rp_prev = grp_rp_entry_prev;
    if (grp_rp_entry_next != (rp_grp_entry_t *) NULL)
	grp_rp_entry_next->grp_rp_prev = grp_rp_entry_new;
    if (grp_rp_entry_prev == (rp_grp_entry_t *) NULL)
	grp_mask_ptr->grp_rp_next = grp_rp_entry_new;
    else
	grp_rp_entry_prev->grp_rp_next = grp_rp_entry_new;

    /*
     * The rp_grp_entry chain is not ordered, so just plug the new entry at
     * the head.
     */

    grp_rp_entry_new->rp_grp_next = cand_rp_ptr->rp_grp_next;
    if (cand_rp_ptr->rp_grp_next != (rp_grp_entry_t *) NULL)
	cand_rp_ptr->rp_grp_next->rp_grp_prev = grp_rp_entry_new;
    grp_rp_entry_new->rp_grp_prev = (rp_grp_entry_t *) NULL;
    cand_rp_ptr->rp_grp_next = grp_rp_entry_new;

    grp_rp_entry_new->holdtime = rp_holdtime;
    grp_rp_entry_new->advholdtime = rp_holdtime;
    grp_rp_entry_new->fragment_tag = fragment_tag;
    grp_rp_entry_new->priority = rp_priority;
    grp_rp_entry_new->group = grp_mask_ptr;
    grp_rp_entry_new->rp = cand_rp_ptr;
    grp_rp_entry_new->grplink = (grpentry_t *) NULL;

    grp_mask_ptr->group_rp_number++;

    if (grp_mask_ptr->grp_rp_next->priority == rp_priority)
    {
	/* The first entries are with the best priority. */
	/* Adding this rp_grp_entry may result in group_to_rp remapping */
	for (grp_rp_entry_next = grp_mask_ptr->grp_rp_next;
	     grp_rp_entry_next != (rp_grp_entry_t *) NULL;
	     grp_rp_entry_next = grp_rp_entry_next->grp_rp_next)
	{
	    if (grp_rp_entry_next->priority > old_highest_priority)
		break;
	    for (grpentry_ptr_prev = grp_rp_entry_next->grplink;
		 grpentry_ptr_prev != (grpentry_t *) NULL;)
	    {
		grpentry_ptr_next = grpentry_ptr_prev->rpnext;
		remap_grpentry(grpentry_ptr_prev);
		grpentry_ptr_prev = grpentry_ptr_next;
	    }
	}
    }

    return (grp_rp_entry_new);
}


void
delete_rp_grp_entry(used_cand_rp_list, used_grp_mask_list,
		    rp_grp_entry_delete)
    cand_rp_t     **used_cand_rp_list;
    grp_mask_t    **used_grp_mask_list;
    rp_grp_entry_t *rp_grp_entry_delete;
{
    grpentry_t     *grpentry_ptr;
    grpentry_t     *grpentry_ptr_next;

    if (rp_grp_entry_delete == (rp_grp_entry_t *) NULL)
	return;
    rp_grp_entry_delete->group->group_rp_number--;
    /* Free the rp_grp* and grp_rp* links */
    if (rp_grp_entry_delete->rp_grp_prev != (rp_grp_entry_t *) NULL)
	rp_grp_entry_delete->rp_grp_prev->rp_grp_next =
	    rp_grp_entry_delete->rp_grp_next;
    else
	rp_grp_entry_delete->rp->rp_grp_next =
	    rp_grp_entry_delete->rp_grp_next;
    if (rp_grp_entry_delete->rp_grp_next != (rp_grp_entry_t *) NULL)
	rp_grp_entry_delete->rp_grp_next->rp_grp_prev =
	    rp_grp_entry_delete->rp_grp_prev;

    if (rp_grp_entry_delete->grp_rp_prev != (rp_grp_entry_t *) NULL)
	rp_grp_entry_delete->grp_rp_prev->grp_rp_next =
	    rp_grp_entry_delete->grp_rp_next;
    else
	rp_grp_entry_delete->group->grp_rp_next =
	    rp_grp_entry_delete->grp_rp_next;
    if (rp_grp_entry_delete->grp_rp_next != (rp_grp_entry_t *) NULL)
	rp_grp_entry_delete->grp_rp_next->grp_rp_prev =
	    rp_grp_entry_delete->grp_rp_prev;

    /* Delete Cand-RP or Group-prefix if useless */
    if (rp_grp_entry_delete->group->grp_rp_next ==
	(rp_grp_entry_t *) NULL)
	delete_grp_mask_entry(used_cand_rp_list, used_grp_mask_list,
			      rp_grp_entry_delete->group);
    if (rp_grp_entry_delete->rp->rp_grp_next ==
	(rp_grp_entry_t *) NULL)
	delete_rp_entry(used_cand_rp_list, used_grp_mask_list,
			rp_grp_entry_delete->rp);

    /* Remap all affected groups */
    for (grpentry_ptr = rp_grp_entry_delete->grplink;
	 grpentry_ptr != (grpentry_t *) NULL;
	 grpentry_ptr = grpentry_ptr_next)
    {
	grpentry_ptr_next = grpentry_ptr->rpnext;
	remap_grpentry(grpentry_ptr);
    }

    free((char *) rp_grp_entry_delete);
}

/*
 * TODO: XXX: the affected group entries will be partially setup, because may
 * have group routing entry, but NULL pointers to RP. After the call to this
 * function, must remap all group entries ASAP.
 */

void
delete_rp_list(used_cand_rp_list, used_grp_mask_list)
    cand_rp_t     **used_cand_rp_list;
    grp_mask_t    **used_grp_mask_list;
{
    cand_rp_t      *cand_rp_ptr,
                   *cand_rp_next;
    grp_mask_t     *grp_mask_ptr,
                   *grp_mask_next;
    rp_grp_entry_t *rp_grp_entry_ptr,
                   *rp_grp_entry_next;
    grpentry_t     *grpentry_ptr,
                   *grpentry_ptr_next;

    for (cand_rp_ptr = *used_cand_rp_list;
	 cand_rp_ptr != (cand_rp_t *) NULL;)
    {
	cand_rp_next = cand_rp_ptr->next;
	/* Free the mrtentry (if any) for this RP */
	if (cand_rp_ptr->rpentry->mrtlink != (mrtentry_t *) NULL)
	{
	    if (cand_rp_ptr->rpentry->mrtlink->flags & MRTF_KERNEL_CACHE)
		delete_mrtentry_all_kernel_cache(cand_rp_ptr->rpentry->mrtlink);
	    FREE_MRTENTRY(cand_rp_ptr->rpentry->mrtlink);
	}
	free(cand_rp_ptr->rpentry);

	/* Free the whole chain of rp_grp_entry for this RP */
	for (rp_grp_entry_ptr = cand_rp_ptr->rp_grp_next;
	     rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
	     rp_grp_entry_ptr = rp_grp_entry_next)
	{
	    rp_grp_entry_next = rp_grp_entry_ptr->rp_grp_next;
	    /* Clear the RP related invalid pointers for all group entries */
	    for (grpentry_ptr = rp_grp_entry_ptr->grplink;
		 grpentry_ptr != (grpentry_t *) NULL;
		 grpentry_ptr = grpentry_ptr_next)
	    {
		grpentry_ptr_next = grpentry_ptr->rpnext;
		grpentry_ptr->rpnext = (grpentry_t *) NULL;
		grpentry_ptr->rpprev = (grpentry_t *) NULL;
		grpentry_ptr->active_rp_grp = (rp_grp_entry_t *) NULL;
		grpentry_ptr->rpaddr = sockaddr6_any;
	    }
	    free(rp_grp_entry_ptr);
	}
	cand_rp_ptr = cand_rp_next;
    }
    *used_cand_rp_list = (cand_rp_t *) NULL;

    for (grp_mask_ptr = *used_grp_mask_list;
	 grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_next)
    {
	grp_mask_next = grp_mask_ptr->next;
	free(grp_mask_ptr);
    }
    *used_grp_mask_list = (grp_mask_t *) NULL;
}


void
delete_grp_mask(used_cand_rp_list, used_grp_mask_list, group_addr, group_mask)
    cand_rp_t     	**used_cand_rp_list;
    grp_mask_t    	**used_grp_mask_list;
    struct sockaddr_in6	*group_addr;
    struct in6_addr     group_mask;
{
    grp_mask_t     	*grp_mask_ptr;
    struct sockaddr_in6	prefix_h;
    struct sockaddr_in6 prefix_h2;
	int i;

    for (i = 0; i < sizeof(struct in6_addr); i++)
	prefix_h.sin6_addr.s6_addr[i] = group_addr->sin6_addr.s6_addr[i]&group_mask.s6_addr[i];

    for (grp_mask_ptr = *used_grp_mask_list;
	 grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_ptr->next)
    {
	for (i = 0; i < sizeof(struct in6_addr); i++)
	    prefix_h2.sin6_addr.s6_addr[i] = 
		grp_mask_ptr->group_addr.sin6_addr.s6_addr[i]&grp_mask_ptr->group_mask.s6_addr[i];

	if (inet6_greaterthan(&prefix_h2, &prefix_h))
	    continue;
	if (IN6_ARE_ADDR_EQUAL(&grp_mask_ptr->group_addr.sin6_addr,
			       &group_addr->sin6_addr) &&
	    IN6_ARE_ADDR_EQUAL(&grp_mask_ptr->group_mask, &group_mask))	
	    break;
	else
	    return;		/* Not found */
    }

    if (grp_mask_ptr == (grp_mask_t *) NULL)
	return;			/* Not found */

    delete_grp_mask_entry(used_cand_rp_list, used_grp_mask_list,
			  grp_mask_ptr);
}

static void
delete_grp_mask_entry(used_cand_rp_list, used_grp_mask_list, grp_mask_delete)
    cand_rp_t     **used_cand_rp_list;
    grp_mask_t    **used_grp_mask_list;
    grp_mask_t     *grp_mask_delete;
{
    grpentry_t     *grpentry_ptr,
                   *grpentry_ptr_next;
    rp_grp_entry_t *grp_rp_entry_ptr;
    rp_grp_entry_t *grp_rp_entry_next;

    if (grp_mask_delete == (grp_mask_t *) NULL)
	return;

    /* Remove from the grp_mask_list first */

    if (grp_mask_delete->prev != (grp_mask_t *) NULL)
	grp_mask_delete->prev->next = grp_mask_delete->next;
    else
	*used_grp_mask_list = grp_mask_delete->next;

    if (grp_mask_delete->next != (grp_mask_t *) NULL)
	grp_mask_delete->next->prev = grp_mask_delete->prev;

    /* Remove all grp_rp entries for this grp_mask */
    for (grp_rp_entry_ptr = grp_mask_delete->grp_rp_next;
	 grp_rp_entry_ptr != (rp_grp_entry_t *) NULL;
	 grp_rp_entry_ptr = grp_rp_entry_next)
    {
	grp_rp_entry_next = grp_rp_entry_ptr->grp_rp_next;
	/* Remap all related grpentry */
	for (grpentry_ptr = grp_rp_entry_ptr->grplink;
	     grpentry_ptr != (grpentry_t *) NULL;
	     grpentry_ptr = grpentry_ptr_next)
	{
	    grpentry_ptr_next = grpentry_ptr->rpnext;
	    remap_grpentry(grpentry_ptr);

	}
	if (grp_rp_entry_ptr->rp_grp_prev != (rp_grp_entry_t *) NULL)
	{
	    grp_rp_entry_ptr->rp_grp_prev->rp_grp_next =
		grp_rp_entry_ptr->rp_grp_next;
	}
	else
	{
	    grp_rp_entry_ptr->rp->rp_grp_next = grp_rp_entry_ptr->rp_grp_next;
	}
	if (grp_rp_entry_ptr->rp_grp_next != (rp_grp_entry_t *) NULL)
	    grp_rp_entry_ptr->rp_grp_next->rp_grp_prev =
		grp_rp_entry_ptr->rp_grp_prev;
	if (grp_rp_entry_ptr->rp->rp_grp_next == (rp_grp_entry_t *) NULL)
	{
	    /* Delete the RP entry */
	    delete_rp_entry(used_cand_rp_list, used_grp_mask_list,
			    grp_rp_entry_ptr->rp);
	}
	free(grp_rp_entry_ptr);
    }
}

/*
 * TODO: currently not used.
 */

void
delete_rp(used_cand_rp_list, used_grp_mask_list, rp_addr)
    cand_rp_t     	**used_cand_rp_list;
    grp_mask_t    	**used_grp_mask_list;
    struct sockaddr_in6  *rp_addr;
{
    cand_rp_t      *cand_rp_ptr;

    for (cand_rp_ptr = *used_cand_rp_list;
	 cand_rp_ptr != (cand_rp_t *) NULL;
	 cand_rp_ptr = cand_rp_ptr->next)
    {
	if (inet6_greaterthan(&cand_rp_ptr->rpentry->address, rp_addr))
	    continue;
	if (inet6_equal(&cand_rp_ptr->rpentry->address,rp_addr))
	    break;
	else
	    return;		/* Not found */
    }

    if (cand_rp_ptr == (cand_rp_t *) NULL)
	return;			/* Not found */
    delete_rp_entry(used_cand_rp_list, used_grp_mask_list, cand_rp_ptr);
}


static void
delete_rp_entry(used_cand_rp_list, used_grp_mask_list, cand_rp_delete)
    cand_rp_t     **used_cand_rp_list;
    grp_mask_t    **used_grp_mask_list;
    cand_rp_t      *cand_rp_delete;
{
    rp_grp_entry_t *rp_grp_entry_ptr;
    rp_grp_entry_t *rp_grp_entry_next;
    grpentry_t     *grpentry_ptr;
    grpentry_t     *grpentry_ptr_next;

    if (cand_rp_delete == (cand_rp_t *) NULL)
	return;

    /* Remove from the cand-RP chain */
    if (cand_rp_delete->prev != (cand_rp_t *) NULL)
	cand_rp_delete->prev->next = cand_rp_delete->next;
    else
	*used_cand_rp_list = cand_rp_delete->next;

    if (cand_rp_delete->next != (cand_rp_t *) NULL)
	cand_rp_delete->next->prev = cand_rp_delete->prev;

    if (cand_rp_delete->rpentry->mrtlink != (mrtentry_t *) NULL)
    {
	if (cand_rp_delete->rpentry->mrtlink->flags & MRTF_KERNEL_CACHE)
	    delete_mrtentry_all_kernel_cache(cand_rp_delete->rpentry->mrtlink);
	FREE_MRTENTRY(cand_rp_delete->rpentry->mrtlink);
    }
    free((char *) cand_rp_delete->rpentry);

    /* Remove all rp_grp entries for this RP */
    for (rp_grp_entry_ptr = cand_rp_delete->rp_grp_next;
	 rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
	 rp_grp_entry_ptr = rp_grp_entry_next)
    {
	rp_grp_entry_next = rp_grp_entry_ptr->rp_grp_next;
	rp_grp_entry_ptr->group->group_rp_number--;

	/* First take care of the grp_rp chain */
	if (rp_grp_entry_ptr->grp_rp_prev != (rp_grp_entry_t *) NULL)
	{
	    rp_grp_entry_ptr->grp_rp_prev->grp_rp_next =
		rp_grp_entry_ptr->grp_rp_next;
	}
	else
	{
	    rp_grp_entry_ptr->group->grp_rp_next =
		rp_grp_entry_ptr->grp_rp_next;
	}
	if (rp_grp_entry_ptr->grp_rp_next != (rp_grp_entry_t *) NULL)
	{
	    rp_grp_entry_ptr->grp_rp_next->grp_rp_prev =
		rp_grp_entry_ptr->grp_rp_prev;
	}

	if (rp_grp_entry_ptr->grp_rp_next == (rp_grp_entry_t *) NULL)
	{
	    delete_grp_mask_entry(used_cand_rp_list, used_grp_mask_list,
				  rp_grp_entry_ptr->group);
	}

	/* Remap the related groups */
	for (grpentry_ptr = rp_grp_entry_ptr->grplink;
	     grpentry_ptr != (grpentry_t *) NULL;
	     grpentry_ptr = grpentry_ptr_next)
	{
	    grpentry_ptr_next = grpentry_ptr->rpnext;
	    remap_grpentry(grpentry_ptr);
	}
	free(rp_grp_entry_ptr);
    }
    free((char *) cand_rp_delete);
}


/*
 * Rehash the RP for the group. XXX: currently, every time when
 * remap_grpentry() is called, there has being a good reason to change the
 * RP, so for performancy reasons no check is performed whether the RP will
 * be really different one.
 */

int
remap_grpentry(grpentry_ptr)
    grpentry_t     *grpentry_ptr;
{
    rpentry_t      *rpentry_ptr;
    rp_grp_entry_t *rp_grp_entry_ptr;
    mrtentry_t     *grp_route;
    mrtentry_t     *mrtentry_ptr;

    if (grpentry_ptr == (grpentry_t *) NULL)
	return (FALSE);

    /* Remove from the list of all groups matching to the same RP */
    if (grpentry_ptr->rpprev != (grpentry_t *) NULL)
	grpentry_ptr->rpprev->rpnext = grpentry_ptr->rpnext;
    else
    {
	if (grpentry_ptr->active_rp_grp != (rp_grp_entry_t *) NULL)
	    grpentry_ptr->active_rp_grp->grplink = grpentry_ptr->rpnext;
    }
    if (grpentry_ptr->rpnext != (grpentry_t *) NULL)
	grpentry_ptr->rpnext->rpprev = grpentry_ptr->rpprev;

    rp_grp_entry_ptr = rp_grp_match(&grpentry_ptr->group);
    if (rp_grp_entry_ptr == (rp_grp_entry_t *) NULL)
    {
	/* If cannot remap, delete the group */
	delete_grpentry(grpentry_ptr);
	return (FALSE);
    }
    rpentry_ptr = rp_grp_entry_ptr->rp->rpentry;

    /* Add to the new chain of all groups mapping to the same RP */
    grpentry_ptr->rpaddr = rpentry_ptr->address;
    grpentry_ptr->active_rp_grp = rp_grp_entry_ptr;
    grpentry_ptr->rpnext = rp_grp_entry_ptr->grplink;
    if (grpentry_ptr->rpnext != (grpentry_t *) NULL)
	grpentry_ptr->rpnext->rpprev = grpentry_ptr;
    grpentry_ptr->rpprev = (grpentry_t *) NULL;
    rp_grp_entry_ptr->grplink = grpentry_ptr;

    if ((grp_route = grpentry_ptr->grp_route) != (mrtentry_t *) NULL)
    {
	grp_route->upstream = rpentry_ptr->upstream;
	grp_route->metric = rpentry_ptr->metric;
	grp_route->preference = rpentry_ptr->preference;
	change_interfaces(grp_route, rpentry_ptr->incoming,
			  &grp_route->joined_oifs,
			  &grp_route->pruned_oifs,
			  &grp_route->leaves,
			  &grp_route->asserted_oifs, MFC_UPDATE_FORCE);
    }

    for (mrtentry_ptr = grpentry_ptr->mrtlink;
	 mrtentry_ptr != (mrtentry_t *) NULL;
	 mrtentry_ptr = mrtentry_ptr->grpnext)
    {
	if (!(mrtentry_ptr->flags & MRTF_RP))
	    continue;
	mrtentry_ptr->upstream = rpentry_ptr->upstream;
	mrtentry_ptr->metric = rpentry_ptr->metric;
	mrtentry_ptr->preference = rpentry_ptr->preference;
	change_interfaces(mrtentry_ptr, rpentry_ptr->incoming,
			  &mrtentry_ptr->joined_oifs,
			  &mrtentry_ptr->pruned_oifs,
			  &mrtentry_ptr->leaves,
			  &mrtentry_ptr->asserted_oifs, MFC_UPDATE_FORCE);
    }

    return (TRUE);
}


rpentry_t      *
rp_match(group)
    struct sockaddr_in6	*group;
{
    rp_grp_entry_t *rp_grp_entry_ptr;

    rp_grp_entry_ptr = rp_grp_match(group);
    if (rp_grp_entry_ptr != (rp_grp_entry_t *) NULL)
	return (rp_grp_entry_ptr->rp->rpentry);
    else
	return (rpentry_t *) NULL;
}


rp_grp_entry_t *
rp_grp_match(group)
    struct sockaddr_in6	*group;
{
    grp_mask_t     *grp_mask_ptr;
    rp_grp_entry_t *grp_rp_entry_ptr;
    rp_grp_entry_t *best_entry = (rp_grp_entry_t *) NULL;
    u_int8          best_priority = ~0;	/* Smaller is better */
    u_int32         best_hash_value = 0;	/* Bigger is better */
    struct sockaddr_in6         best_address_h;	/* Bigger is better */
    u_int32         curr_hash_value = 0;
    struct sockaddr_in6         curr_address_h;

    struct sockaddr_in6     prefix_h;
    struct sockaddr_in6	    prefix_h2;
    int i;	

    if (grp_mask_list == (grp_mask_t *) NULL)
	return (rp_grp_entry_t *) NULL;
 
    /* XXX: I compare on the adresses, inet6_equal use the scope too */
    prefix_h.sin6_scope_id = prefix_h2.sin6_scope_id = 0;

    for (grp_mask_ptr = grp_mask_list; grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_ptr->next)
    {
	for (i = 0; i < sizeof(struct in6_addr); i++)
	    prefix_h2.sin6_addr.s6_addr[i] = 
		(grp_mask_ptr->group_addr.sin6_addr.s6_addr[i] &
		 grp_mask_ptr->group_mask.s6_addr[i]);
	for (i = 0; i < sizeof(struct in6_addr); i++)
	    prefix_h.sin6_addr.s6_addr[i] = 
		(group->sin6_addr.s6_addr[i] &
		 grp_mask_ptr->group_mask.s6_addr[i]);

	/* Search the grp_mask (group_prefix) list */
	if (!inet6_equal(&prefix_h, &prefix_h2))
	    continue;

	for (grp_rp_entry_ptr = grp_mask_ptr->grp_rp_next;
	     grp_rp_entry_ptr != (rp_grp_entry_t *) NULL;
	     grp_rp_entry_ptr = grp_rp_entry_ptr->grp_rp_next)
	{

	    if (best_priority < grp_rp_entry_ptr->priority)
		break;

	    curr_address_h = grp_rp_entry_ptr->rp->rpentry->address;
#if 0
	    curr_hash_value = RP_HASH_VALUE(crc((char *)&group->sin6_addr,
						sizeof(struct in6_addr)),
					    crc((char *)&grp_mask_ptr->hash_mask,
						sizeof(struct in6_addr)),
					    crc((char *)&curr_address_h.sin6_addr,
						sizeof(struct in6_addr)));
#else
	    {
		    struct in6_addr masked_grp;
		    int i;

		    for (i = 0; i < sizeof(struct in6_addr); i++)
			    masked_grp.s6_addr[i] =
				    group->sin6_addr.s6_addr[i] &
				    grp_mask_ptr->hash_mask.s6_addr[i];
		    curr_hash_value = RP_HASH_VALUE2(crc((char *)&masked_grp,
							 sizeof(struct in6_addr)),
						     crc((char *)&curr_address_h.sin6_addr,
							 sizeof(struct in6_addr)));
	    }
#endif

	    if (best_priority == grp_rp_entry_ptr->priority)
	    {
		/* Compare the hash_value and then the addresses */

		if (curr_hash_value < best_hash_value)
		    continue;
		if (curr_hash_value == best_hash_value)
		    if (inet6_lessthan(&curr_address_h ,&best_address_h))
			continue;
	    }

	    /* The current entry in the loop is preferred */

	    best_entry = grp_rp_entry_ptr;
	    best_priority = best_entry->priority;
	    best_address_h = curr_address_h;
	    best_hash_value = curr_hash_value;
	}
    }


    if (best_entry == (rp_grp_entry_t *) NULL)
	return (rp_grp_entry_t *) NULL;

    IF_DEBUG(DEBUG_PIM_CAND_RP)
	log(LOG_DEBUG,0,"Rp_grp_match found %s for group %s",
	    inet6_fmt(&best_entry->rp->rpentry->address.sin6_addr),
	    inet6_fmt(&group->sin6_addr));

    return (best_entry);
}


rpentry_t      *
rp_find(rp_address)
    struct sockaddr_in6	*rp_address;
{
    cand_rp_t      *cand_rp_ptr;

    for (cand_rp_ptr = cand_rp_list; cand_rp_ptr != (cand_rp_t *) NULL;
	 cand_rp_ptr = cand_rp_ptr->next)
    {
	if( inet6_greaterthan(&cand_rp_ptr->rpentry->address,rp_address))
	    continue;
	if( inet6_equal(&cand_rp_ptr->rpentry->address,rp_address))
	    return (cand_rp_ptr->rpentry);
	return (rpentry_t *) NULL;
    }

    return (rpentry_t *) NULL;
}


/*
 * Create a bootstrap message in "send_buff" and returns the data size
 * (excluding the IP header and the PIM header) Can be used both by the
 * Bootstrap router to multicast the RP-set or by the DR to unicast it to a
 * new neighbor. It DOES NOT change any timers.
 */

int
create_pim6_bootstrap_message(send_buff)
    char           *send_buff;
{
    u_int8         *data_ptr;
    grp_mask_t     *grp_mask_ptr;
    rp_grp_entry_t *grp_rp_entry_ptr;
    int             datalen;
    u_int8          masklen=0;

    if (IN6_IS_ADDR_UNSPECIFIED(&curr_bsr_address.sin6_addr))
	return (0);

    data_ptr = (u_int8 *) (send_buff + sizeof(struct pim));

    if( inet6_equal(&curr_bsr_address , &my_bsr_address ))
	curr_bsr_fragment_tag++;
    PUT_HOSTSHORT(curr_bsr_fragment_tag, data_ptr);
    MASK_TO_MASKLEN6(curr_bsr_hash_mask, masklen);
    PUT_BYTE(masklen, data_ptr);
    PUT_BYTE(curr_bsr_priority, data_ptr);
    PUT_EUADDR6(curr_bsr_address.sin6_addr, data_ptr);

    /* TODO: XXX: No fragmentation support (yet) */

    for (grp_mask_ptr = grp_mask_list; grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_ptr->next)
    {
	MASK_TO_MASKLEN6(grp_mask_ptr->group_mask, masklen);
	PUT_EGADDR6(grp_mask_ptr->group_addr.sin6_addr, masklen, 0, data_ptr);
	PUT_BYTE(grp_mask_ptr->group_rp_number, data_ptr);
	PUT_BYTE(grp_mask_ptr->group_rp_number, data_ptr);	/* TODO: if frag. */
	PUT_HOSTSHORT(0, data_ptr);
	for (grp_rp_entry_ptr = grp_mask_ptr->grp_rp_next;
	     grp_rp_entry_ptr != (rp_grp_entry_t *) NULL;
	     grp_rp_entry_ptr = grp_rp_entry_ptr->grp_rp_next)
	{
	    PUT_EUADDR6(grp_rp_entry_ptr->rp->rpentry->address.sin6_addr, data_ptr);
	    PUT_HOSTSHORT(grp_rp_entry_ptr->advholdtime, data_ptr);
	    PUT_BYTE(grp_rp_entry_ptr->priority, data_ptr);
	    PUT_BYTE(0, data_ptr);	/* The reserved field */
	}
    }

    datalen = (data_ptr - (u_int8 *) send_buff) - sizeof(struct pim);

    return (datalen);
}


/*
 * Check if the rp_addr is the RP for the group corresponding to
 * mrtentry_ptr. Return TRUE or FALSE.
 */

int 
check_mrtentry_rp(mrtentry_ptr, rp_addr)
    mrtentry_t     	*mrtentry_ptr;
    struct sockaddr_in6	*rp_addr;
{
    rp_grp_entry_t *rp_grp_entry_ptr;

    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return (FALSE);
    if (IN6_IS_ADDR_UNSPECIFIED(&rp_addr->sin6_addr))
	return (FALSE);
    rp_grp_entry_ptr = mrtentry_ptr->group->active_rp_grp;
    if (rp_grp_entry_ptr == (rp_grp_entry_t *) NULL)
	return (FALSE);
    if (inet6_equal(&mrtentry_ptr->group->rpaddr,rp_addr))
	return (TRUE);
    return (FALSE);
}

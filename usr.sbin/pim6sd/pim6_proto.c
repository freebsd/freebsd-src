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
 * $FreeBSD: src/usr.sbin/pim6sd/pim6_proto.c,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet6/pim6.h>
#include <netinet/ip6.h>
#include <syslog.h>
#include <stdlib.h>
#include "mrt.h"
#include "defs.h"
#include "vif.h"
#include "debug.h"
#include "pim6.h"
#include "pim6_proto.h"
#include "pimd.h"
#include "rp.h"
#include "mld6.h"
#include "timer.h"
#include "route.h"
#include "inet6.h"
#include "kern.h"
#include "routesock.h"

/*
 * Local functions definitions.
 */

static int parse_pim6_hello 			__P((char *pktPtr , int datalen , struct sockaddr_in6 *src,
						     u_int16 *holdtime));

static int send_pim6_register_stop 		__P((struct sockaddr_in6 *reg_src , struct sockaddr_in6 *reg_dst , 
	                       		     	     struct sockaddr_in6 *inner_source,
		                	             struct sockaddr_in6 *inner_grp));

static build_jp_message_t *get_jp6_working_buff __P((void));
static void return_jp6_working_buff 		__P((pim_nbr_entry_t * pim_nbr));
static void pack_jp6_message 			__P((pim_nbr_entry_t * pim_nbr));
static void send_jp6_message 			__P((pim_nbr_entry_t * pim_nbr));
static int compare_metrics 			__P((u_int32 local_preference,
		                		     u_int32 local_metric,
		                		     struct sockaddr_in6 *local_address,
		                		     u_int32 remote_preference,
		                		     u_int32 remote_metric,
		                		     struct sockaddr_in6 *remote_address));

build_jp_message_t 	*build_jp_message_pool;
int			build_jp_message_pool_counter;
struct sockaddr_in6 sockaddr6_any = {sizeof(struct sockaddr_in6) , AF_INET6 ,0,0, IN6ADDR_ANY_INIT};
struct sockaddr_in6 sockaddr6_d;

struct pim6dstat pim6dstat;

/************************************************************************
 *                        PIM_HELLO
 ************************************************************************/
int
receive_pim6_hello(src, pim_message, datalen)
    struct sockaddr_in6 *src;
    register char       *pim_message;
    int                 datalen;
{
    mifi_t			mifi;
    struct uvif    		*v;
    register pim_nbr_entry_t 	*nbr,
                   		*prev_nbr,
                   		*new_nbr;
    u_int16         		holdtime;
    int             		bsr_length;
    u_int8         		*data_ptr;
    srcentry_t     		*srcentry_ptr;
    mrtentry_t     		*mrtentry_ptr;


    if ((mifi = find_vif_direct(src)) == NO_VIF)
    {
	/*
	 * Either a local vif or somehow received PIM_HELLO from non-directly
	 * connected router. Ignore it.
	 */

	if (local_address(src) == NO_VIF)
	    log(LOG_INFO, 0, "Ignoring PIM_HELLO from non-neighbor router %s",
		inet6_fmt(&src->sin6_addr));
	return (FALSE);
    }

    v = &uvifs[mifi];
    v->uv_in_pim6_hello++;	/* increment statistacs */
    if (v->uv_flags & (VIFF_DOWN | VIFF_DISABLED | MIFF_REGISTER))
	return (FALSE);		/* Shoudn't come on this interface */

    data_ptr = (u_int8 *) (pim_message + sizeof(struct pim));

    /* Get the Holdtime (in seconds) from the message. Return if error. */

    if (parse_pim6_hello(pim_message, datalen, src, &holdtime) == FALSE)
	return (FALSE);
    IF_DEBUG(DEBUG_PIM_HELLO | DEBUG_PIM_TIMER)
	log(LOG_DEBUG, 0, "PIM HELLO holdtime from %s is %u",
	    inet6_fmt(&src->sin6_addr), holdtime);

    for (prev_nbr = (pim_nbr_entry_t *) NULL, nbr = v->uv_pim_neighbors;
	 nbr != (pim_nbr_entry_t *) NULL;
	 prev_nbr = nbr, nbr = nbr->next)
    {
	/*
	 * The PIM neighbors are sorted in decreasing order of the network
	 * addresses (note that to be able to compare them correctly we must
	 * translate the addresses in host order.
	 */

	if (inet6_lessthan(src, &nbr->address))  
	    continue;
	if (inet6_equal(src, &nbr->address)) 
	{
	    /* We already have an entry for this host */

	    if (0 == holdtime)
	    {
		/*
		 * Looks like we have a nice neighbor who is going down and
		 * wants to inform us by sending "holdtime=0". Thanks buddy
		 * and see you again!
		 */

		log(LOG_INFO, 0, "PIM HELLO received: neighbor %s going down",
		    inet6_fmt(&src->sin6_addr));
		delete_pim6_nbr(nbr);
		return (TRUE);
	    }
	    SET_TIMER(nbr->timer, holdtime);
	    return (TRUE);
	}
	else
	    /*
	     * No entry for this neighbor. Exit the loop and create an entry
	     * for it.
	     */
	    break;
    }

    /*
     * This is a new neighbor. Create a new entry for it. It must be added
     * right after `prev_nbr`
     */

    new_nbr = (pim_nbr_entry_t *) malloc(sizeof(pim_nbr_entry_t));
    new_nbr->address 		= *src;
    new_nbr->vifi 		= mifi;
    SET_TIMER(new_nbr->timer, holdtime);
    new_nbr->build_jp_message 	= (build_jp_message_t *) NULL;
    new_nbr->next 		= nbr;
    new_nbr->prev 		= prev_nbr;

    if (prev_nbr != (pim_nbr_entry_t *) NULL)
	prev_nbr->next = new_nbr;
    else
	v->uv_pim_neighbors = new_nbr;
    if (new_nbr->next != (pim_nbr_entry_t *) NULL)
	new_nbr->next->prev = new_nbr;

    v->uv_flags &= ~VIFF_NONBRS;
    v->uv_flags |= VIFF_PIM_NBR;

    /* Since a new neighbour has come up, let it know your existence */
    /*
     * XXX: TODO: not in the spec, but probably should send the message after
     * a short random period?
     */

    send_pim6_hello(v, pim_hello_holdtime);

    if (v->uv_flags & VIFF_DR)
    {
	/*
	 * If I am the current DR on that interface, so send an RP-Set
	 * message to the new neighbor.
	 */

	if ((bsr_length = create_pim6_bootstrap_message(pim6_send_buf)))
    		send_pim6(pim6_send_buf, &v->uv_linklocal->pa_addr , src , PIM_BOOTSTRAP,
	     		  bsr_length);


	/* The router with highest network address is the elected DR */
	if (inet6_lessthan(&v->uv_linklocal->pa_addr,&v->uv_pim_neighbors->address))
	{
	    /*
	     * I was the DR, but not anymore. Remove all register_vif from
	     * oif list for all directly connected sources (for vifi).
	     */
	    /* TODO: XXX: first entry is not used! */
	    for (srcentry_ptr = srclist->next;
		 srcentry_ptr != (srcentry_t *) NULL;
		 srcentry_ptr = srcentry_ptr->next)
	    {
		/* If not directly connected source for vifi */

		if ((srcentry_ptr->incoming != mifi)
		    || (srcentry_ptr->upstream != (pim_nbr_entry_t *) NULL))
		    continue;
		for (mrtentry_ptr = srcentry_ptr->mrtlink;
		     mrtentry_ptr != (mrtentry_t *) NULL;
		     mrtentry_ptr = mrtentry_ptr->srcnext)
		{
		    if (!(mrtentry_ptr->flags & MRTF_SG))
			continue;	/* This is not (S,G) entry */
		    /* Remove the register oif */
		    IF_CLR(reg_vif_num, &mrtentry_ptr->joined_oifs);
		    change_interfaces(mrtentry_ptr,
				      mrtentry_ptr->incoming,
				      &mrtentry_ptr->joined_oifs,
				      &mrtentry_ptr->pruned_oifs,
				      &mrtentry_ptr->leaves,
				      &mrtentry_ptr->asserted_oifs, 0);
		}
	    }
	    v->uv_flags &= ~VIFF_DR;
	}
    }

    /*
     * TODO: XXX: does a new neighbor change any routing entries info? Need
     * to trigger joins?
     */

    IF_DEBUG(DEBUG_PIM_HELLO)
	log(LOG_DEBUG,0,"I'have got a new neighbor %s on vif %s",inet6_fmt(&src->sin6_addr),v->uv_name);
    return (TRUE);
}


void
delete_pim6_nbr(nbr_delete)
    pim_nbr_entry_t *nbr_delete;
{
    srcentry_t     *srcentry_ptr;
    srcentry_t     *srcentry_ptr_next;
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_srcs;
    grpentry_t     *grpentry_ptr;
    pim_nbr_entry_t *new_nbr;
    cand_rp_t      *cand_rp_ptr;
    rp_grp_entry_t *rp_grp_entry_ptr;
    rpentry_t      *rpentry_ptr;
    struct uvif    *v;

    v = &uvifs[nbr_delete->vifi];

    /* Delete the entry from the pim_nbrs chain */

    if (nbr_delete->prev != (pim_nbr_entry_t *) NULL)
	nbr_delete->prev->next = nbr_delete->next;
    else
	v->uv_pim_neighbors = nbr_delete->next;
    if (nbr_delete->next != (pim_nbr_entry_t *) NULL)
	nbr_delete->next->prev = nbr_delete->prev;

    return_jp6_working_buff(nbr_delete);

    if (v->uv_pim_neighbors == (pim_nbr_entry_t *) NULL)
    {
	/* This was our last neighbor. */
	v->uv_flags &= ~VIFF_PIM_NBR;
	v->uv_flags |= (VIFF_NONBRS | VIFF_DR);
    }
    else
    {
	if  (inet6_greaterthan(&v->uv_linklocal->pa_addr, 
			       &v->uv_pim_neighbors->address)) 
	    /*
	     * The first address is the new potential remote DR address, but
	     * the local address is the winner.
	     */
	    v->uv_flags |= VIFF_DR;
    }

    /* Update the source entries */
    for (srcentry_ptr = srclist; srcentry_ptr != (srcentry_t *) NULL;
	 srcentry_ptr = srcentry_ptr_next)
    {
	srcentry_ptr_next = srcentry_ptr->next;

	if (srcentry_ptr->upstream != nbr_delete)
	    continue;

	/* Reset the next hop (PIM) router */

	if (set_incoming(srcentry_ptr, PIM_IIF_SOURCE) == FALSE)
	{
	    /*
	     * Coudn't reset it. Sorry, the hext hop router toward that
	     * source is probably not a PIM router, or cannot find route at
	     * all, hence I cannot handle this source and have to delete it.
	     */
	    delete_srcentry(srcentry_ptr);
	}
	else
	    if (srcentry_ptr->upstream != (pim_nbr_entry_t *) NULL)
	    {
		/* Ignore the local or directly connected sources */
		/*
		 * Browse all MRT entries for this source and reset the
		 * upstream router. Note that the upstream router is not
		 * always toward the source: it could be toward the RP for
		 * example.
		 */
		new_nbr = srcentry_ptr->upstream;
		for (mrtentry_ptr = srcentry_ptr->mrtlink;
		     mrtentry_ptr != (mrtentry_t *) NULL;
		     mrtentry_ptr = mrtentry_ptr->srcnext)
		{
		    if (!(mrtentry_ptr->flags & MRTF_RP))
		    {
			mrtentry_ptr->upstream = srcentry_ptr->upstream;
			mrtentry_ptr->metric = srcentry_ptr->metric;
			mrtentry_ptr->preference = srcentry_ptr->preference;
			change_interfaces(mrtentry_ptr, srcentry_ptr->incoming,
					  &mrtentry_ptr->joined_oifs,
					  &mrtentry_ptr->pruned_oifs,
					  &mrtentry_ptr->leaves,
					  &mrtentry_ptr->asserted_oifs, 0);
		    }
		}
	    }
    }

    /* Update the RP entries */
    for (cand_rp_ptr = cand_rp_list; cand_rp_ptr != (cand_rp_t *) NULL;
	 cand_rp_ptr = cand_rp_ptr->next)
    {
	if (cand_rp_ptr->rpentry->upstream != nbr_delete)
	    continue;
	rpentry_ptr = cand_rp_ptr->rpentry;
	/* Reset the RP entry iif */
	/* TODO: check if error setting the iif! */
	if (local_address(&rpentry_ptr->address) == NO_VIF)
	{
		   set_incoming(rpentry_ptr, PIM_IIF_RP);
	}
	else
	{
	    rpentry_ptr->incoming = reg_vif_num;
	    rpentry_ptr->upstream = (pim_nbr_entry_t *) NULL;
	}
	mrtentry_ptr = rpentry_ptr->mrtlink;
	if (mrtentry_ptr != (mrtentry_t *) NULL)
	{
	    mrtentry_ptr->upstream = rpentry_ptr->upstream;
	    mrtentry_ptr->metric = rpentry_ptr->metric;
	    mrtentry_ptr->preference = rpentry_ptr->preference;
	    change_interfaces(mrtentry_ptr,
			      rpentry_ptr->incoming,
			      &mrtentry_ptr->joined_oifs,
			      &mrtentry_ptr->pruned_oifs,
			      &mrtentry_ptr->leaves,
			      &mrtentry_ptr->asserted_oifs, 0);
	}
	/* Update the group entries for this RP */
	for (rp_grp_entry_ptr = cand_rp_ptr->rp_grp_next;
	     rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
	     rp_grp_entry_ptr = rp_grp_entry_ptr->rp_grp_next)
	{
	    for (grpentry_ptr = rp_grp_entry_ptr->grplink;
		 grpentry_ptr != (grpentry_t *) NULL;
		 grpentry_ptr = grpentry_ptr->rpnext)
	    {
		mrtentry_ptr = grpentry_ptr->grp_route;
		if (mrtentry_ptr != (mrtentry_t *) NULL)
		{
		    mrtentry_ptr->upstream = rpentry_ptr->upstream;
		    mrtentry_ptr->metric = rpentry_ptr->metric;
		    mrtentry_ptr->preference = rpentry_ptr->preference;
		    change_interfaces(mrtentry_ptr,
				      rpentry_ptr->incoming,
				      &mrtentry_ptr->joined_oifs,
				      &mrtentry_ptr->pruned_oifs,
				      &mrtentry_ptr->leaves,
				      &mrtentry_ptr->asserted_oifs, 0);
		}
		/* Update only the (S,G)RPbit entries for this group */
		for (mrtentry_srcs = grpentry_ptr->mrtlink;
		     mrtentry_srcs != (mrtentry_t *) NULL;
		     mrtentry_srcs = mrtentry_srcs->grpnext)
		{
		    if (mrtentry_srcs->flags & MRTF_RP)
		    {
			mrtentry_ptr->upstream = rpentry_ptr->upstream;
			mrtentry_ptr->metric = rpentry_ptr->metric;
			mrtentry_ptr->preference = rpentry_ptr->preference;
			change_interfaces(mrtentry_srcs,
					  rpentry_ptr->incoming,
					  &mrtentry_srcs->joined_oifs,
					  &mrtentry_srcs->pruned_oifs,
					  &mrtentry_srcs->leaves,
					  &mrtentry_srcs->asserted_oifs, 0);
		    }
		}
	    }
	}
    }

    free((char *) nbr_delete);
}


/* TODO: simplify it! */
static int
parse_pim6_hello(pim_message, datalen, src, holdtime)
    char           	*pim_message;
    int             	datalen;
    struct sockaddr_in6 *src;
    u_int16        	*holdtime;
{
    u_int8         *pim_hello_message;
    u_int8         *data_ptr;
    u_int16         option_type;
    u_int16         option_length;
    int             holdtime_received_ok = FALSE;
    int             option_total_length;

    pim_hello_message = (u_int8 *) (pim_message + sizeof(struct pim));
    datalen -= sizeof(struct pim);
    for (; datalen >= sizeof(pim_hello_t);)
    {
	/* Ignore any data if shorter than (pim_hello header) */
	data_ptr = pim_hello_message;
	GET_HOSTSHORT(option_type, data_ptr);
	GET_HOSTSHORT(option_length, data_ptr);
	switch (option_type)
	{
	case PIM_MESSAGE_HELLO_HOLDTIME:
	    if (PIM_MESSAGE_HELLO_HOLDTIME_LENGTH != option_length)
	    {
		IF_DEBUG(DEBUG_PIM_HELLO)
		    log(LOG_DEBUG, 0,
		    "PIM HELLO Holdtime from %s: invalid OptionLength = %u",
			inet6_fmt(&src->sin6_addr), option_length);
		return (FALSE);
	    }
	    GET_HOSTSHORT(*holdtime, data_ptr);
	    holdtime_received_ok = TRUE;
	    break;
	default:
	    /* Ignore any unknown options */
	    break;
	}

	/* Move to the next option */
	/*
	 * XXX: TODO: If we are padding to the end of the 32 bit boundary,
	 * use the first method to move to the next option, otherwise simply
	 * (sizeof(pim_hello_t) + option_length).
	 */

#ifdef BOUNDARY_32_BIT
	option_total_length = (sizeof(pim_hello_t) + (option_length & ~0x3) +
			       ((option_length & 0x3) ? 4 : 0));
#else
	option_total_length = (sizeof(pim_hello_t) + option_length);
#endif				/* BOUNDARY_32_BIT */
	datalen -= option_total_length;
	pim_hello_message += option_total_length;
    }
    return (holdtime_received_ok);
}


int
send_pim6_hello(v, holdtime)
    struct uvif    *v;
    u_int16         holdtime;
{
    char           *buf;
    u_int8         *data_ptr;

    int             datalen;

    buf = pim6_send_buf + sizeof(struct pim);
    data_ptr = (u_int8 *) buf;
    PUT_HOSTSHORT(PIM_MESSAGE_HELLO_HOLDTIME, data_ptr);
    PUT_HOSTSHORT(PIM_MESSAGE_HELLO_HOLDTIME_LENGTH, data_ptr);
    PUT_HOSTSHORT(holdtime, data_ptr);

    datalen = data_ptr - (u_int8 *) buf;

    send_pim6(pim6_send_buf, &v->uv_linklocal->pa_addr,
	      &allpim6routers_group, PIM_HELLO, datalen);
    SET_TIMER(v->uv_pim_hello_timer, pim_hello_period);

    v->uv_out_pim6_hello++;
    return (TRUE);
}

/************************************************************************
 *                        PIM_REGISTER
 ************************************************************************/
/*
 * TODO: XXX: IF THE BORDER BIT IS SET, THEN FORWARD THE WHOLE PACKET FROM
 * USER SPACE AND AT THE SAME TIME IGNORE ANY CACHE_MISS SIGNALS FROM THE
 * KERNEL.
 */

int
receive_pim6_register(reg_src, reg_dst, pim_message, datalen)
    struct sockaddr_in6     *reg_src,
                    		*reg_dst;
    char           			*pim_message;
    int             		datalen;
{
    struct sockaddr_in6		inner_src,
                    		inner_grp;
    pim_register_t		*register_p;
    struct ip6_hdr    		*ip;
    u_int32         		borderBit,
                    		nullRegisterBit;
    mrtentry_t 			*mrtentry_ptr;
    mrtentry_t 			*mrtentry_ptr2;
    if_set			oifs;

    pim6dstat.in_pim6_register++;

    register_p = (pim_register_t *) (pim_message + sizeof(struct pim));

    borderBit = ntohl(register_p->reg_flags) & PIM_MESSAGE_REGISTER_BORDER_BIT;
    nullRegisterBit =
	ntohl(register_p->reg_flags) & PIM_MESSAGE_REGISTER_NULL_REGISTER_BIT;

    /* initialize the pointer to the encapsulated packet */
    ip = (struct ip6_hdr *) (register_p + 1);

    /*
     * We are keeping all addresses in network order,
     * so no need for byte order translation.
     */
    inner_src.sin6_addr = ip->ip6_src;
    inner_grp.sin6_addr = ip->ip6_dst;

    /* scope validation of the inner source and destination addresses */
    if (IN6_IS_ADDR_LINKLOCAL(&ip->ip6_src)) {
	    log(LOG_WARNING, 0,
		"receive_pim6_register: inner source(%s) has invalid scope",
		inet6_fmt(&ip->ip6_src));
    }
#ifdef notyet
    if (IN6_IS_ADDR_SITELOCAL)
	    inner_src.sin6_scope_id = addr2scopeid(&ip->ip6_src, &ip->ip6_dst,
						   reg_src, reg_dst);
    else
#endif
	    inner_src.sin6_scope_id = 0;

    if (IN6_IS_ADDR_MC_NODELOCAL(&ip->ip6_dst) ||
	IN6_IS_ADDR_MC_LINKLOCAL(&ip->ip6_dst)) {
	    log(LOG_WARNING, 0,
		"receive_pim6_register: inner group(%s) has invalid scope",
		inet6_fmt(&ip->ip6_dst));
	    return(FALSE);	/* XXX: can we discard it? */
    }
#ifdef notyet
    if (IN6_IS_ADDR_MC_SITELOCAL(&ip->ip6_dst))
	    inner_grp.sin6_scope_id = addr2scopeid(&ip->ip6_src, &ip->ip6_dst,
						   reg_src, reg_dst);
    else
	    inner_grp.sin6_scope_id = 0;
#endif
    inner_grp.sin6_scope_id = 0;

    mrtentry_ptr = find_route(&inner_src, &inner_grp,
			      MRTF_SG | MRTF_WC | MRTF_PMBR, DONT_CREATE);

    if (mrtentry_ptr == (mrtentry_t *) NULL)
    {

	/* No routing entry. Send REGISTER_STOP and return. */

	IF_DEBUG(DEBUG_PIM_REGISTER)
	    log(LOG_DEBUG, 0,
		"No routing entry for source %s and/or group %s",
		inet6_fmt(&inner_src.sin6_addr), inet6_fmt(&inner_grp.sin6_addr));

	/* TODO: XXX: shouldn't be inner_src=IN6ADDR_ANY? Not in the spec. */

	send_pim6_register_stop(reg_dst, reg_src, &inner_grp, &inner_src);
	return (TRUE);
    }

    /* XXX: not in the spec: check if I am the RP for that group */
	if (!inet6_equal(&my_cand_rp_address, reg_dst)
	|| (check_mrtentry_rp(mrtentry_ptr, &my_cand_rp_address) == FALSE))
    {
	send_pim6_register_stop(reg_dst, reg_src, &inner_grp, &inner_src);
	return (TRUE);
    }

    if (mrtentry_ptr->flags & MRTF_SG)
    {

	/* (S,G) found */
	/* TODO: check the timer again */

	SET_TIMER(mrtentry_ptr->timer, pim_data_timeout);	/* restart timer */
	if (!(mrtentry_ptr->flags & MRTF_SPT))
	{			
		/* The SPT bit is not set */

	    if (!nullRegisterBit)
	    {
		calc_oifs(mrtentry_ptr, &oifs);
		if (IF_ISEMPTY(&oifs)
		    && (mrtentry_ptr->incoming == reg_vif_num))
		{
		    send_pim6_register_stop(reg_dst, reg_src, &inner_grp,
					   				&inner_src);
		    return (TRUE);
		}

		/*
		 * TODO: XXX: BUG!!! The data will be forwarded by the kernel
		 * MFC!!! Need to set a special flag for this routing entry
		 * so after a cache miss occur, the multicast packet will be
		 * forwarded from user space and won't install entry in the
		 * kernel MFC. The problem is that the kernel MFC doesn't
		 * know the PMBR address and simply sets the multicast
		 * forwarding cache to accept/forward all data coming from
		 * the register_vif.
		 */

		if (borderBit)
		{
		    if (!inet6_equal(&mrtentry_ptr->pmbr_addr,reg_src))
		    {
			send_pim6_register_stop(reg_dst, reg_src,
					       			&inner_grp, &inner_src);
			return (TRUE);

		    }
		}
		return (TRUE);
	    }
	    /* TODO: XXX: if NULL_REGISTER and has (S,G) with SPT=0, then..? */
	    return (TRUE);
	}
	else
	{
	    /* The SPT bit is set */
	    send_pim6_register_stop(reg_dst, reg_src, &inner_grp, &inner_src);
	    return (TRUE);
	}
    }
    if (mrtentry_ptr->flags & (MRTF_WC | MRTF_PMBR))
    {
	if (borderBit)
	{
	    /*
	     * Create (S,G) state. The oifs will be the copied from the
	     * existing (*,G) or (*,*,RP) entry.
	     */

	    mrtentry_ptr2 = find_route(&inner_src, &inner_grp, MRTF_SG,
				       CREATE);
	    if (mrtentry_ptr2 != (mrtentry_t *) NULL)
	    {
		mrtentry_ptr2->pmbr_addr = *reg_src;

		/* Clear the SPT flag */

		mrtentry_ptr2->flags &= ~(MRTF_SPT | MRTF_NEW);
		SET_TIMER(mrtentry_ptr2->timer, pim_data_timeout);

		/* TODO: explicitly call the Join/Prune send function? */

		FIRE_TIMER(mrtentry_ptr2->jp_timer);	/* Send the Join
							 * immediately */
		/*
		 * TODO: explicitly call this function?
		 * send_pim6_join_prune(mrtentry_ptr2->upstream->vifi,
		 * mrtentry_ptr2->upstream, pim_join_prune_holdtime);
		 */
	    }
	}
    }

    if (mrtentry_ptr->flags & MRTF_WC)
    {
	/* (*,G) entry */

	calc_oifs(mrtentry_ptr, &oifs);
	if (IF_ISEMPTY(&oifs))
	{
	    send_pim6_register_stop(reg_dst, reg_src, &inner_grp, &sockaddr6_any);
	    return (FALSE);
	}

	/* XXX: TODO: check with the spec again */

	else
	{
	    if (!nullRegisterBit)
	    {
		/* Install cache entry in the kernel */
		/*
		 * TODO: XXX: probably redundant here, because the
		 * decapsulated mcast packet in the kernel will result in
		 * CACHE_MISS
		 */

		struct sockaddr_in6    *mfc_source = &inner_src;


#ifdef KERNEL_MFC_WC_G
		if (!(mrtentry_ptr->flags & MRTF_MFC_CLONE_SG))
		    mfc_source = NULL;
#endif				/* KERNEL_MFC_WC_G */

		add_kernel_cache(mrtentry_ptr, mfc_source, &inner_grp, 0);
		k_chg_mfc(mld6_socket, mfc_source, &inner_grp,
			  mrtentry_ptr->incoming, &mrtentry_ptr->oifs,
			  &mrtentry_ptr->group->rpaddr);
		return (TRUE);
	    }
	}
	return (TRUE);
    }

    if (mrtentry_ptr->flags & MRTF_PMBR)
    {
	/* (*,*,RP) entry */
	if (!nullRegisterBit)
	{
	    struct sockaddr_in6		*mfc_source = &inner_src;

	    /*
	     * XXX: have to create either (S,G) or (*,G). The choice below is
	     * (*,G)
	     */

	    mrtentry_ptr2 = find_route(NULL, &inner_grp, MRTF_WC,
				       CREATE);
	    if (mrtentry_ptr2 == (mrtentry_t *) NULL)
		return (FALSE);
	    if (mrtentry_ptr2->flags & MRTF_NEW)
	    {
		/* TODO: something else? Have the feeling sth is missing */

		mrtentry_ptr2->flags &= ~MRTF_NEW;

		/* TODO: XXX: copy the timer from the (*,*,RP) entry? */

		COPY_TIMER(mrtentry_ptr->timer, mrtentry_ptr2->timer);
	    }
	    /* Install cache entry in the kernel */

#ifdef KERNEL_MFC_WC_G

	    if (!(mrtentry_ptr->flags & MRTF_MFC_CLONE_SG))
		mfc_source = NULL;
#endif				/* KERNEL_MFC_WC_G */
	    add_kernel_cache(mrtentry_ptr, mfc_source, &inner_grp, 0);
	    k_chg_mfc(mld6_socket, mfc_source, &inner_grp,
		      mrtentry_ptr->incoming, &mrtentry_ptr->oifs,
		      &mrtentry_ptr2->group->rpaddr);

	    return (TRUE);
	}
    }

    /* Shoudn't happen: invalid routing entry? */
    /* XXX: TODO: shoudn't be inner_src=IN6ADDR_ANY? Not in the spec. */

    send_pim6_register_stop(reg_dst, reg_src, &inner_grp, &inner_src);
    return (TRUE);
}


int
send_pim6_register(pkt)
    char *pkt;
{
    register struct ip6_hdr *ip6;
    static struct sockaddr_in6		source= {sizeof(source) , AF_INET6 };
    static struct sockaddr_in6 		group= {sizeof(group) , AF_INET6 };
    mifi_t          		mifi;
    rpentry_t      		*rpentry_ptr;
    mrtentry_t     		*mrtentry_ptr;
    mrtentry_t     		*mrtentry_ptr2;

    struct sockaddr_in6		*reg_src,
                    		*reg_dst;
    int             		pktlen = 0;
    char           		*buf;

    ip6=(struct ip6_hdr *)pkt;
	
    group.sin6_addr = ip6->ip6_dst;
    source.sin6_addr = ip6->ip6_src;

    if ((mifi = find_vif_direct_local(&source)) == NO_VIF)
	return (FALSE);

    if (!(uvifs[mifi].uv_flags & VIFF_DR))
	return (FALSE);		/* I am not the DR for that subnet */



    rpentry_ptr = rp_match(&group);
    if (rpentry_ptr == (rpentry_t *) NULL)
	return (FALSE);		/* No RP for this group */
    if (local_address(&rpentry_ptr->address) != NO_VIF)
	/* TODO: XXX: not sure it is working! */
	return (FALSE);		/* I am the RP for this group */

    mrtentry_ptr = find_route(&source, &group, MRTF_SG, CREATE);
    if (mrtentry_ptr == (mrtentry_t *) NULL)
	return (FALSE);		/* Cannot create (S,G) state */

    if (mrtentry_ptr->flags & MRTF_NEW)
    {
	/* A new entry */
	mrtentry_ptr->flags &= ~MRTF_NEW;
	RESET_TIMER(mrtentry_ptr->rs_timer);	/* Reset the
						 * Register-Suppression timer */
	if ((mrtentry_ptr2 = mrtentry_ptr->group->grp_route) ==
	    (mrtentry_t *) NULL)
	    mrtentry_ptr2 =
		mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_ptr2 != (mrtentry_t *) NULL)
	{
	    FIRE_TIMER(mrtentry_ptr2->jp_timer);	/* Timeout the
							 * Join/Prune timer */
	    /*
	     * TODO: explicitly call this function?
	     * send_pim6_join_prune(mrtentry_ptr2->upstream->vifi,
	     * mrtentry_ptr2->upstream, pim_join_prune_holdtime);
	     */
	}
    }
    /* Restart the (S,G) Entry-timer */

    SET_TIMER(mrtentry_ptr->timer, pim_data_timeout);

    IF_TIMER_NOT_SET(mrtentry_ptr->rs_timer)
    {
	/*
	 * The Register-Suppression Timer is not running. Encapsulate the
	 * data and send to the RP.
	 */
	buf = pim6_send_buf + sizeof(struct pim);

	bzero(buf, sizeof(pim_register_t));	/* No flags set */
	buf += sizeof(pim_register_t);

	/* Copy the data packet at the back of the register packet */
	/* TODO: check pktlen. ntohs? */

	pktlen = ntohs(ip6->ip6_plen);
	pktlen +=sizeof(struct ip6_hdr);			/* XXX */

	bcopy((char *) ip6, buf, pktlen);
	pktlen += sizeof(pim_register_t);
	reg_src = max_global_address();
	reg_dst = &mrtentry_ptr->group->rpaddr;

    	send_pim6(pim6_send_buf, reg_src , reg_dst , PIM_REGISTER,
	     	  pktlen);

	pim6dstat.out_pim6_register++;

	return (TRUE);
    }
    return (TRUE);
}

int
send_pim6_null_register(mrtentry_ptr)
    mrtentry_t     *mrtentry_ptr;
{
	struct 				ip6_hdr *ip;
    pim_register_t 		*pim_register;
    int             		pktlen = 0;
    mifi_t          		mifi;
    struct sockaddr_in6  	*reg_source,
                    		*dest;

    /* No directly connected source; no local address */

    if ((mifi = find_vif_direct_local(&mrtentry_ptr->source->address)) == NO_VIF)
	return (FALSE);

    pim_register = (pim_register_t *) (pim6_send_buf  + sizeof(struct pim));
    bzero((char *) pim_register, sizeof(pim_register_t));
    pim_register->reg_flags = htonl(pim_register->reg_flags
				  | PIM_MESSAGE_REGISTER_NULL_REGISTER_BIT);

    /* include the dummy ip header */
    ip = (struct ip6_hdr *) (pim_register +1);
    ip->ip6_plen= 0;
    ip->ip6_flow=0;
    ip->ip6_vfc = 0x60;
    ip->ip6_hlim = MINHLIM;
    ip->ip6_nxt = IPPROTO_NONE;
    ip->ip6_src = mrtentry_ptr->source->address.sin6_addr;
    ip->ip6_dst = mrtentry_ptr->group->group.sin6_addr;

    pktlen = sizeof(pim_register_t) + sizeof(struct ip6_hdr);

    dest = &mrtentry_ptr->group->rpaddr;
    reg_source = max_global_address();
    
    send_pim6(pim6_send_buf, reg_source , dest, PIM_REGISTER,
	      pktlen);
    pim6dstat.out_pim6_register++; /* should be counted separately? */

    return (TRUE);
}


/************************************************************************
 *                        PIM_REGISTER_STOP
 ************************************************************************/
int
receive_pim6_register_stop(reg_src, reg_dst, pim_message, datalen)
    struct sockaddr_in6 	*reg_src,
                    		*reg_dst;
    char           		*pim_message;
    register int    		datalen;
{
    pim_register_stop_t 	*pim_regstop_p;
    pim6_encod_grp_addr_t 	encod_grp;
    pim6_encod_uni_addr_t 	encod_unisrc;
    struct sockaddr_in6		source,
							group;
    u_int8         		*data_ptr;
    mrtentry_t     		*mrtentry_ptr;
    if_set     			pruned_oifs;
    mifi_t			mifi;
    struct uvif 		*v;

    pim6dstat.in_pim6_register_stop++;

    pim_regstop_p = (pim_register_stop_t *) (pim_message +
					     sizeof(struct pim));
    data_ptr = (u_int8 *) & pim_regstop_p->encod_grp;
    GET_EGADDR6(&encod_grp, data_ptr);
    GET_EUADDR6(&encod_unisrc, data_ptr);

    group.sin6_addr = encod_grp.mcast_addr;

    /* scope validation of the inner source and destination addresses */

#ifdef notyet
    if (IN6_IS_ADDR_MC_SITELOCAL(&ip->ip6_dst))
	    group.sin6_scope_id = addr2scopeid(&ip->ip6_src, &ip->ip6_dst,
						   reg_src, reg_dst);
    else
#endif

    group.sin6_scope_id = 0;
    source.sin6_addr = encod_unisrc.unicast_addr;

    /* the source address must be global...but is it always true? */

#ifdef notyet
    if (IN6_IS_ADDR_SITELOCAL)
	    source.sin6_scope_id = addr2scopeid(&ip->ip6_src, &ip->ip6_dst,
						   reg_src, reg_dst);
    else
#endif

    source.sin6_scope_id = 0;

    if((mifi= find_vif_direct_local(&source))==NO_VIF)
    {
	    IF_DEBUG(DEBUG_PIM_REGISTER)
		    {
			    log(LOG_WARNING,0,
				"Received PIM_REGISTER_STOP from RP %s for a non "
				"direct-connect source %s",
				inet6_fmt(&reg_src->sin6_addr),
				inet6_fmt(&encod_unisrc.unicast_addr));
		    }
	    return FALSE;
    }		

    v=&uvifs[mifi];	


    group.sin6_scope_id = inet6_uvif2scopeid(&group, v);
    source.sin6_scope_id = inet6_uvif2scopeid(&source,
					      v);
	

    IF_DEBUG(DEBUG_PIM_REGISTER)
	{
		log(LOG_DEBUG, 0,
		    "Received PIM_REGISTER_STOP from RP %s to %s "
		    "source : %s group : %s",
		    inet6_fmt(&reg_src->sin6_addr),
		    inet6_fmt(&reg_dst->sin6_addr),
		    inet6_fmt(&encod_unisrc.unicast_addr),
		    inet6_fmt(&encod_grp.mcast_addr));
	}

    /* TODO: apply the group mask and do register_stop for all grp addresses */
    /* TODO: check for SourceAddress == 0 */


    mrtentry_ptr = find_route(&source, &group,
			      MRTF_SG, DONT_CREATE);
    if (mrtentry_ptr == (mrtentry_t *) NULL)
    {
	return (FALSE);
    }

    /*
     * XXX: not in the spec: check if the PIM_REGISTER_STOP originator is
     * really the RP
     */


    if (check_mrtentry_rp(mrtentry_ptr, reg_src) == FALSE)
    {
	return (FALSE);
    }

    /* restart the Register-Suppression timer */
 
    SET_TIMER(mrtentry_ptr->rs_timer, (0.5 * pim_register_suppression_timeout)
	      + (RANDOM() % (pim_register_suppression_timeout + 1)));
    /* Prune the register_vif from the outgoing list */

    IF_COPY(&mrtentry_ptr->pruned_oifs, &pruned_oifs);
    IF_SET(reg_vif_num, &pruned_oifs);
    change_interfaces(mrtentry_ptr, mrtentry_ptr->incoming,
		      &mrtentry_ptr->joined_oifs, &pruned_oifs,
		      &mrtentry_ptr->leaves,
		      &mrtentry_ptr->asserted_oifs, 0);
    return (TRUE);
}


/* TODO: optional rate limiting is not implemented yet */
/* Unicasts a REGISTER_STOP message to the DR */

static int
send_pim6_register_stop(reg_src, reg_dst, inner_grp, inner_src)
    struct sockaddr_in6		*reg_src,
                    		*reg_dst,
                    		*inner_grp,
                    		*inner_src;
{
    char           *buf;
    u_int8         *data_ptr;

    buf = pim6_send_buf +  sizeof(struct pim);
    data_ptr = (u_int8 *) buf;
    PUT_EGADDR6(inner_grp->sin6_addr, SINGLE_GRP_MSK6LEN, 0, data_ptr);
    PUT_EUADDR6(inner_src->sin6_addr, data_ptr);

    send_pim6(pim6_send_buf, reg_src , reg_dst , PIM_REGISTER_STOP,
	      data_ptr-(u_int8 *) buf);
    pim6dstat.out_pim6_register_stop++;
 
    return (TRUE);
}

/************************************************************************
 *                        PIM_JOIN_PRUNE
 ************************************************************************/
int
join_or_prune(mrtentry_ptr, upstream_router)
    mrtentry_t     *mrtentry_ptr;
    pim_nbr_entry_t *upstream_router;
{
    if_set     		entry_oifs;
    mrtentry_t     	*mrtentry_grp;

    if ((mrtentry_ptr == (mrtentry_t *) NULL))
    {
	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	    log(LOG_DEBUG,0,"Join_or_prune : mrtentry_ptr is null");
	return (PIM_ACTION_NOTHING);
    }
    if( upstream_router == (pim_nbr_entry_t *) NULL)
    {
	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	    log(LOG_DEBUG,0,"Join_or_prune : upstream_router is null");
	return (PIM_ACTION_NOTHING);
    }

    calc_oifs(mrtentry_ptr, &entry_oifs);
    if (mrtentry_ptr->flags & (MRTF_PMBR | MRTF_WC))
    {
	/* (*,*,RP) or (*,G) entry */
	/* The (*,*,RP) or (*,G) J/P messages are sent only toward the RP */

	if (upstream_router != mrtentry_ptr->upstream)
	    return (PIM_ACTION_NOTHING);

	/* TODO: XXX: Can we have (*,*,RP) prune? */

	if (IF_ISEMPTY(&entry_oifs))
	{
	    /* NULL oifs */
	    if (!(uvifs[mrtentry_ptr->incoming].uv_flags & VIFF_DR))
	    {
		/* I am not the DR for that subnet. */
		return (PIM_ACTION_PRUNE);
            }	
	    if (IF_ISSET(mrtentry_ptr->incoming, &mrtentry_ptr->leaves))
		/* I am the DR and have local leaves */
		return (PIM_ACTION_JOIN);
	    /* Probably the last local member hast timeout */
	    return (PIM_ACTION_PRUNE);
	}
	return (PIM_ACTION_JOIN);
    }

    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/* (S,G) entry */
	/* TODO: check again */
	if (mrtentry_ptr->upstream == upstream_router)
	{
	    if (!(mrtentry_ptr->flags & MRTF_RP))
	    {
		/* Upstream router toward S */
		if (IF_ISEMPTY(&entry_oifs))
		{
		    if (mrtentry_ptr->group->active_rp_grp != (rp_grp_entry_t *)NULL &&
			inet6_equal(&mrtentry_ptr->group->rpaddr,
				    &my_cand_rp_address))
		    {
			/*
			 * (S,G) at the RP. Don't send Join/Prune (see the
			 * end of Section 3.3.2)
			 */

			return (PIM_ACTION_NOTHING);
		    }
		    return (PIM_ACTION_PRUNE);
		}
		else
		    return (PIM_ACTION_JOIN);
	    }
	    else
	    {
		/* Upstream router toward RP */
		if (IF_ISEMPTY(&entry_oifs))
		    return (PIM_ACTION_PRUNE);
	    }
	}

	/*
	 * Looks like the case when the upstream router toward S is different
	 * from the upstream router toward RP
	 */

	if (mrtentry_ptr->group->active_rp_grp == (rp_grp_entry_t *) NULL)
	    return (PIM_ACTION_NOTHING);
	mrtentry_grp = mrtentry_ptr->group->grp_route;
	if (mrtentry_grp == (mrtentry_t *) NULL)
	    mrtentry_grp =
		mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_grp == (mrtentry_t *) NULL)
	    return (PIM_ACTION_NOTHING);
	if (mrtentry_grp->upstream != upstream_router)
	    return (PIM_ACTION_NOTHING);	/* XXX: shoudn't happen */

	if ((!(mrtentry_ptr->flags & MRTF_RP))
	    && (mrtentry_ptr->flags & MRTF_SPT))
	{
	    return (PIM_ACTION_PRUNE);
	}
    }
    return (PIM_ACTION_NOTHING);
}

/* TODO: too long, simplify it! */
#define PIM6_JOIN_PRUNE_MINLEN (4 + PIM6_ENCODE_UNI_ADDR_LEN + 4)

int
receive_pim6_join_prune(src, dst, pim_message, datalen)
    struct sockaddr_in6		*src,
                    		*dst;
    char           		*pim_message;
    register int    		datalen;
{
    mifi_t          		mifi;
    struct uvif    		*v;
    pim6_encod_uni_addr_t 	uni_target_addr;
    pim6_encod_grp_addr_t 	encod_group;
    pim6_encod_src_addr_t 	encod_src;
    u_int8         		*data_ptr;
    u_int8         		*data_ptr_start;
    u_int8          		num_groups;
    u_int8          		num_groups_tmp;
    int             		star_star_rp_found;
    u_int16         		holdtime;
    u_int16         		num_j_srcs;
    u_int16         		num_j_srcs_tmp;
    u_int16         		num_p_srcs;
    struct sockaddr_in6		source;
    struct sockaddr_in6         group;
    struct sockaddr_in6		target;
    struct in6_addr          	s_mask;
    struct in6_addr         	g_mask;
    u_int8          		s_flags;
    u_int8          		reserved;
    rpentry_t      		*rpentry_ptr;
    mrtentry_t     		*mrtentry_ptr;
    mrtentry_t     		*mrtentry_srcs;
    mrtentry_t     		*mrtentry_rp;
    grpentry_t     		*grpentry_ptr;
    u_int16         		jp_value;
    pim_nbr_entry_t 		*upstream_router;
    int             		my_action;
    int             		ignore_group;
    rp_grp_entry_t 		*rp_grp_entry_ptr;
    u_int8         		*data_ptr_group_j_start;
    u_int8         		*data_ptr_group_p_start;

    if ((mifi = find_vif_direct(src)) == NO_VIF)
    {
	/*
	 * Either a local vif or somehow received PIM_JOIN_PRUNE from
	 * non-directly connected router. Ignore it.
	 */

	if (local_address(src) == NO_VIF)
	    log(LOG_INFO, 0,
		"Ignoring PIM_JOIN_PRUNE from non-neighbor router %s",
		inet6_fmt(&src->sin6_addr));
	return (FALSE);
    }

    v = &uvifs[mifi];
    v->uv_in_pim6_join_prune++;
    if (uvifs[mifi].uv_flags &
	(VIFF_DOWN | VIFF_DISABLED | VIFF_NONBRS | MIFF_REGISTER))
    {
	return (FALSE);		/* Shoudn't come on this interface */
    }

    /* sanity check for the minimum length */
    if (datalen < PIM6_JOIN_PRUNE_MINLEN) {
	    log(LOG_NOTICE, 0,
		"receive_pim6_join_prune: Join/Prune message size(%u) is"
		" too short from %s on %s",
		datalen, inet6_fmt(&src->sin6_addr), v->uv_name);
	    return(FALSE);
    }
    datalen -= PIM6_JOIN_PRUNE_MINLEN;
    data_ptr = (u_int8 *) (pim_message + sizeof(struct pim));

    /* Get the target address */
    GET_EUADDR6(&uni_target_addr, data_ptr);
    GET_BYTE(reserved, data_ptr);
    GET_BYTE(num_groups, data_ptr);
    if (num_groups == 0)
	return (FALSE);		/* No indication for groups in the message */
    GET_HOSTSHORT(holdtime, data_ptr);
    target.sin6_len = sizeof(target);
    target.sin6_family = AF_INET6;
    target.sin6_addr = uni_target_addr.unicast_addr;
    target.sin6_scope_id = inet6_uvif2scopeid(&target, v);

    /* Sanity check for the message length through all the groups */
    num_groups_tmp = num_groups;
    data_ptr_start = data_ptr;
    while (num_groups_tmp--) {
	int srclen;

	/* group addr + #join + #src */
	if (datalen < PIM6_ENCODE_GRP_ADDR_LEN + sizeof(u_int32_t)) {
	    log(LOG_NOTICE, 0,
		"receive_pim6_join_prune: Join/Prune message from %s on %s is"
		" too short to contain enough data",
		inet6_fmt(&src->sin6_addr), v->uv_name);
	    return(FALSE);
	}
	datalen -= (PIM6_ENCODE_GRP_ADDR_LEN + sizeof(u_int32_t));
	data_ptr += PIM6_ENCODE_GRP_ADDR_LEN;

	/* joined source addresses and pruned source addresses */
	GET_HOSTSHORT(num_j_srcs, data_ptr);
	GET_HOSTSHORT(num_p_srcs, data_ptr);
	srclen = (num_j_srcs + num_p_srcs) * PIM6_ENCODE_SRC_ADDR_LEN;
	if (datalen < srclen) {
	    log(LOG_NOTICE, 0,
		"receive_pim6_join_prune: Join/Prune message from %s on %s is"
		" too short to contain enough data",
		inet6_fmt(&src->sin6_addr), v->uv_name);
	    return(FALSE);
	}
	datalen -= srclen;
	data_ptr += srclen;
    }
    data_ptr = data_ptr_start;
    num_groups_tmp = num_groups;

    if (!inet6_localif_address(&target, v) &&
	!IN6_IS_ADDR_UNSPECIFIED(&uni_target_addr.unicast_addr))
    {

	/* if I am not the targer of the join message */
	/*
	 * Join/Prune suppression code. This either modifies the J/P timers
	 * or triggers an overriding Join.
	 */
	/*
	 * Note that if we have (S,G) prune and (*,G) Join, we must send them
	 * in the same message. We don't bother to modify both timers here.
	 * The Join/Prune sending function will take care of that.
	 */

	upstream_router = find_pim6_nbr(&target);
	if (upstream_router == (pim_nbr_entry_t *) NULL)
	    return (FALSE);	/* I have no such neighbor */

	group.sin6_len = sizeof(group);
	group.sin6_family = AF_INET6;
	source.sin6_len = sizeof(source);
	source.sin6_family = AF_INET6;

	while (num_groups--)
	{
	    GET_EGADDR6(&encod_group, data_ptr);
	    GET_HOSTSHORT(num_j_srcs, data_ptr);
	    GET_HOSTSHORT(num_p_srcs, data_ptr);
	    if (encod_group.masklen > (sizeof(struct in6_addr) << 3))
		continue;	
	    MASKLEN_TO_MASK6(encod_group.masklen, g_mask);
	    group.sin6_addr = encod_group.mcast_addr;
	    group.sin6_scope_id = inet6_uvif2scopeid(&group, v);
	   
	    if (!IN6_IS_ADDR_MULTICAST(&group.sin6_addr)) 
	    {	
		data_ptr +=
		    (num_j_srcs + num_p_srcs) * sizeof(pim6_encod_src_addr_t);
		continue;	/* Ignore this group and jump to the next */
	    }

	    if (inet6_equal(&group,&sockaddr6_d) &&
		(encod_src.masklen == STAR_STAR_RP_MSK6LEN))
	    {
		/* (*,*,RP) Join suppression */

		while (num_j_srcs--)
		{
		    GET_ESADDR6(&encod_src, data_ptr);
		    source.sin6_addr = encod_src.src_addr;
		    source.sin6_scope_id = inet6_uvif2scopeid(&source,
                                      v);
			/* sanity checks */
		    if (!inet6_valid_host(&source))
				continue;
		    if (encod_src.masklen >
                    (sizeof(struct in6_addr) << 3))
                    continue;
 

	   	    s_flags = encod_src.flags;
		    MASKLEN_TO_MASK6(encod_src.masklen, s_mask);
		    if ((s_flags & USADDR_RP_BIT) &&
			(s_flags & USADDR_WC_BIT))
		    {
			/* This is the RP address. */
			rpentry_ptr = rp_find(&source);
			if (rpentry_ptr == (rpentry_t *) NULL)
			    continue;	/* Don't have such RP. Ignore */
			mrtentry_rp = rpentry_ptr->mrtlink;
			my_action = join_or_prune(mrtentry_rp,
						  upstream_router);
			if (my_action != PIM_ACTION_JOIN)
			    continue;

			/* Check the holdtime */
			/* TODO: XXX: TIMER implem. dependency! */

			if (mrtentry_rp->jp_timer > holdtime)
			    continue;
			if ((mrtentry_rp->jp_timer == holdtime)
			    && (inet6_greaterthan(src, &v->uv_linklocal->pa_addr)))
			    continue;

			/*
			 * Set the Join/Prune suppression timer for this
			 * routing entry by increasing the current Join/Prune
			 * timer.
			 */
			jp_value = pim_join_prune_period +
			    0.5 * (RANDOM() % pim_join_prune_period);
			/* TODO: XXX: TIMER implem. dependency! */

			if (mrtentry_rp->jp_timer < jp_value)
			    SET_TIMER(mrtentry_rp->jp_timer, jp_value);
		    }
		}		/* num_j_srcs */

		while (num_p_srcs--)
		{
		    /*
		     * TODO: XXX: Can we have (*,*,RP) prune message? Not in
		     * the spec, but anyway, the code below can handle them:
		     * either suppress the local (*,*,RP) prunes or override
		     * the prunes by sending (*,*,RP) and/or (*,G) and/or
		     * (S,G) Join.
		     */

		    GET_ESADDR6(&encod_src, data_ptr);
		    source.sin6_addr = encod_src.src_addr;
		    source.sin6_scope_id = inet6_uvif2scopeid(&source,
                                      v);
 
		    if (!inet6_valid_host(&source))
			continue;

                    if (encod_src.masklen >
                    (sizeof(struct in6_addr) << 3))
                    continue;


		    s_flags = encod_src.flags;
		    MASKLEN_TO_MASK6(encod_src.masklen, s_mask);
		    if ((s_flags & USADDR_RP_BIT) &&
			(s_flags & USADDR_WC_BIT))
		    {
			/* This is the RP address. */
			rpentry_ptr = rp_find(&source);
			if (rpentry_ptr == (rpentry_t *) NULL)
			    continue;	/* Don't have such RP. Ignore */
			mrtentry_rp = rpentry_ptr->mrtlink;
			my_action = join_or_prune(mrtentry_rp,
						  upstream_router);
			if (my_action == PIM_ACTION_PRUNE)
			{
			    /* TODO: XXX: TIMER implem. dependency! */
			    if ((mrtentry_rp->jp_timer < holdtime)
				|| ((mrtentry_rp->jp_timer == holdtime)
			    	&& (inet6_greaterthan(src, &v->uv_linklocal->pa_addr))))
			    {
				/* Suppress the Prune */
				jp_value =  pim_join_prune_period+
				    0.5 * (RANDOM() % pim_join_prune_period);
				if (mrtentry_rp->jp_timer < jp_value)
				    SET_TIMER(mrtentry_rp->jp_timer, jp_value);
			    }
			}
			else
			    if (my_action == PIM_ACTION_JOIN)
			    {
				/* Override the Prune by scheduling a Join */
				jp_value = (RANDOM() % 11) / (10 * PIM_RANDOM_DELAY_JOIN_TIMEOUT);
				/* TODO: XXX: TIMER implem. dependency! */
				if (mrtentry_rp->jp_timer > jp_value)
				    SET_TIMER(mrtentry_rp->jp_timer, jp_value);
			    }
			/*
			 * Check all (*,G) and (S,G) matching to this RP. If
			 * my_action == JOIN, then send a Join and override
			 * the (*,*,RP) Prune.
			 */
			for (grpentry_ptr =
			     rpentry_ptr->cand_rp->rp_grp_next->grplink;
			     grpentry_ptr != (grpentry_t *) NULL;
			     grpentry_ptr = grpentry_ptr->rpnext)
			{
			    my_action = join_or_prune(grpentry_ptr->grp_route,
						      upstream_router);
			    if (my_action == PIM_ACTION_JOIN)
			    {

				jp_value = (RANDOM() % 11) / (10 * PIM_RANDOM_DELAY_JOIN_TIMEOUT);
				/* TODO: XXX: TIMER implem. dependency! */
				if (grpentry_ptr->grp_route->jp_timer >
				    jp_value)
				    SET_TIMER(grpentry_ptr->grp_route->jp_timer, jp_value);
			    }
			    for (mrtentry_srcs = grpentry_ptr->mrtlink;
				 mrtentry_srcs != (mrtentry_t *) NULL;
				 mrtentry_srcs = mrtentry_srcs->grpnext)
			    {
				my_action = join_or_prune(mrtentry_srcs,
							  upstream_router);
				if (my_action == PIM_ACTION_JOIN)
				{

				    jp_value = (RANDOM() % 11) / (10 * PIM_RANDOM_DELAY_JOIN_TIMEOUT);
				    /* TODO: XXX: TIMER implem. dependency! */
				    if (mrtentry_srcs->jp_timer > jp_value)
					SET_TIMER(mrtentry_srcs->jp_timer, jp_value);
				}
			    }	/* For all (S,G) */
			}	/* For all (*,G) */
		    }
		}		/* num_p_srcs */
		continue;	/* This was (*,*,RP) suppression */
	    }

	    /* (*,G) or (S,G) suppression */
	    /*
	     * TODO: XXX: currently, accumulated groups (i.e. group_masklen <
	     * group_address_lengt) are not implemented. Just need to create
	     * a loop and apply the procedure below for all groups matching
	     * the prefix.
	     */

	    while (num_j_srcs--)
	    {
		GET_ESADDR6(&encod_src, data_ptr);
		source.sin6_addr  = encod_src.src_addr;
	        source.sin6_scope_id = inet6_uvif2scopeid(&source,v);

		if (!inet6_valid_host(&source))
		    continue;
	        if (encod_src.masklen >
                (sizeof(struct in6_addr) << 3))
                continue;


		s_flags = encod_src.flags;
		MASKLEN_TO_MASK6(encod_src.masklen, s_mask);

		if ((s_flags & USADDR_RP_BIT) && (s_flags & USADDR_WC_BIT))
		{
		    /* (*,G) JOIN_REQUEST (toward the RP) */
		    mrtentry_ptr = find_route(&sockaddr6_any , &group, MRTF_WC,
					      DONT_CREATE);
		    my_action = join_or_prune(mrtentry_ptr, upstream_router);
		    if (my_action != PIM_ACTION_JOIN)
			continue;
		    /* (*,G) Join suppresion */
		    if (!inet6_equal(&source,&mrtentry_ptr->group->active_rp_grp->rp->rpentry->address))
			continue;	/* The RP address doesn't match.
					 * Ignore. */

		    /* Check the holdtime */
		    /* TODO: XXX: TIMER implem. dependency! */
		    if (mrtentry_ptr->jp_timer > holdtime)
			continue;
		    if ((mrtentry_ptr->jp_timer == holdtime)
		    && (inet6_greaterthan(src, &v->uv_linklocal->pa_addr)))
			continue;
		    jp_value = pim_join_prune_period +
			0.5 * (RANDOM() % pim_join_prune_period);
		    if (mrtentry_ptr->jp_timer < jp_value)
			SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
		    continue;
		}		/* End of (*,G) Join suppression */

		/* (S,G) Join suppresion */
		mrtentry_ptr = find_route(&source, &group, MRTF_SG,
					  DONT_CREATE);
		my_action = join_or_prune(mrtentry_ptr, upstream_router);
		if (my_action != PIM_ACTION_JOIN)
		    continue;

		/* Check the holdtime */
		/* TODO: XXX: TIMER implem. dependency! */
		if (mrtentry_ptr->jp_timer > holdtime)
		    continue;
		if ((mrtentry_ptr->jp_timer == holdtime)
		    && (inet6_greaterthan(src, &v->uv_linklocal->pa_addr)))
		    continue;
		jp_value = pim_join_prune_period +
		    0.5 * (RANDOM() % pim_join_prune_period);
		if (mrtentry_ptr->jp_timer < jp_value)
		    SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
		continue;
	    }

	    /* Prunes suppression */
	    while (num_p_srcs--)
	    {
		GET_ESADDR6(&encod_src, data_ptr);
		source.sin6_addr = encod_src.src_addr;
	        source.sin6_scope_id = inet6_uvif2scopeid(&source,
                                 v);
	        if (encod_src.masklen >
                (sizeof(struct in6_addr) << 3))
                continue;


		if (!inet6_valid_host(&source))
		    continue;
		s_flags = encod_src.flags;
		MASKLEN_TO_MASK6(encod_src.masklen, s_mask);
		if ((s_flags & USADDR_RP_BIT) && (s_flags & USADDR_WC_BIT))
		{
		    /* (*,G) prune suppression */
		    rpentry_ptr = rp_match(&source);
		    if ((rpentry_ptr == (rpentry_t *) NULL)
			|| (!inet6_equal(&rpentry_ptr->address , &source)))
			continue;	/* No such RP or it is different.
					 * Ignore */
		    mrtentry_ptr = find_route(&sockaddr6_any, &group, MRTF_WC,
					      DONT_CREATE);
		    my_action = join_or_prune(mrtentry_ptr, upstream_router);
		    if (my_action == PIM_ACTION_PRUNE)
		    {
			/* TODO: XXX: TIMER implem. dependency! */
			if ((mrtentry_ptr->jp_timer < holdtime)
			    || ((mrtentry_ptr->jp_timer == holdtime)
		    		&& (inet6_greaterthan(src, &v->uv_linklocal->pa_addr))))
			{
			    /* Suppress the Prune */
			    jp_value = pim_join_prune_period +
				0.5 * (RANDOM() % pim_join_prune_period);
			    if (mrtentry_ptr->jp_timer < jp_value)
				SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
			}
		    }
		    else
			if (my_action == PIM_ACTION_JOIN)
			{
			    /* Override the Prune by scheduling a Join */
			    jp_value = (RANDOM() % 11) / (10 * PIM_RANDOM_DELAY_JOIN_TIMEOUT);
			    /* TODO: XXX: TIMER implem. dependency! */
			    if (mrtentry_ptr->jp_timer > jp_value)
				SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
			}

		    /*
		     * Check all (S,G) entries for this group. If my_action
		     * == JOIN, then send the Join and override the (*,G)
		     * Prune.
		     */
		    for (mrtentry_srcs = mrtentry_ptr->group->mrtlink;
			 mrtentry_srcs != (mrtentry_t *) NULL;
			 mrtentry_srcs = mrtentry_srcs->grpnext)
		    {
			my_action = join_or_prune(mrtentry_srcs,
						  upstream_router);
			if (my_action == PIM_ACTION_JOIN)
			{
			    jp_value = (RANDOM() % 11) / (10 * PIM_RANDOM_DELAY_JOIN_TIMEOUT);
			    /* TODO: XXX: TIMER implem. dependency! */
			    if (mrtentry_ptr->jp_timer > jp_value)
				SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
			}
		    }		/* For all (S,G) */
		    continue;	/* End of (*,G) prune suppression */
		}

		/* (S,G) prune suppression */
		mrtentry_ptr = find_route(&source, &group, MRTF_SG,
					  DONT_CREATE);
		my_action = join_or_prune(mrtentry_ptr, upstream_router);
		if (my_action == PIM_ACTION_PRUNE)
		{
		    /* Suppress the (S,G) Prune */
		    /* TODO: XXX: TIMER implem. dependency! */
		    if ((mrtentry_ptr->jp_timer < holdtime)
			|| ((mrtentry_ptr->jp_timer == holdtime)
		    	&& (inet6_greaterthan(src, &v->uv_linklocal->pa_addr))))
		    {
			jp_value = pim_join_prune_period +
			    0.5 * (RANDOM() % pim_join_prune_period);
			if (mrtentry_ptr->jp_timer < jp_value)
			    SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
		    }
		}
		else
		    if (my_action == PIM_ACTION_JOIN)
		    {
			/* Override the Prune by scheduling a Join */
			jp_value = (RANDOM() % 11) / (10 * PIM_RANDOM_DELAY_JOIN_TIMEOUT);
			/* TODO: XXX: TIMER implem. dependency! */
			if (mrtentry_ptr->jp_timer > jp_value)
			    SET_TIMER(mrtentry_ptr->jp_timer, jp_value);
		    }
	    }			/* while (num_p_srcs--) */
	}			/* while (num_groups--) */
	return (TRUE);
    }				/* End of Join/Prune suppression code */

    /* I am the target of this join, so process the message */

    /*
     * The spec says that if there is (*,G) Join, it has priority over old
     * existing ~(S,G) prunes in the routing table. However, if the (*,G)
     * Join and the ~(S,G) prune are in the same message, ~(S,G) has the
     * priority. The spec doesn't say it, but I think the same is true for
     * (*,*,RP) and ~(S,G) prunes.
     * 
     * The code below do: (1) Check the whole message for (*,*,RP) Joins. (1.1)
     * If found, clean all pruned_oifs for all (*,G) and all (S,G) for each
     * RP in the list, but do not update the kernel cache. Then go back to
     * the beginning of the message and start processing for each group: (2)
     * Check for Prunes. If no prunes, process the Joins. (3) If there are
     * Prunes: (3.1) Scan the Join part for existing (*,G) Join. (3.1.1) If
     * there is (*,G) Join, clear join interface from the pruned_oifs for all
     * (S,G), but DO NOT flush the change to the kernel (by using
     * change_interfaces() for example) (3.2) After the pruned_oifs are
     * eventually cleared in (3.1.1), process the Prune part of the message
     * normally (setting the prune_oifs and flashing the changes to the
     * (kernel). (3.3) After the Prune part is processed, process the Join
     * part normally (by applying any changes to the kernel) (4) If there
     * were (*,*,RP) Join/Prune, process them.
     * 
     * If the Join/Prune list is too long, it may result in long processing
     * overhead. The idea above is not to place any wrong info in the kernel,
     * because it may result in short-time existing traffic forwarding on
     * wrong interface. Hopefully, in the future will find a better way to
     * implement it.
     */

    IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	log(LOG_DEBUG,0,"I'm the target of the JOIN/PRUNE message");

    num_groups_tmp = num_groups;
    data_ptr_start = data_ptr;
    star_star_rp_found = FALSE;	/* Indicating whether we have (*,*,RP) join */
 
    IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	log(LOG_DEBUG,0,"Number of groups to process : %d",num_groups_tmp);

    while (num_groups_tmp--)
    {
	/* Search for (*,*,RP) Join */
	GET_EGADDR6(&encod_group, data_ptr);
	GET_HOSTSHORT(num_j_srcs, data_ptr);
	GET_HOSTSHORT(num_p_srcs, data_ptr);
	group.sin6_addr = encod_group.mcast_addr;
	group.sin6_scope_id = inet6_uvif2scopeid(&group, v);

   	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	{
		log(LOG_DEBUG, 0,
		    "Group to process : %s",inet6_fmt(&encod_group.mcast_addr));
		log(LOG_DEBUG, 0,
		    "Number of join   : %d",num_j_srcs );
		log(LOG_DEBUG, 0,
		    "Number of prune  : %d",num_p_srcs );
	}

	if (!(inet6_equal(&group,&sockaddr6_d))
	    || (encod_src.masklen != STAR_STAR_RP_MSK6LEN))
	{
	    /* This is not (*,*,RP). Jump to the next group. */
	    data_ptr += (num_j_srcs + num_p_srcs) * sizeof(pim6_encod_src_addr_t);
	    IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
		{
		    log(LOG_DEBUG, 0,
			"I'm looking for the (*,*,RP) entry , skip to next entry");
		}
	    continue;
	}

	/*
	 * (*,*,RP) found. For each RP and each (*,G) and each (S,G) clear
	 * the pruned oif, but do not update the kernel.
	 */

	star_star_rp_found = TRUE;
	while (num_j_srcs--)
	{
	    GET_ESADDR6(&encod_src, data_ptr);
	    source.sin6_addr = encod_src.src_addr;
	    rpentry_ptr = rp_find(&source);

	    if (rpentry_ptr == (rpentry_t *) NULL)
		continue;
	    for (rp_grp_entry_ptr = rpentry_ptr->cand_rp->rp_grp_next;
		 rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
		 rp_grp_entry_ptr = rp_grp_entry_ptr->rp_grp_next)
	    {
		for (grpentry_ptr = rp_grp_entry_ptr->grplink;
		     grpentry_ptr != (grpentry_t *) NULL;
		     grpentry_ptr = grpentry_ptr->rpnext)
		{
		    if (grpentry_ptr->grp_route != (mrtentry_t *) NULL)
			IF_CLR(mifi, &grpentry_ptr->grp_route->pruned_oifs);
		    for (mrtentry_ptr = grpentry_ptr->mrtlink;
			 mrtentry_ptr != (mrtentry_t *) NULL;
			 mrtentry_ptr = mrtentry_ptr->grpnext)
			IF_CLR(mifi, &mrtentry_ptr->pruned_oifs);
		}
	    }
	}
	data_ptr += (num_p_srcs) * sizeof(pim6_encod_src_addr_t);
    }

    /*
     * Start processing the groups. If this is (*,*,RP), skip it, but process
     * it at the end.don't forget to reinit data_ptr!
     */

    data_ptr = data_ptr_start;
    num_groups_tmp = num_groups;

    while (num_groups_tmp--)
    {
	GET_EGADDR6(&encod_group, data_ptr);
	GET_HOSTSHORT(num_j_srcs, data_ptr);
	GET_HOSTSHORT(num_p_srcs, data_ptr);
	group.sin6_addr = encod_group.mcast_addr;
	group.sin6_scope_id = inet6_uvif2scopeid(&group, v);
  
 	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
	{
		log(LOG_DEBUG,0,"Group to process : %s",inet6_fmt(&encod_group.mcast_addr));
		log(LOG_DEBUG,0,"Number of join   : %d",num_j_srcs );
		log(LOG_DEBUG,0,"Number of prune  : %d",num_p_srcs );
	}	

	if (!IN6_IS_ADDR_MULTICAST(&group.sin6_addr))
	{
	    data_ptr += (num_j_srcs + num_p_srcs) * sizeof(pim6_encod_src_addr_t);
	    continue;		/* Ignore this group and jump to the next one */
	}


	if (inet6_equal(&group, &sockaddr6_d)
	    && (encod_group.masklen == STAR_STAR_RP_MSK6LEN))
	{
	    /* This is (*,*,RP). Jump to the next group. */
	    IF_DEBUG(DEBUG_PIM_JOIN_PRUNE) {
		    log(LOG_DEBUG, 0, "This is (*,*,RP). Jump to next.");
	    }
	    data_ptr += (num_j_srcs + num_p_srcs) * sizeof(pim6_encod_src_addr_t);
	    continue;
	}

	rpentry_ptr = rp_match(&group);
	if (rpentry_ptr == (rpentry_t *) NULL)
	    continue;

	IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
		log(LOG_DEBUG,0,"The rp for this JOIN/PRUNE is %s",inet6_fmt(&rpentry_ptr->address.sin6_addr));

	data_ptr_group_j_start = data_ptr;
	data_ptr_group_p_start = data_ptr + num_j_srcs * sizeof(pim6_encod_src_addr_t);

	/*
	 * Scan the Join part for (*,G) Join and then clear the particular
	 * interface from pruned_oifs for all (S,G). If the RP address in the
	 * Join message is different from the local match, ignore the whole
	 * group.
	 */

	num_j_srcs_tmp = num_j_srcs;
	ignore_group = FALSE;

	while (num_j_srcs_tmp--)
	{
	    GET_ESADDR6(&encod_src, data_ptr);
	    source.sin6_addr=encod_src.src_addr;
	    source.sin6_scope_id = inet6_uvif2scopeid(&source,v);
 
   	    if ((encod_src.flags & USADDR_RP_BIT)
		&& (encod_src.flags & USADDR_WC_BIT))
	    {
		/*
		 * This is the RP address, i.e. (*,G) Join. Check if the
		 * RP-mapping is consistent and if "yes", then Reset the
		 * pruned_oifs for all (S,G) entries.
		 */

		if(!inet6_equal(&rpentry_ptr->address, &source))	
		{
		    ignore_group = TRUE;
		    IF_DEBUG(DEBUG_PIM_JOIN_PRUNE)
			log(LOG_DEBUG,0,"And I'm not the RP for this address");
		    break;
		}

		mrtentry_ptr = find_route(&sockaddr6_any, &group,
					  MRTF_WC, DONT_CREATE);

		if (mrtentry_ptr != (mrtentry_t *) NULL)
		{
		    for (mrtentry_srcs = mrtentry_ptr->group->mrtlink;
			 mrtentry_srcs != (mrtentry_t *) NULL;
			 mrtentry_srcs = mrtentry_srcs->grpnext)
			IF_CLR(mifi, &mrtentry_srcs->pruned_oifs);
		}
		break;
	    }
	}

	if (ignore_group == TRUE)
	    continue;

	data_ptr = data_ptr_group_p_start;

	/* Process the Prune part first */

	while (num_p_srcs--)
	{
	    GET_ESADDR6(&encod_src, data_ptr);
	    source.sin6_addr = encod_src.src_addr;
	    source.sin6_scope_id = inet6_uvif2scopeid(&source, v);	    

	    if (!inet6_valid_host(&source))
		continue;
	    s_flags = encod_src.flags;
	    if (!(s_flags & (USADDR_WC_BIT | USADDR_RP_BIT)))
	    {
		/* (S,G) prune sent toward S */
		mrtentry_ptr = find_route(&source, &group, MRTF_SG,
					  DONT_CREATE);
		if (mrtentry_ptr == (mrtentry_t *) NULL)
		    continue;	/* I don't have (S,G) to prune. Ignore. */
		/*
		 * If the link is point-to-point, timeout the oif
		 * immediately, otherwise decrease the timer to allow other
		 * downstream routers to override the prune.
		 */
		/* TODO: XXX: increase the entry timer? */

		if (v->uv_flags & VIFF_POINT_TO_POINT)
		{
		    FIRE_TIMER(mrtentry_ptr->vif_timers[mifi]);
		}
		else
		{
		    /* TODO: XXX: TIMER implem. dependency! */
		    if (mrtentry_ptr->vif_timers[mifi] >
			mrtentry_ptr->vif_deletion_delay[mifi])
			SET_TIMER(mrtentry_ptr->vif_timers[mifi],
				  mrtentry_ptr->vif_deletion_delay[mifi]);
		}
		IF_TIMER_NOT_SET(mrtentry_ptr->vif_timers[mifi])
		{
		    IF_CLR(mifi, &mrtentry_ptr->joined_oifs);
		    IF_SET(mifi, &mrtentry_ptr->pruned_oifs);
		    change_interfaces(mrtentry_ptr,
				      mrtentry_ptr->incoming,
				      &mrtentry_ptr->joined_oifs,
				      &mrtentry_ptr->pruned_oifs,
				      &mrtentry_ptr->leaves,
				      &mrtentry_ptr->asserted_oifs, 0);
		}
		continue;
	    }

	    if ((s_flags & USADDR_RP_BIT)
		&& (!(s_flags & USADDR_WC_BIT)))
	    {
		/* ~(S,G)RPbit prune sent toward the RP */
		mrtentry_ptr = find_route(&source, &group, MRTF_SG,
					  DONT_CREATE);
		if (mrtentry_ptr != (mrtentry_t *) NULL)
		{
		    SET_TIMER(mrtentry_ptr->timer, holdtime);
		    if (v->uv_flags & VIFF_POINT_TO_POINT)
		    {
			FIRE_TIMER(mrtentry_ptr->vif_timers[mifi]);
		    }
		    else
		    {
			/* TODO: XXX: TIMER implem. dependency! */
			if (mrtentry_ptr->vif_timers[mifi] >
			    mrtentry_ptr->vif_deletion_delay[mifi])
			    SET_TIMER(mrtentry_ptr->vif_timers[mifi],
				    mrtentry_ptr->vif_deletion_delay[mifi]);
		    }
		    IF_TIMER_NOT_SET(mrtentry_ptr->vif_timers[mifi])
		    {
			IF_CLR(mifi, &mrtentry_ptr->joined_oifs);
			IF_SET(mifi, &mrtentry_ptr->pruned_oifs);
			change_interfaces(mrtentry_ptr,
					  mrtentry_ptr->incoming,
					  &mrtentry_ptr->joined_oifs,
					  &mrtentry_ptr->pruned_oifs,
					  &mrtentry_ptr->leaves,
					  &mrtentry_ptr->asserted_oifs, 0);
		    }
		    continue;
		}
		/* There is no (S,G) entry. Check for (*,G) or (*,*,RP) */
		mrtentry_ptr = find_route(NULL, &group,
					  MRTF_WC | MRTF_PMBR,
					  DONT_CREATE);
		if (mrtentry_ptr != (mrtentry_t *) NULL)
		{
		    mrtentry_ptr = find_route(&source, &group,
					      MRTF_SG | MRTF_RP,
					      CREATE);
		    if (mrtentry_ptr == (mrtentry_t *) NULL)
			continue;
		    mrtentry_ptr->flags &= ~MRTF_NEW;
		    RESET_TIMER(mrtentry_ptr->vif_timers[mifi]);

		    /*
		     * TODO: XXX: The spec doens't say what value to use for
		     * the entry time. Use the J/P holdtime.
		     */

		    SET_TIMER(mrtentry_ptr->timer, holdtime);

		    /*
		     * TODO: XXX: The spec says to delete the oif. However,
		     * its timer only should be lowered, so the prune can be
		     * overwritten on multiaccess LAN. Spec BUG.
		     */

		    IF_CLR(mifi, &mrtentry_ptr->joined_oifs);
		    IF_SET(mifi, &mrtentry_ptr->pruned_oifs);
		    change_interfaces(mrtentry_ptr,
				      mrtentry_ptr->incoming,
				      &mrtentry_ptr->joined_oifs,
				      &mrtentry_ptr->pruned_oifs,
				      &mrtentry_ptr->leaves,
				      &mrtentry_ptr->asserted_oifs, 0);
		}
		continue;
	    }

	    if ((s_flags & USADDR_RP_BIT) && (s_flags & USADDR_WC_BIT))
	    {
		/* (*,G) Prune */
		mrtentry_ptr = find_route(NULL, &group,
					  MRTF_WC | MRTF_PMBR,
					  DONT_CREATE);
		if (mrtentry_ptr != (mrtentry_t *) NULL)
		{
		    if (mrtentry_ptr->flags & MRTF_WC)
		    {
			/*
			 * TODO: XXX: Should check the whole Prune list in
			 * advance for (*,G) prune and if the RP address does
			 * not match the local RP-map, then ignore the whole
			 * group, not only this particular (*,G) prune.
			 */
			if (!inet6_equal(&mrtentry_ptr->group->active_rp_grp->rp->rpentry->address, &source )) 
			    continue;	/* The RP address doesn't match. */
			if (v->uv_flags & VIFF_POINT_TO_POINT)
			{
			    FIRE_TIMER(mrtentry_ptr->vif_timers[mifi]);
			}
			else
			{
			    /* TODO: XXX: TIMER implem. dependency! */
			    if (mrtentry_ptr->vif_timers[mifi] >
				mrtentry_ptr->vif_deletion_delay[mifi])
				SET_TIMER(mrtentry_ptr->vif_timers[mifi],
				    mrtentry_ptr->vif_deletion_delay[mifi]);
			}
			IF_TIMER_NOT_SET(mrtentry_ptr->vif_timers[mifi])
			{
			    IF_CLR(mifi, &mrtentry_ptr->joined_oifs);
			    IF_SET(mifi, &mrtentry_ptr->pruned_oifs);
			    change_interfaces(mrtentry_ptr,
					      mrtentry_ptr->incoming,
					      &mrtentry_ptr->joined_oifs,
					      &mrtentry_ptr->pruned_oifs,
					      &mrtentry_ptr->leaves,
					      &mrtentry_ptr->asserted_oifs, 0);
			}
			continue;
		    }
		    /* No (*,G) entry, but found (*,*,RP). Create (*,G) */
		    if (!inet6_equal(&mrtentry_ptr->source->address, &source))
			continue;	/* The RP address doesn't match. */
		    mrtentry_ptr = find_route(NULL, &group,
					      MRTF_WC, CREATE);
		    if (mrtentry_ptr == (mrtentry_t *) NULL)
			continue;
		    mrtentry_ptr->flags &= ~MRTF_NEW;
		    RESET_TIMER(mrtentry_ptr->vif_timers[mifi]);

		    /*
		     * TODO: XXX: should only lower the oif timer, so it can
		     * be overwritten on multiaccess LAN. Spec bug.
		     */

		    IF_CLR(mifi, &mrtentry_ptr->joined_oifs);
		    IF_SET(mifi, &mrtentry_ptr->pruned_oifs);
		    change_interfaces(mrtentry_ptr,
				      mrtentry_ptr->incoming,
				      &mrtentry_ptr->joined_oifs,
				      &mrtentry_ptr->pruned_oifs,
				      &mrtentry_ptr->leaves,
				      &mrtentry_ptr->asserted_oifs, 0);
		}		/* (*,G) or (*,*,RP) found */
	    }			/* (*,G) prune */
	}			/* while(num_p_srcs--) */

	/* End of (S,G) and (*,G) Prune handling */

	/* Jump back to the Join part and process it */
	data_ptr = data_ptr_group_j_start;
	while (num_j_srcs--)
	{
	    GET_ESADDR6(&encod_src, data_ptr);
	    source.sin6_addr = encod_src.src_addr;
	    source.sin6_scope_id = inet6_uvif2scopeid(&source, v);

	    if (!inet6_valid_host(&source))
		continue;
	    s_flags = encod_src.flags;
	    MASKLEN_TO_MASK6(encod_src.masklen, s_mask);
	    if ((s_flags & USADDR_WC_BIT)
		&& (s_flags & USADDR_RP_BIT))
	    {
		/* (*,G) Join toward RP */
		/*
		 * It has been checked already that this RP address is the
		 * same as the local RP-maping.
		 */
		mrtentry_ptr = find_route(NULL, &group, MRTF_WC,
					  CREATE);
		if (mrtentry_ptr == (mrtentry_t *) NULL)
		    continue;
		IF_SET(mifi, &mrtentry_ptr->joined_oifs);
		IF_CLR(mifi, &mrtentry_ptr->pruned_oifs);
		IF_CLR(mifi, &mrtentry_ptr->asserted_oifs);
		/* TODO: XXX: TIMER implem. dependency! */
		if (mrtentry_ptr->vif_timers[mifi] < holdtime)
		{
		    SET_TIMER(mrtentry_ptr->vif_timers[mifi], holdtime);
		    mrtentry_ptr->vif_deletion_delay[mifi] = holdtime / 3;
		}
		if (mrtentry_ptr->timer < holdtime)
		    SET_TIMER(mrtentry_ptr->timer, holdtime);
		mrtentry_ptr->flags &= ~MRTF_NEW;
		change_interfaces(mrtentry_ptr,
				  mrtentry_ptr->incoming,
				  &mrtentry_ptr->joined_oifs,
				  &mrtentry_ptr->pruned_oifs,
				  &mrtentry_ptr->leaves,
				  &mrtentry_ptr->asserted_oifs, 0);
		/*
		 * Need to update the (S,G) entries, because of the previous
		 * cleaning of the pruned_oifs. The reason is that if the
		 * oifs for (*,G) weren't changed, the (S,G) entries won't be
		 * updated by change_interfaces()
		 */

		for (mrtentry_srcs = mrtentry_ptr->group->mrtlink;
		     mrtentry_srcs != (mrtentry_t *) NULL;
		     mrtentry_srcs = mrtentry_srcs->grpnext)
		    change_interfaces(mrtentry_srcs,
				      mrtentry_srcs->incoming,
				      &mrtentry_srcs->joined_oifs,
				      &mrtentry_srcs->pruned_oifs,
				      &mrtentry_srcs->leaves,
				      &mrtentry_srcs->asserted_oifs, 0);
		continue;
	    }

	    if (!(s_flags & (USADDR_WC_BIT | USADDR_RP_BIT)))
	    {
		/* (S,G) Join toward S */
		if (mifi == get_iif(&source))
		    continue;	/* Ignore this (S,G) Join */
		mrtentry_ptr = find_route(&source, &group, MRTF_SG, CREATE);
		if (mrtentry_ptr == (mrtentry_t *) NULL)
		    continue;
		IF_SET(mifi, &mrtentry_ptr->joined_oifs);
		IF_CLR(mifi, &mrtentry_ptr->pruned_oifs);
		IF_CLR(mifi, &mrtentry_ptr->asserted_oifs);
		/* TODO: XXX: TIMER implem. dependency! */
		if (mrtentry_ptr->vif_timers[mifi] < holdtime)
		{
		    SET_TIMER(mrtentry_ptr->vif_timers[mifi], holdtime);
		    mrtentry_ptr->vif_deletion_delay[mifi] = holdtime / 3;
		}
		if (mrtentry_ptr->timer < holdtime)
		    SET_TIMER(mrtentry_ptr->timer, holdtime);
		/*
		 * TODO: if this is a new entry, send immediately the Join
		 * message toward S. The Join/Prune timer for new entries is
		 * 0, but it does not means the message will be sent
		 * immediately.
		 */
		mrtentry_ptr->flags &= ~MRTF_NEW;
		/*
		 * Note that we must create (S,G) without the RPbit set. If
		 * we already had such entry, change_interfaces() will reset
		 * the RPbit propertly.
		 */
		change_interfaces(mrtentry_ptr,
				  mrtentry_ptr->source->incoming,
				  &mrtentry_ptr->joined_oifs,
				  &mrtentry_ptr->pruned_oifs,
				  &mrtentry_ptr->leaves,
				  &mrtentry_ptr->asserted_oifs, 0);
		continue;
	    }
	}			/* while(num_j_srcs--) */
    }				/* for all groups */

    /* Now process the (*,*,RP) Join/Prune */

    if (star_star_rp_found == TRUE)
	return (TRUE);
    data_ptr = data_ptr_start;
    while (num_groups--)
    {
	/*
	 * The conservative approach is to scan again the whole message, just
	 * in case if we have more than one (*,*,RP) requests.
	 */
	GET_EGADDR6(&encod_group, data_ptr);
	GET_HOSTSHORT(num_j_srcs, data_ptr);
	GET_HOSTSHORT(num_p_srcs, data_ptr);

	group.sin6_addr = encod_group.mcast_addr;
	group.sin6_scope_id = inet6_uvif2scopeid(&group, v);

	if (!inet6_equal(&group,&sockaddr6_d)
	    || (encod_group.masklen != STAR_STAR_RP_MSK6LEN))
	{
	    /* This is not (*,*,RP). Jump to the next group. */
	    data_ptr +=
		(num_j_srcs + num_p_srcs) * sizeof(pim6_encod_src_addr_t);
	    continue;
	}
	/* (*,*,RP) found */
	while (num_j_srcs--)
	{
	    /* TODO: XXX: check that the iif is different from the Join oifs */
	    GET_ESADDR6(&encod_src, data_ptr);
	    source.sin6_addr = encod_src.src_addr;
	    source.sin6_scope_id = inet6_uvif2scopeid(&source,
                                      v);


	    if (!inet6_valid_host(&source))
		continue;
	    s_flags = encod_src.flags;
	    MASKLEN_TO_MASK6(encod_src.masklen, s_mask);
	    mrtentry_ptr = find_route(&source, NULL, MRTF_PMBR,
				      CREATE);
	    if (mrtentry_ptr == (mrtentry_t *) NULL)
		continue;
	    IF_SET(mifi, &mrtentry_ptr->joined_oifs);
	    IF_CLR(mifi, &mrtentry_ptr->pruned_oifs);
	    IF_CLR(mifi, &mrtentry_ptr->asserted_oifs);
	    /* TODO: XXX: TIMER implem. dependency! */
	    if (mrtentry_ptr->vif_timers[mifi] < holdtime)
	    {
		SET_TIMER(mrtentry_ptr->vif_timers[mifi], holdtime);
		mrtentry_ptr->vif_deletion_delay[mifi] = holdtime / 3;
	    }
	    if (mrtentry_ptr->timer < holdtime)
		SET_TIMER(mrtentry_ptr->timer, holdtime);
	    mrtentry_ptr->flags &= ~MRTF_NEW;
	    change_interfaces(mrtentry_ptr,
			      mrtentry_ptr->incoming,
			      &mrtentry_ptr->joined_oifs,
			      &mrtentry_ptr->pruned_oifs,
			      &mrtentry_ptr->leaves,
			      &mrtentry_ptr->asserted_oifs, 0);

	    /*
	     * Need to update the (S,G) and (*,G) entries, because of the
	     * previous cleaning of the pruned_oifs. The reason is that if
	     * the oifs for (*,*,RP) weren't changed, the (*,G) and (S,G)
	     * entries won't be updated by change_interfaces()
	     */

	    for (rp_grp_entry_ptr = mrtentry_ptr->source->cand_rp->rp_grp_next;
		 rp_grp_entry_ptr != (rp_grp_entry_t *) NULL;
		 rp_grp_entry_ptr = rp_grp_entry_ptr->rp_grp_next)
		for (grpentry_ptr = rp_grp_entry_ptr->grplink;
		     grpentry_ptr != (grpentry_t *) NULL;
		     grpentry_ptr = grpentry_ptr->rpnext)
		{
		    /* Update the (*,G) entry */
		    change_interfaces(grpentry_ptr->grp_route,
				      grpentry_ptr->grp_route->incoming,
				      &grpentry_ptr->grp_route->joined_oifs,
				      &grpentry_ptr->grp_route->pruned_oifs,
				      &grpentry_ptr->grp_route->leaves,
				 	  &grpentry_ptr->grp_route->asserted_oifs, 0);
		    /* Update the (S,G) entries */
		    for (mrtentry_srcs = grpentry_ptr->mrtlink;
			 mrtentry_srcs != (mrtentry_t *) NULL;
			 mrtentry_srcs = mrtentry_srcs->grpnext)
			change_interfaces(mrtentry_srcs,
					  mrtentry_srcs->incoming,
					  &mrtentry_srcs->joined_oifs,
					  &mrtentry_srcs->pruned_oifs,
					  &mrtentry_srcs->leaves,
					  &mrtentry_srcs->asserted_oifs, 0);
		}
	    continue;
	}

	while (num_p_srcs--)
	{
	    /* TODO: XXX: can we have (*,*,RP) Prune? */
	    GET_ESADDR6(&encod_src, data_ptr);
	    source.sin6_addr = encod_src.src_addr;
	    source.sin6_scope_id = inet6_uvif2scopeid(&source,
                                      v);

	    if (!inet6_valid_host(&source))
		continue;
	    s_flags = encod_src.flags;
	    MASKLEN_TO_MASK6(encod_src.masklen, s_mask);
	    mrtentry_ptr = find_route(&source, NULL , MRTF_PMBR,
				      DONT_CREATE);
	    if (mrtentry_ptr == (mrtentry_t *) NULL)
		continue;
	    /*
	     * If the link is point-to-point, timeout the oif immediately,
	     * otherwise decrease the timer to allow other downstream routers
	     * to override the prune.
	     */
	    /* TODO: XXX: increase the entry timer? */
	    if (v->uv_flags & VIFF_POINT_TO_POINT)
	    {
		FIRE_TIMER(mrtentry_ptr->vif_timers[mifi]);
	    }
	    else
	    {
		/* TODO: XXX: TIMER implem. dependency! */
		if (mrtentry_ptr->vif_timers[mifi] >
		    mrtentry_ptr->vif_deletion_delay[mifi])
		    SET_TIMER(mrtentry_ptr->vif_timers[mifi],
			      mrtentry_ptr->vif_deletion_delay[mifi]);
	    }
	    IF_TIMER_NOT_SET(mrtentry_ptr->vif_timers[mifi])
	    {
		IF_CLR(mifi, &mrtentry_ptr->joined_oifs);
		IF_SET(mifi, &mrtentry_ptr->pruned_oifs);
		IF_SET(mifi, &mrtentry_ptr->asserted_oifs);
		change_interfaces(mrtentry_ptr,
				  mrtentry_ptr->incoming,
				  &mrtentry_ptr->joined_oifs,
				  &mrtentry_ptr->pruned_oifs,
				  &mrtentry_ptr->leaves,
				  &mrtentry_ptr->asserted_oifs, 0);
	    }

	}
    }				/* For all groups processing (*,*,R) */

    return (TRUE);
}


/*
 * TODO: NOT USED, probably buggy, but may need it in the future.
 */
/*
 * TODO: create two functions: periodic which timeout the timers and
 * non-periodic which only check but don't timeout the timers.
 */
/*
 * Create and send Join/Prune messages per interface. Only the entries which
 * have the Join/Prune timer expired are included. In the special case when
 * we have ~(S,G)RPbit Prune entry, we must include any (*,G) or (*,*,RP)
 * Currently the whole table is scanned. In the future will have all routing
 * entries linked in a chain with the corresponding upstream pim_nbr_entry.
 * 
 * If pim_nbr is not NULL, then send to only this particular PIM neighbor,
 */
int
send_periodic_pim6_join_prune(mifi, pim_nbr, holdtime)
    mifi_t          mifi;
    pim_nbr_entry_t *pim_nbr;
    u_int16         holdtime;
{
    grpentry_t     		*grpentry_ptr;
    mrtentry_t     		*mrtentry_ptr;
    rpentry_t      		*rpentry_ptr;
    struct sockaddr_in6   	src_addr;
    struct uvif    		*v;
    pim_nbr_entry_t 		*pim_nbr_ptr;
    cand_rp_t      		*cand_rp_ptr;

    /*
     * Walk through all routing entries. The iif must match to include the
     * entry. Check first the (*,G) entry and then all associated (S,G). At
     * the end of the message will add any (*,*,RP) entries. TODO: check
     * other PIM-SM implementations and decide the more appropriate place to
     * put the (*,*,RP) entries: in the beginning of the message or at the
     * end.
     */

    v = &uvifs[mifi];

    /* Check the (*,G) and (S,G) entries */
    for (grpentry_ptr = grplist; grpentry_ptr != (grpentry_t *) NULL;
	 grpentry_ptr = grpentry_ptr->next)
    {
	mrtentry_ptr = grpentry_ptr->grp_route;
	/* TODO: XXX: TIMER implem. dependency! */
	if ((mrtentry_ptr != (mrtentry_t *) NULL)
	    && (mrtentry_ptr->incoming == mifi)
	    && (mrtentry_ptr->jp_timer <= timer_interval))
	{

	    /* If join/prune to a particular neighbor only was specified */
	    if ((pim_nbr != (pim_nbr_entry_t *) NULL)
		&& (mrtentry_ptr->upstream != pim_nbr))
		continue;

	    /* TODO: XXX: The J/P suppression timer is not in the spec! */
	    if (!IF_ISEMPTY(&mrtentry_ptr->joined_oifs) ||
		(v->uv_flags & VIFF_DR))
	    {
		add_jp_entry(mrtentry_ptr->upstream, holdtime,
			     &grpentry_ptr->group,
			     SINGLE_GRP_MSK6LEN,
			     &grpentry_ptr->rpaddr,
			     SINGLE_SRC_MSK6LEN, 0, PIM_ACTION_JOIN);
	    }
	    /* TODO: XXX: TIMER implem. dependency! */
	    if (IF_ISEMPTY(&mrtentry_ptr->joined_oifs)
		&& (!(v->uv_flags & VIFF_DR))
		&& (mrtentry_ptr->jp_timer <= timer_interval))
	    {
		add_jp_entry(mrtentry_ptr->upstream, holdtime,
			     &grpentry_ptr->group, SINGLE_GRP_MSK6LEN,
			     &grpentry_ptr->rpaddr,
			     SINGLE_SRC_MSK6LEN, 0, PIM_ACTION_PRUNE);
	    }
	}

	/* Check the (S,G) entries */
	for (mrtentry_ptr = grpentry_ptr->mrtlink;
	     mrtentry_ptr != (mrtentry_t *) NULL;
	     mrtentry_ptr = mrtentry_ptr->grpnext)
	{

	    /* If join/prune to a particular neighbor only was specified */
	    if ((pim_nbr != (pim_nbr_entry_t *) NULL)
		&& (mrtentry_ptr->upstream != pim_nbr))
		continue;

	    if (mrtentry_ptr->flags & MRTF_RP)
	    {
		/* RPbit set */

		src_addr = mrtentry_ptr->source->address;
		if (IF_ISEMPTY(&mrtentry_ptr->joined_oifs)
		    || ((find_vif_direct_local(&src_addr) != NO_VIF)
			&& grpentry_ptr->grp_route != (mrtentry_t *) NULL))
		    /* TODO: XXX: TIMER implem. dependency! */
		    if ((grpentry_ptr->grp_route->incoming == mifi)
			&& (grpentry_ptr->grp_route->jp_timer
			    <= timer_interval))
			/* S is directly connected. Send toward RP */
			add_jp_entry(grpentry_ptr->grp_route->upstream,
				     holdtime,
				     &grpentry_ptr->group, SINGLE_GRP_MSK6LEN,
				     &src_addr, SINGLE_SRC_MSK6LEN,
				     MRTF_RP, PIM_ACTION_PRUNE);
	    }
	    else
	    {
		/* RPbit cleared */
		if (IF_ISEMPTY(&mrtentry_ptr->joined_oifs))
		{
		    /* TODO: XXX: TIMER implem. dependency! */
		    if ((mrtentry_ptr->incoming == mifi)
			&& (mrtentry_ptr->jp_timer <= timer_interval))
			add_jp_entry(mrtentry_ptr->upstream, holdtime,
				     &grpentry_ptr->group, SINGLE_GRP_MSK6LEN,
				     &mrtentry_ptr->source->address,
				     SINGLE_SRC_MSK6LEN, 0, PIM_ACTION_PRUNE);
		}
		else
		{
		    /* TODO: XXX: TIMER implem. dependency! */
		    if ((mrtentry_ptr->incoming == mifi)
			&& (mrtentry_ptr->jp_timer <= timer_interval))
			add_jp_entry(mrtentry_ptr->upstream, holdtime,
				     &grpentry_ptr->group, SINGLE_GRP_MSK6LEN,
				     &mrtentry_ptr->source->address,
				     SINGLE_SRC_MSK6LEN, 0, PIM_ACTION_JOIN);
		}
		/* TODO: XXX: TIMER implem. dependency! */
		if ((mrtentry_ptr->flags & MRTF_SPT)
		    && (grpentry_ptr->grp_route != (mrtentry_t *) NULL)
		    && (mrtentry_ptr->incoming !=
			grpentry_ptr->grp_route->incoming)
		    && (grpentry_ptr->grp_route->incoming == mifi)
		    && (grpentry_ptr->grp_route->jp_timer
			<= timer_interval))
		    add_jp_entry(grpentry_ptr->grp_route->upstream, holdtime,
				 &grpentry_ptr->group, SINGLE_GRP_MSK6LEN,
				 &mrtentry_ptr->source->address,
				 SINGLE_SRC_MSK6LEN, MRTF_RP,
				 PIM_ACTION_PRUNE);
	    }
	}
    }

    /* Check the (*,*,RP) entries */
    for (cand_rp_ptr = cand_rp_list; cand_rp_ptr != (cand_rp_t *) NULL;
	 cand_rp_ptr = cand_rp_ptr->next)
    {
	rpentry_ptr = cand_rp_ptr->rpentry;

	/* If join/prune to a particular neighbor only was specified */
	if ((pim_nbr != (pim_nbr_entry_t *) NULL)
	    && (rpentry_ptr->upstream != pim_nbr))
	    continue;


	/* TODO: XXX: TIMER implem. dependency! */
	if ((rpentry_ptr->mrtlink != (mrtentry_t *) NULL)
	    && (rpentry_ptr->incoming == mifi)
	    && (rpentry_ptr->mrtlink->jp_timer <= timer_interval))
	{
	    add_jp_entry(rpentry_ptr->upstream, holdtime,
			 &sockaddr6_d, STAR_STAR_RP_MSK6LEN,
			 &rpentry_ptr->address,
			 SINGLE_SRC_MSK6LEN, MRTF_RP | MRTF_WC,
			 PIM_ACTION_JOIN);
	}
    }

    /* Send all pending Join/Prune messages */
    for (pim_nbr_ptr = v->uv_pim_neighbors;
	 pim_nbr_ptr != (pim_nbr_entry_t *) NULL;
	 pim_nbr_ptr = pim_nbr->next)
    {

	/* If join/prune to a particular neighbor only was specified */
	if ((pim_nbr != (pim_nbr_entry_t *) NULL)
	    && (pim_nbr_ptr != pim_nbr))
	    continue;

	pack_and_send_jp6_message(pim_nbr_ptr);
    }

    return (TRUE);
}


int
add_jp_entry(pim_nbr, holdtime, group, grp_msklen, source, src_msklen,
	     addr_flags, join_prune)
    pim_nbr_entry_t 		*pim_nbr;
    u_int16         		holdtime;
    struct sockaddr_in6         *group;
    u_int8          		grp_msklen;
    struct sockaddr_in6         *source;
    u_int8          		src_msklen;
    u_int16         		addr_flags;
    u_int8          		join_prune;
{
    build_jp_message_t 		*bjpm;
    u_int8         		*data_ptr;
    u_int8          		flags = 0;
    int             		rp_flag;


    bjpm = pim_nbr->build_jp_message;

    if (bjpm != (build_jp_message_t *) NULL)
    {
	if ((bjpm->jp_message_size + bjpm->join_list_size +
	     bjpm->prune_list_size + bjpm->rp_list_join_size +
	     bjpm->rp_list_prune_size >= MAX_JP_MESSAGE_SIZE)
	    || (bjpm->join_list_size >= MAX_JOIN_LIST_SIZE)
	    || (bjpm->prune_list_size >= MAX_PRUNE_LIST_SIZE)
	    || (bjpm->rp_list_join_size >= MAX_JOIN_LIST_SIZE)
	    || (bjpm->rp_list_prune_size >= MAX_PRUNE_LIST_SIZE))
	{
	    /*
	     * TODO: XXX: BUG: If the list is getting too large, must be
	     * careful with the fragmentation.
	     */
	    pack_and_send_jp6_message(pim_nbr);
	    bjpm = pim_nbr->build_jp_message;	/* The buffer will be freed */
	}
    }

    if (bjpm != (build_jp_message_t *) NULL)
    {
	if ((!inet6_equal(&bjpm->curr_group, group)
	    || (bjpm->curr_group_msklen != grp_msklen)
	    || (bjpm->holdtime != holdtime)))
	{
	    pack_jp6_message(pim_nbr);
	}
    }

    if (bjpm == (build_jp_message_t *) NULL)
    {
	bjpm = get_jp6_working_buff();
	pim_nbr->build_jp_message = bjpm;
	data_ptr = bjpm->jp_message;
	PUT_EUADDR6(pim_nbr->address.sin6_addr, data_ptr);
	PUT_BYTE(0, data_ptr);	/* Reserved */
	bjpm->num_groups_ptr = data_ptr++;	/* The pointer for numgroups */
	*(bjpm->num_groups_ptr) = 0;	/* Zero groups */
	PUT_HOSTSHORT(holdtime, data_ptr);
	bjpm->holdtime = holdtime;
	bjpm->jp_message_size = data_ptr - bjpm->jp_message;
    }

    /* TODO: move somewhere else, only when it is a new group */
    bjpm->curr_group = *group;
    bjpm->curr_group_msklen = grp_msklen;

    if (inet6_equal(group, &sockaddr6_d) &&
	(grp_msklen == STAR_STAR_RP_MSK6LEN))
	rp_flag = TRUE;
    else
	rp_flag = FALSE;

    switch (join_prune)
    {
    case PIM_ACTION_JOIN:
	if (rp_flag == TRUE)
	    data_ptr = bjpm->rp_list_join + bjpm->rp_list_join_size;
	else
	    data_ptr = bjpm->join_list + bjpm->join_list_size;
	break;
    case PIM_ACTION_PRUNE:
	if (rp_flag == TRUE)
	    data_ptr = bjpm->rp_list_join + bjpm->rp_list_join_size;
	else
	    data_ptr = bjpm->prune_list + bjpm->prune_list_size;
	break;
    default:
	return (FALSE);
    }

    flags |= USADDR_S_BIT;	/* Mandatory for PIMv2 */
    if (addr_flags & MRTF_RP)
	flags |= USADDR_RP_BIT;
    if (addr_flags & MRTF_WC)
	flags |= USADDR_WC_BIT;
    PUT_ESADDR6(source->sin6_addr, src_msklen, flags, data_ptr);

    switch (join_prune)
    {
    case PIM_ACTION_JOIN:
	if (rp_flag == TRUE)
	{
	    bjpm->rp_list_join_size = data_ptr - bjpm->rp_list_join;
	    bjpm->rp_list_join_number++;
	}
	else
	{
	    bjpm->join_list_size = data_ptr - bjpm->join_list;
	    bjpm->join_addr_number++;
	}
	break;
    case PIM_ACTION_PRUNE:
	if (rp_flag == TRUE)
	{
	    bjpm->rp_list_prune_size = data_ptr - bjpm->rp_list_prune;
	    bjpm->rp_list_prune_number++;
	}
	else
	{
	    bjpm->prune_list_size = data_ptr - bjpm->prune_list;
	    bjpm->prune_addr_number++;
	}
	break;
    default:
	return (FALSE);
    }

    return (TRUE);
}


/* TODO: check again the size of the buffers */

static build_jp_message_t *
get_jp6_working_buff()
{
    build_jp_message_t *bjpm_ptr;

    if (build_jp_message_pool_counter == 0)
    {
	bjpm_ptr = (build_jp_message_t *) malloc(sizeof(build_jp_message_t));
	bjpm_ptr->next = (build_jp_message_t *) NULL;
	bjpm_ptr->jp_message =
	    (u_int8 *) malloc(MAX_JP_MESSAGE_SIZE +
			      sizeof(pim_jp_encod_grp_t) +
			      2 * sizeof(pim6_encod_src_addr_t));
	bjpm_ptr->jp_message_size = 0;
	bjpm_ptr->join_list_size = 0;
	bjpm_ptr->join_addr_number = 0;
	bjpm_ptr->join_list = (u_int8 *) malloc(MAX_JOIN_LIST_SIZE +
					      sizeof(pim6_encod_src_addr_t));
	bjpm_ptr->prune_list_size = 0;
	bjpm_ptr->prune_addr_number = 0;
	bjpm_ptr->prune_list = (u_int8 *) malloc(MAX_PRUNE_LIST_SIZE +
					      sizeof(pim6_encod_src_addr_t));
	bjpm_ptr->rp_list_join_size = 0;
	bjpm_ptr->rp_list_join_number = 0;
	bjpm_ptr->rp_list_join = (u_int8 *) malloc(MAX_JOIN_LIST_SIZE +
					      sizeof(pim6_encod_src_addr_t));
	bjpm_ptr->rp_list_prune_size = 0;
	bjpm_ptr->rp_list_prune_number = 0;
	bjpm_ptr->rp_list_prune = (u_int8 *) malloc(MAX_PRUNE_LIST_SIZE +
					      sizeof(pim6_encod_src_addr_t));
	bjpm_ptr->curr_group = sockaddr6_any;
	bjpm_ptr->curr_group_msklen = 0;
	bjpm_ptr->holdtime = 0;
	return bjpm_ptr;
    }
    else
    {
	bjpm_ptr = build_jp_message_pool;
	build_jp_message_pool = build_jp_message_pool->next;
	build_jp_message_pool_counter--;
	bjpm_ptr->jp_message_size = 0;
	bjpm_ptr->join_list_size = 0;
	bjpm_ptr->join_addr_number = 0;
	bjpm_ptr->prune_list_size = 0;
	bjpm_ptr->prune_addr_number = 0;
	bjpm_ptr->curr_group = sockaddr6_any;
	bjpm_ptr->curr_group_msklen = 0;
	return (bjpm_ptr);
    }
}


static void
return_jp6_working_buff(pim_nbr)
    pim_nbr_entry_t *pim_nbr;
{
    build_jp_message_t *bjpm_ptr = pim_nbr->build_jp_message;

    if (bjpm_ptr == (build_jp_message_t *) NULL)
	return;
    /* Don't waste memory by keeping too many free buffers */
    /* TODO: check/modify the definitions for POOL_NUMBER and size */
    if (build_jp_message_pool_counter >= MAX_JP_MESSAGE_POOL_NUMBER)
    {
	free((void *) bjpm_ptr->jp_message);
	free((void *) bjpm_ptr->join_list);
	free((void *) bjpm_ptr->prune_list);
	free((void *) bjpm_ptr->rp_list_join);
	free((void *) bjpm_ptr->rp_list_prune);
	free((void *) bjpm_ptr);
    }
    else
    {
	bjpm_ptr->next = build_jp_message_pool;
	build_jp_message_pool = bjpm_ptr;
	build_jp_message_pool_counter++;
    }
    pim_nbr->build_jp_message = (build_jp_message_t *) NULL;
}


/*
 * TODO: XXX: Currently, the (*,*,RP) stuff goes at the end of the Join/Prune
 * message. However, this particular implementation of PIM processes the
 * Join/Prune messages faster if (*,*,RP) is at the beginning. Modify some of
 * the functions below such that the outgoing messages place (*,*,RP) at the
 * beginning, not at the end.
 */

static void
pack_jp6_message(pim_nbr)
    pim_nbr_entry_t *pim_nbr;
{
    build_jp_message_t *bjpm;
    u_int8         *data_ptr;

    bjpm = pim_nbr->build_jp_message;
    if ((bjpm == (build_jp_message_t *) NULL)
	|| (inet6_equal(&bjpm->curr_group,&sockaddr6_any)))
	return;
    data_ptr = bjpm->jp_message + bjpm->jp_message_size;
    PUT_EGADDR6(bjpm->curr_group.sin6_addr, bjpm->curr_group_msklen, 0, data_ptr);
    PUT_HOSTSHORT(bjpm->join_addr_number, data_ptr);
    PUT_HOSTSHORT(bjpm->prune_addr_number, data_ptr);
    bcopy(bjpm->join_list, data_ptr, bjpm->join_list_size);
    data_ptr += bjpm->join_list_size;
    bcopy(bjpm->prune_list, data_ptr, bjpm->prune_list_size);
    data_ptr += bjpm->prune_list_size;
    bjpm->jp_message_size = (data_ptr - bjpm->jp_message);
    bjpm->join_list_size = 0;
    bjpm->join_addr_number = 0;
#if 0				/* isn't this necessary? */
    bjpm->rp_list_join_size = 0;
    bjpm->rp_list_join_number = 0;
#endif
    bjpm->prune_list_size = 0;
    bjpm->prune_addr_number = 0;
#if 0				/* isn't this necessary? */
    bjpm->rp_list_prune_size = 0;
    bjpm->rp_list_prune_number = 0;
#endif
    (*bjpm->num_groups_ptr)++;
    bjpm->curr_group = sockaddr6_any;
    bjpm->curr_group_msklen = 0;
    if (*bjpm->num_groups_ptr == ((u_int8) ~ 0 - 1))
    {
	if (bjpm->rp_list_join_number + bjpm->rp_list_prune_number)
	{
	    /* Add the (*,*,RP) at the end */
	    data_ptr = bjpm->jp_message + bjpm->jp_message_size;
	    PUT_EGADDR6(sockaddr6_d.sin6_addr, STAR_STAR_RP_MSK6LEN, 0, data_ptr);
	    PUT_HOSTSHORT(bjpm->rp_list_join_number, data_ptr);
	    PUT_HOSTSHORT(bjpm->rp_list_prune_number, data_ptr);
	    bcopy(bjpm->rp_list_join, data_ptr, bjpm->rp_list_join_size);
	    data_ptr += bjpm->rp_list_join_size;
	    bcopy(bjpm->rp_list_prune, data_ptr, bjpm->rp_list_prune_size);
	    data_ptr += bjpm->rp_list_prune_size;
	    bjpm->jp_message_size = (data_ptr - bjpm->jp_message);
	    bjpm->rp_list_join_size = 0;
	    bjpm->rp_list_join_number = 0;
	    bjpm->rp_list_prune_size = 0;
	    bjpm->rp_list_prune_number = 0;
	    (*bjpm->num_groups_ptr)++;
	}
	send_jp6_message(pim_nbr);
    }
}

void
pack_and_send_jp6_message(pim_nbr)
    pim_nbr_entry_t *pim_nbr;
{
    u_int8         	*data_ptr;
    build_jp_message_t 	*bjpm;


    if ((pim_nbr == (pim_nbr_entry_t *) NULL)
     || ((bjpm = pim_nbr->build_jp_message) == (build_jp_message_t *) NULL))
	{
		return;
	}
    pack_jp6_message(pim_nbr);


    if (bjpm->rp_list_join_number + bjpm->rp_list_prune_number)
    {
	/* Add the (*,*,RP) at the end */
	data_ptr = bjpm->jp_message + bjpm->jp_message_size;

	PUT_EGADDR6(sockaddr6_d.sin6_addr, STAR_STAR_RP_MSK6LEN, 0, data_ptr);
	PUT_HOSTSHORT(bjpm->rp_list_join_number, data_ptr);
	PUT_HOSTSHORT(bjpm->rp_list_prune_number, data_ptr);
	bcopy(bjpm->rp_list_join, data_ptr, bjpm->rp_list_join_size);
	data_ptr += bjpm->rp_list_join_size;
	bcopy(bjpm->rp_list_prune, data_ptr, bjpm->rp_list_prune_size);
	data_ptr += bjpm->rp_list_prune_size;
	bjpm->jp_message_size = (data_ptr - bjpm->jp_message);
	bjpm->rp_list_join_size = 0;
	bjpm->rp_list_join_number = 0;
	bjpm->rp_list_prune_size = 0;
	bjpm->rp_list_prune_number = 0;
	(*bjpm->num_groups_ptr)++;
    }
    send_jp6_message(pim_nbr);
}

static void
send_jp6_message(pim_nbr)
    pim_nbr_entry_t *pim_nbr;
{
    u_int16         datalen;
    mifi_t          mifi;

    datalen = pim_nbr->build_jp_message->jp_message_size;
    mifi = pim_nbr->vifi;
    bcopy(pim_nbr->build_jp_message->jp_message,
	  pim6_send_buf+sizeof(struct pim), datalen);

    send_pim6(pim6_send_buf, &uvifs[mifi].uv_linklocal->pa_addr,
	      &allpim6routers_group , PIM_JOIN_PRUNE, datalen);
    uvifs[mifi].uv_out_pim6_join_prune++;
    return_jp6_working_buff(pim_nbr);
}

/************************************************************************
 *                        PIM_ASSERT
 ************************************************************************/
int
receive_pim6_assert(src, dst, pim_message, datalen)
    struct sockaddr_in6 	*src,
                    		*dst;
    register char  		*pim_message;
    int             		datalen;
{
    mifi_t          		mifi;
    pim6_encod_uni_addr_t 	eusaddr;
    pim6_encod_grp_addr_t 	egaddr;
    struct sockaddr_in6		source,
                    		group;
    mrtentry_t     		*mrtentry_ptr,
                   		*mrtentry_ptr2;
    u_int8         		*data_ptr;
    struct uvif    		*v;
    u_int32         		assert_preference;
    u_int32         		assert_metric;
    u_int32         		assert_rptbit;
    u_int32         		local_metric;
    u_int32         		local_preference;
    u_int8          		local_rptbit;
    u_int8          		local_wins;
    pim_nbr_entry_t 		*original_upstream_router;


    if ((mifi = find_vif_direct(src)) == NO_VIF)
    {
	/*
	 * Either a local vif or somehow received PIM_ASSERT from
	 * non-directly connected router. Ignore it.
	 */

	if (local_address(src) == NO_VIF)
	    log(LOG_INFO, 0,
		"Ignoring PIM_ASSERT from non-neighbor router %s",
		inet6_fmt(&src->sin6_addr));
	return (FALSE);
    }

    v = &uvifs[mifi];
    v->uv_in_pim6_assert++;
    if (uvifs[mifi].uv_flags &
	(VIFF_DOWN | VIFF_DISABLED | VIFF_NONBRS | MIFF_REGISTER))
	return (FALSE);		/* Shoudn't come on this interface */
    data_ptr = (u_int8 *) (pim_message + sizeof(struct pim));

    /* Get the group and source addresses */
    GET_EGADDR6(&egaddr, data_ptr);
    GET_EUADDR6(&eusaddr, data_ptr);

    /* Get the metric related info */
    GET_HOSTLONG(assert_preference, data_ptr);
    GET_HOSTLONG(assert_metric, data_ptr);
    assert_rptbit = assert_preference & PIM_ASSERT_RPT_BIT;

    source.sin6_addr  = eusaddr.unicast_addr;
    source.sin6_scope_id = inet6_uvif2scopeid(&source, v);

    group.sin6_addr = egaddr.mcast_addr;
    group.sin6_scope_id = inet6_uvif2scopeid(&group, v);

    /* Find the longest "active" entry, i.e. the one with a kernel mirror */
    if (assert_rptbit)
    {
	mrtentry_ptr = find_route(NULL, &group,
				  MRTF_WC | MRTF_PMBR, DONT_CREATE);
	if (mrtentry_ptr != (mrtentry_t *) NULL)
	    if (!(mrtentry_ptr->flags & MRTF_KERNEL_CACHE))
		if (mrtentry_ptr->flags & MRTF_WC)
		{
		    mrtentry_ptr =
			mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
		}
    }
    else
    {
	mrtentry_ptr = find_route(&source, &group,
				MRTF_SG | MRTF_WC | MRTF_PMBR, DONT_CREATE);
	if ((mrtentry_ptr != (mrtentry_t *) NULL))
	    if (!(mrtentry_ptr->flags & MRTF_KERNEL_CACHE))
	    {
		if (mrtentry_ptr->flags & MRTF_SG)
		{
		    mrtentry_ptr2 = mrtentry_ptr->group->grp_route;
		    if ((mrtentry_ptr2 != (mrtentry_t *) NULL)
			&& (mrtentry_ptr2->flags & MRTF_KERNEL_CACHE))
			mrtentry_ptr = mrtentry_ptr2;
		    else
			mrtentry_ptr = mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
		}
		else
		    if (mrtentry_ptr->flags & MRTF_WC)
			mrtentry_ptr = mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	    }
    }
    if ((mrtentry_ptr == (mrtentry_t *) NULL)
	|| (!(mrtentry_ptr->flags & MRTF_KERNEL_CACHE)))
	/* No routing entry or not "active" entry. Ignore the assert */
	return (FALSE);

    /* Prepare the local preference and metric */
    if ((mrtentry_ptr->flags & MRTF_PMBR)
	|| ((mrtentry_ptr->flags & MRTF_SG)
	    && (!(mrtentry_ptr->flags & MRTF_RP))))
    {
	/* Either (S,G) (toward S) or (*,*,RP). */
	/* TODO: XXX: get the info from mrtentry, or source or from kernel ? */
	/*
	 * local_metric = mrtentry_ptr->source->metric; local_preference =
	 * mrtentry_ptr->source->preference;
	 */
	local_metric = mrtentry_ptr->metric;
	local_preference = mrtentry_ptr->preference;
    }
    else
    {
	/*
	 * Should be (*,G) or (S,G)RPbit entry. Get what we need from the RP
	 * info.
	 */
	/* TODO: get the info from mrtentry, RP-entry or kernel? */
	/*
	 * local_metric =
	 * mrtentry_ptr->group->active_rp_grp->rp->rpentry->metric;
	 * local_preference =
	 * mrtentry_ptr->group->active_rp_grp->rp->rpentry->preference;
	 */

	local_metric = mrtentry_ptr->metric;
	local_preference = mrtentry_ptr->preference;
    }

    local_rptbit = (mrtentry_ptr->flags & MRTF_RP);
    if (local_rptbit)
	/* Make the RPT bit the most significant one */
	local_preference |= PIM_ASSERT_RPT_BIT;


    if (IF_ISSET(mifi, &mrtentry_ptr->oifs))
    {
	/* The ASSERT has arrived on oif */

	/*
	 * TODO: XXX: here the processing order is different from the spec.
	 * The spec requires first eventually to create a routing entry (see
	 * 3.5.2.1(1) and then compare the metrics. Here we compare first the
	 * metrics with the existing longest match entry and if we lose then
	 * create a new entry and compare again. This saves us the
	 * unnecessary creating of a routing entry if we anyway are going to
	 * lose: for example the local (*,*,RP) vs the remote (*,*,RP) or
	 * (*,G)
	 */

	local_wins = compare_metrics(local_preference, local_metric,
				     &v->uv_linklocal->pa_addr, assert_preference,
				     assert_metric, src);

	if (local_wins == TRUE)
	{
	    /* TODO: verify the parameters */
	    send_pim6_assert(&source, &group, mifi, mrtentry_ptr);
	    return (TRUE);
	}

	/* Create a "better" routing entry and try again */

	if ((assert_rptbit) && (mrtentry_ptr->flags & MRTF_PMBR))
	{
	    /* The matching entry was (*,*,RP). Create (*,G) */
	    mrtentry_ptr2 = find_route(NULL, &group, MRTF_WC, CREATE);
	}
	else
	    if ((!assert_rptbit) &&
		(mrtentry_ptr->flags & (MRTF_WC | MRTF_PMBR)))
	    {
		/* create (S,G) */
		mrtentry_ptr2 = find_route(&source, &group, MRTF_SG, CREATE);
	    }
	    else
	    {
		/* We have no chance to win. Give up and prune the oif */
		mrtentry_ptr2 = (mrtentry_t *) NULL;
	    }

	if (mrtentry_ptr2 != (mrtentry_t *) NULL)
	{
	    mrtentry_ptr2->flags &= ~MRTF_NEW;

	    /*
	     * TODO: XXX: The spec doesn't say what entry timer value to use
	     * when the routing entry is created because of asserts.
	     */

	    SET_TIMER(mrtentry_ptr2->timer, pim_data_timeout);
	    if (mrtentry_ptr2->flags & MRTF_RP)
	    {
		/*
		 * Either (*,G) or (S,G)RPbit entry. Get what we need from
		 * the RP info.
		 */
		/* TODO: where to get the metric+preference from? */
		/*
		 * local_metric =
		 * mrtentry_ptr->group->active_rp_grp->rp->rpentry->metric;
		 * local_preference =
		 * mrtentry_ptr->group->active_rp_grp->rp->rpentry->preference
		 * ;
		 */
		local_metric = mrtentry_ptr->metric;
		local_preference = mrtentry_ptr->preference;
		local_preference |= PIM_ASSERT_RPT_BIT;
	    }
	    else
	    {
		/* (S,G) toward the source */
		/* TODO: where to get the metric from ? */
		/*
		 * local_metric = mrtentry_ptr->source->metric;
		 * local_preference = mrtentry_ptr->source->preference;
		 */
		local_metric = mrtentry_ptr->metric;
		local_preference = mrtentry_ptr->preference;
	    }

	    local_wins = compare_metrics(local_preference, local_metric,
					 &v->uv_linklocal->pa_addr, assert_preference,
					 assert_metric, src);

	    if (local_wins == TRUE)
	    {
		/* TODO: verify the parameters */
		send_pim6_assert(&source, &group, mifi, mrtentry_ptr);
		return (TRUE);
	    }
	    /* We lost, but have created the entry which has to be pruned */
	    mrtentry_ptr = mrtentry_ptr2;
	}

	/* Have to remove that outgoing vifi from mrtentry_ptr */
	IF_SET(mifi, &mrtentry_ptr->asserted_oifs);
	/* TODO: XXX: TIMER implem. dependency! */
	if (mrtentry_ptr->timer < pim_assert_timeout)
	    SET_TIMER(mrtentry_ptr->timer, pim_assert_timeout);
	/*
	 * TODO: XXX: check that the timer of all affected routing entries
	 * has been restarted.
	 */
	change_interfaces(mrtentry_ptr,
			  mrtentry_ptr->incoming,
			  &mrtentry_ptr->joined_oifs,
			  &mrtentry_ptr->pruned_oifs,
			  &mrtentry_ptr->leaves,
			  &mrtentry_ptr->asserted_oifs, 0);
	return (FALSE);		/* Doesn't matter the return value */
    }				/* End of assert received on oif */


    if (mrtentry_ptr->incoming == mifi)
    {
	/* Assert received on iif */
	if (assert_rptbit)
	{
	    if (!(mrtentry_ptr->flags & MRTF_RP))
		return (TRUE);	/* The locally used upstream router will win
				 * the assert, so don't change it. */
	}

	/*
	 * TODO: where to get the local metric and preference from? system
	 * call or mrtentry is fine?
	 */
	local_metric = mrtentry_ptr->metric;
	local_preference = mrtentry_ptr->preference;
	if (mrtentry_ptr->flags & MRTF_RP)
	    local_preference |= PIM_ASSERT_RPT_BIT;

	local_wins = compare_metrics(local_preference, local_metric,
				     &mrtentry_ptr->upstream->address,
				     assert_preference, assert_metric, src);

	if (local_wins == TRUE)
	    return (TRUE);	/* return whatever */

	/* The upstream must be changed to the winner */
	mrtentry_ptr->preference = assert_preference;
	mrtentry_ptr->metric = assert_metric;
	mrtentry_ptr->upstream = find_pim6_nbr(src);

	/* Check if the upstream router is different from the original one */
	if (mrtentry_ptr->flags & MRTF_PMBR)
	    original_upstream_router = mrtentry_ptr->source->upstream;
	else
	    if (mrtentry_ptr->flags & MRTF_RP)
		original_upstream_router =
		    mrtentry_ptr->group->active_rp_grp->rp->rpentry->upstream;
	    else
		original_upstream_router = mrtentry_ptr->source->upstream;
	if (mrtentry_ptr->upstream != original_upstream_router)
	{
	    mrtentry_ptr->flags |= MRTF_ASSERTED;
	    SET_TIMER(mrtentry_ptr->assert_timer, pim_assert_timeout);
	}
	else
	    mrtentry_ptr->flags &= ~MRTF_ASSERTED;
    }

    return (TRUE);
}


int
send_pim6_assert(source, group, mifi, mrtentry_ptr)
    struct sockaddr_in6	*source;
    struct sockaddr_in6 *group;
    mifi_t          mifi;
    mrtentry_t     *mrtentry_ptr;
{
    u_int8         *data_ptr;
    u_int8         *data_start_ptr;
    u_int32         local_preference;
    u_int32         local_metric;
    srcentry_t     *srcentry_ptr;

    /* Don't send assert if the outgoing interface a tunnel or register vif */
    if (uvifs[mifi].uv_flags & (MIFF_REGISTER | VIFF_TUNNEL))
	return (FALSE);

    data_ptr = (u_int8 *) (pim6_send_buf + sizeof(struct pim));
    data_start_ptr = data_ptr;
    PUT_EGADDR6(group->sin6_addr, SINGLE_GRP_MSK6LEN, 0, data_ptr);
    PUT_EUADDR6(source->sin6_addr, data_ptr);

    /*
     * TODO: XXX: where to get the metric from: srcentry_ptr or mrtentry_ptr
     * or from the kernel?
     */

    if (mrtentry_ptr->flags & MRTF_PMBR)
    {
	/* (*,*,RP) */
	srcentry_ptr = mrtentry_ptr->source;
	/*
	 * TODO: set_incoming(srcentry_ptr, PIM_IIF_RP);
	 */
    }
    else
	if (mrtentry_ptr->flags & MRTF_RP)
	{
	    /* (*,G) or (S,G)RPbit (iif toward RP) */
	    srcentry_ptr = mrtentry_ptr->group->active_rp_grp->rp->rpentry;
	    /*
	     * TODO: set_incoming(srcentry_ptr, PIM_IIF_RP);
	     */
	}
	else
	{
	    /* (S,G) toward S */
	    srcentry_ptr = mrtentry_ptr->source;
	    /*
	     * TODO: set_incoming(srcentry_ptr, PIM_IIF_SOURCE);
	     */
	}

    /*
     * TODO: check again! local_metric = srcentry_ptr->metric;
     * local_preference = srcentry_ptr->preference;
     */
    local_metric = mrtentry_ptr->metric;
    local_preference = mrtentry_ptr->preference;

    if (mrtentry_ptr->flags & MRTF_RP)
	local_preference |= PIM_ASSERT_RPT_BIT;
    PUT_HOSTLONG(local_preference, data_ptr);
    PUT_HOSTLONG(local_metric, data_ptr);

    send_pim6(pim6_send_buf, &uvifs[mifi].uv_linklocal->pa_addr,
          &allpim6routers_group, PIM_ASSERT,
          data_ptr - data_start_ptr);
    uvifs[mifi].uv_out_pim6_assert++;

    return (TRUE);
}


/* Return TRUE if the local win, otherwise FALSE */
static int
compare_metrics(local_preference, local_metric, local_address,
		remote_preference, remote_metric, remote_address)
    u_int32         		local_preference;
    u_int32         		local_metric;
    struct sockaddr_in6		*local_address;
    u_int32         		remote_preference;
    u_int32         		remote_metric;
    struct sockaddr_in6 	*remote_address;
{
    /* Now lets see who has a smaller gun (aka "asserts war") */
    /*
     * FYI, the smaller gun...err metric wins, but if the same caliber, then
     * the bigger network address wins. The order of threatment is:
     * preference, metric, address.
     */
    /*
     * The RPT bits are already included as the most significant bits of the
     * preferences.
     */
    if (remote_preference > local_preference)
	return TRUE;
    if (remote_preference < local_preference)
	return FALSE;
    if (remote_metric > local_metric)
	return TRUE;
    if (remote_metric < local_metric)
	return FALSE;
    if (inet6_greaterthan(local_address, remote_address))
       return TRUE;

    return FALSE;
}


/************************************************************************
 *                        PIM_BOOTSTRAP
 ************************************************************************/
#define PIM6_BOOTSTRAP_MINLEN (PIM_MINLEN + PIM6_ENCODE_UNI_ADDR_LEN)

int
receive_pim6_bootstrap(src, dst, pim_message, datalen)
    struct sockaddr_in6		*src,
                    		*dst;
    char           		*pim_message;
    int             		datalen;
{
    u_int8         		*data_ptr;
    u_int8         		*max_data_ptr;
    u_int16         		new_bsr_fragment_tag;
    u_int8          		new_bsr_hash_masklen;
    u_int8          		new_bsr_priority;
    pim6_encod_uni_addr_t 	new_bsr_uni_addr;
    struct sockaddr_in6         new_bsr_address;
    struct rpfctl   		rpfc;
    pim_nbr_entry_t 		*n,
                   		*rpf_neighbor;
    struct sockaddr_in6         neighbor_addr;
    mifi_t          		mifi,
                    		incoming = NO_VIF;
    int             		min_datalen;
    pim6_encod_grp_addr_t 	curr_group_addr;
    pim6_encod_uni_addr_t 	curr_rp_addr;
    u_int8          		curr_rp_count;
    u_int8          		curr_frag_rp_count;
    u_int16         		reserved_short;
    u_int16         		curr_rp_holdtime;
    u_int8          		curr_rp_priority;
    u_int8          		reserved_byte;
    struct in6_addr    		curr_group_mask;
    grp_mask_t     		*grp_mask_ptr;
    grp_mask_t     		*grp_mask_next;
    rp_grp_entry_t 		*grp_rp_entry_ptr;
    rp_grp_entry_t 		*grp_rp_entry_next;
    struct sockaddr_in6		prefix_h,
				prefix_h2,
				group_,
				rpp_;
    int i;
    struct uvif 		*v;


    if ((mifi=find_vif_direct(src)) == NO_VIF)
    {
	/*
	 * Either a local vif or somehow received PIM_BOOTSTRAP from
	 * non-directly connected router. Ignore it.
	 */
	if (local_address(src) == NO_VIF)
	    log(LOG_INFO, 0,
		"Ignoring PIM_BOOTSTRAP from non-neighbor router %s",
		inet6_fmt(&src->sin6_addr));
	return (FALSE);
    }

    /* sanity check for the minimum length */
    if (datalen < PIM6_BOOTSTRAP_MINLEN) {
	    log(LOG_NOTICE, 0,
		"receive_pim6_bootstrap: Bootstrap message size(%u) is"
		" too short from %s",
		datalen, inet6_fmt(&src->sin6_addr));
	    return(FALSE);
    }

    v = &uvifs[mifi];
    v->uv_in_pim6_bootsrap++;
    data_ptr = (u_int8 *) (pim_message + sizeof(struct pim));

    /* Parse the PIM_BOOTSTRAP message */
    GET_HOSTSHORT(new_bsr_fragment_tag, data_ptr);
    GET_BYTE(new_bsr_hash_masklen, data_ptr);
    GET_BYTE(new_bsr_priority, data_ptr);
    GET_EUADDR6(&new_bsr_uni_addr, data_ptr);

    /* 
     * BSR address must be a global unicast address.
     * [draft-ietf-pim-ipv6-01.txt sec 4.5]
     */
    if (IN6_IS_ADDR_MULTICAST(&new_bsr_uni_addr.unicast_addr) ||
	IN6_IS_ADDR_LINKLOCAL(&new_bsr_uni_addr.unicast_addr) ||
	IN6_IS_ADDR_SITELOCAL(&new_bsr_uni_addr.unicast_addr)) {
	    log(LOG_WARNING, 0,
		"receive_pim6_bootstrap: invalid BSR address: %s",
		inet6_fmt(&new_bsr_uni_addr.unicast_addr));
	    return(FALSE);
    }

    new_bsr_address.sin6_addr = new_bsr_uni_addr.unicast_addr;
    new_bsr_address.sin6_len = sizeof(new_bsr_address);
    new_bsr_address.sin6_family = AF_INET6;
    new_bsr_address.sin6_scope_id = inet6_uvif2scopeid(&new_bsr_address, v);

    if (local_address(&new_bsr_address) != NO_VIF)
    {
	IF_DEBUG(DEBUG_RPF | DEBUG_PIM_BOOTSTRAP)
	    log(LOG_DEBUG, 0,
		"receive_pim6_bootstrap: Bootstrap from myself(%s), ignored.",
		inet6_fmt(&new_bsr_address.sin6_addr));
	return (FALSE);		/* The new BSR is one of my local addresses */
    }

    /*
     * Compare the current BSR priority with the priority of the BSR included
     * in the message.
     */
    /*
     * TODO: if I am just starting and will become the BSR, I should accept
     * the message coming from the current BSR and get the current
     * Cand-RP-Set.
     */

    if ((curr_bsr_priority > new_bsr_priority) ||
	((curr_bsr_priority == new_bsr_priority)
	&& (inet6_greaterthan(&curr_bsr_address, &new_bsr_address))))
    {
	/* The message's BSR is less preferred than the current BSR */
	log(LOG_DEBUG, 0,
	    "receive_pim6_bootstrap: BSR(%s, prio=%d) is less preferred"
	    " than the current BSR(%s, prio=%d)",
	    inet6_fmt(&new_bsr_address.sin6_addr), new_bsr_priority,
	    inet6_fmt(&curr_bsr_address.sin6_addr), curr_bsr_priority);
	return (FALSE);		/* Ignore the received BSR message */
    }

    /* Check the iif, if this was PIM-ROUTERS multicast */
    if (IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &allpim6routers_group.sin6_addr))
    {
	k_req_incoming(&new_bsr_address, &rpfc);
	if ((rpfc.iif == NO_VIF) ||
	    IN6_IS_ADDR_UNSPECIFIED(&rpfc.rpfneighbor.sin6_addr))
	{
	    /* coudn't find a route to the BSR */
	    log(LOG_NOTICE, 0,
		"receive_pim6_bootstrap: can't find a route to the BSR(%s)",
		inet6_fmt(&new_bsr_address.sin6_addr));
	    return (FALSE);
	}

	neighbor_addr = *src;
	incoming = rpfc.iif;

	if (uvifs[incoming].uv_flags &
	    (VIFF_DISABLED | VIFF_DOWN | MIFF_REGISTER))
	{
	  log(LOG_NOTICE, 0,
	      "receive_pim6_bootstrap: Bootstrap from an invalid interface(%s)",
	      uvifs[incoming].uv_name);
	    return (FALSE);	/* Shoudn't arrive on that interface */
	}

	/* Find the upstream router */

	for (n = uvifs[incoming].uv_pim_neighbors; n != NULL; n = n->next)
	{
	    if (inet6_lessthan(&neighbor_addr, &n->address))
		continue;
	    if (inet6_equal(&neighbor_addr, &n->address))
	    {
		rpf_neighbor = n;
		break;
	    }
	    log(LOG_NOTICE, 0,
		"receive_pim6_bootstrap: Bootstrap from an unrecognized "
		"neighbor(%s) on %s",
		inet6_fmt(&neighbor_addr.sin6_addr), uvifs[incoming].uv_name);
	    return (FALSE);	/* No neighbor toward BSR found */
	}

	/* redundant checks? */
	if ((n == (pim_nbr_entry_t *) NULL ))
	{
	    return (FALSE);	/* Sender of this message is not the RPF*/
	}
				 			/* neighbor */
	if(!(inet6_equal(&n->address, src)))
	{
		return (FALSE);
	}
    }
    else
    {
	if (local_address(dst) == NO_VIF)
	    /*
	     * TODO: XXX: this situation should be handled earlier: The
	     * destination is neither ALL_PIM_ROUTERS nor me
	     */
	    log(LOG_NOTICE, 0,
		"receive_pim6_bootstrap: Bootstrap with an invalid dst(%s)",
		inet6_fmt(&dst->sin6_addr));
	    return (FALSE);

	/* Probably unicasted from the current DR */
	if (cand_rp_list != (cand_rp_t *) NULL)
	{
	    /*
	     * Hmmm, I do have a Cand-RP-list, but some neighbor has a
	     * different opinion and is unicasting it to me. Ignore this guy.
	     */
	    log(LOG_INFO, 0,
		"receive_pim6_bootstrap: Bootstrap received but we already "
		"have RPs. ignored.");
	    return (FALSE);
	}
	for (mifi = 0; mifi < numvifs; mifi++)
	{
	    if (uvifs[mifi].uv_flags & (VIFF_DISABLED | VIFF_DOWN |
					MIFF_REGISTER))
		continue;
	    if (inet6_equal(&uvifs[mifi].uv_linklocal->pa_addr,dst))
	    {
		incoming = mifi;
		break;
	    }
	}
	if (incoming == NO_VIF)
	{
	    /* Cannot find the receiving iif toward that DR */
	    IF_DEBUG(DEBUG_RPF | DEBUG_PIM_BOOTSTRAP)
		log(LOG_DEBUG, 0,
		    "Unicast boostrap message from %s to %s ignored: "
		    "cannot find iif",
		    inet6_fmt(&src->sin6_addr), inet6_fmt(&dst->sin6_addr));
	    return (FALSE);
	}
	/*
	 * TODO: check the sender is directly connected and I am really the
	 * DR.
	 */
    }

    if (cand_rp_flag == TRUE)
    {
	/* If change in the BSR address, send immediately Cand-RP-Adv */
	/* TODO: use some random delay? */

	if (!inet6_equal(&new_bsr_address , &curr_bsr_address))
	{
	    send_pim6_cand_rp_adv();
	    SET_TIMER(pim_cand_rp_adv_timer, my_cand_rp_adv_period);
	}
    }

    /* Forward the BSR Message first and then update the RP-set list */
    /* XXX: should we do sanity checks before forwarding?? */
    /* TODO: if the message was unicasted to me, resend? */

    for (mifi = 0; mifi < numvifs; mifi++)
    {
	if (mifi == incoming)
	    continue;
	if (uvifs[mifi].uv_flags & (VIFF_DISABLED | VIFF_DOWN |
				    MIFF_REGISTER | VIFF_TUNNEL | VIFF_NONBRS))
	    continue;

	bcopy(pim_message, (char *)(pim6_send_buf), datalen);

	send_pim6(pim6_send_buf, &uvifs[mifi].uv_linklocal->pa_addr,
		  &allpim6routers_group, PIM_BOOTSTRAP,
		  datalen - sizeof(struct pim));
    }

    max_data_ptr = (u_int8 *) pim_message + datalen;
 
    /*
     * TODO: XXX: this 24 is HARDCODING!!! Do a bunch of definitions and make
     * it stylish!
     * 24 = Encoded-Group Address(20) + RP-cound(1) + Frag-RP(1) + Reserved(2)
     */
    min_datalen = 24;

    if ((new_bsr_fragment_tag != curr_bsr_fragment_tag) ||
	(inet6_equal(&new_bsr_address, &curr_bsr_address)))
    {
	/* Throw away the old segment */
	delete_rp_list(&segmented_cand_rp_list, &segmented_grp_mask_list);
    }

    curr_bsr_address = new_bsr_address;
    curr_bsr_priority = new_bsr_priority;
    curr_bsr_fragment_tag = new_bsr_fragment_tag;
    MASKLEN_TO_MASK6(new_bsr_hash_masklen, curr_bsr_hash_mask);
    SET_TIMER(pim_bootstrap_timer, PIM_BOOTSTRAP_TIMEOUT);

    while (data_ptr + min_datalen <= max_data_ptr)
    {
	GET_EGADDR6(&curr_group_addr, data_ptr);
	GET_BYTE(curr_rp_count, data_ptr);
	GET_BYTE(curr_frag_rp_count, data_ptr);
	GET_HOSTSHORT(reserved_short, data_ptr);
	MASKLEN_TO_MASK6(curr_group_addr.masklen, curr_group_mask);

	if (IN6_IS_ADDR_MC_NODELOCAL(&curr_group_addr.mcast_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&curr_group_addr.mcast_addr)) {
		log(LOG_WARNING, 0,
		    "receive_pim6_bootstrap: "
		    "group prefix has a narraw scope: %s (ignored)",
		    inet6_fmt(&curr_group_addr.mcast_addr));
		continue;
	}
	if (curr_rp_count == 0)
	{
	    group_.sin6_addr = curr_group_addr.mcast_addr;
	    delete_grp_mask(&cand_rp_list, &grp_mask_list,
			    &group_, curr_group_mask);
	    continue;
	}
	if (curr_rp_count == curr_frag_rp_count)
    	{
            /* Add all RPs */
	    while (curr_frag_rp_count--)
	    {
		/*
		 * Sanity for the data length; the data packet must contain
		 * Encoded-Unicast-RP-Address(18) + RP-Holdtime(2) +
		 * RP-Priority(1) + Reserved(1).
		 */
		if (data_ptr + PIM6_ENCODE_UNI_ADDR_LEN + sizeof(u_int32_t)
		    > max_data_ptr) {
		    log(LOG_NOTICE, 0,
			"receive_pim6_bootstrap: Bootstrap from %s on %s " 
			"does not have enough length to contatin RP information",
			inet6_fmt(&src->sin6_addr), v->uv_name);

		    /*
		     * Ignore the rest of the message.
		     * XXX: should we discard the entire message?
		     */
		    goto garbage_collect;
		}

		GET_EUADDR6(&curr_rp_addr, data_ptr);
		GET_HOSTSHORT(curr_rp_holdtime, data_ptr);
		GET_BYTE(curr_rp_priority, data_ptr);
		GET_BYTE(reserved_byte, data_ptr);
		MASKLEN_TO_MASK6(curr_group_addr.masklen, curr_group_mask);
		rpp_.sin6_addr = curr_rp_addr.unicast_addr;
    		rpp_.sin6_len = sizeof(rpp_);
    		rpp_.sin6_family = AF_INET6;
		/*
		 * The cand_rp address scope should be global.
		 * XXX: however, is a site-local RP sometimes useful?
		 *      we currently discard such RP...
		 */
		if (IN6_IS_ADDR_LINKLOCAL(&rpp_.sin6_addr) ||
		    IN6_IS_ADDR_SITELOCAL(&rpp_.sin6_addr)) {
			log(LOG_WARNING, 0,
			    "receive_pim6_bootstrap: invalid RP address: %s",
			    inet6_fmt(&rpp_.sin6_addr));
			continue;
		}
    		rpp_.sin6_scope_id = 0;
		group_.sin6_addr = curr_group_addr.mcast_addr;
		group_.sin6_len = sizeof(group_);
		group_.sin6_family = AF_INET6;
		group_.sin6_scope_id = inet6_uvif2scopeid(&group_,v);
	
		add_rp_grp_entry(&cand_rp_list, &grp_mask_list,
				 &rpp_, curr_rp_priority,
				 curr_rp_holdtime, &group_,
				 curr_group_mask,
				 curr_bsr_hash_mask,
				 curr_bsr_fragment_tag);
	    }
	    continue;
	}
	/*
	 * This is a partial list of the RPs for this group prefix. Save
	 * until all segments arrive.
	 */

	for (i = 0; i < sizeof(struct in6_addr); i++) {
	    prefix_h.sin6_addr.s6_addr[i] =
		curr_group_addr.mcast_addr.s6_addr[i]
		    & curr_group_mask.s6_addr[i];
	}
	
	for (grp_mask_ptr = segmented_grp_mask_list;
	     grp_mask_ptr != (grp_mask_t *) NULL;
	     grp_mask_ptr = grp_mask_ptr->next)
	{
	    for (i = 0; i < sizeof(struct in6_addr); i++) {
		prefix_h2.sin6_addr.s6_addr[i] =
		    grp_mask_ptr->group_addr.sin6_addr.s6_addr[i]
			& grp_mask_ptr->group_mask.s6_addr[i];
	    }

	    if (inet6_greaterthan(&prefix_h2, &prefix_h))
		continue;
	    else
		break;
	}
	if ((grp_mask_ptr != (grp_mask_t *) NULL)
	    && (IN6_ARE_ADDR_EQUAL(&grp_mask_ptr->group_addr.sin6_addr,
				   &curr_group_addr.mcast_addr))
	    && (IN6_ARE_ADDR_EQUAL(&grp_mask_ptr->group_mask, &curr_group_mask))
	    && (grp_mask_ptr->group_rp_number + curr_frag_rp_count
		== curr_rp_count))
	{
	    /* All missing PRs have arrived. Add all RP entries */
	    while (curr_frag_rp_count--)
	    {
		GET_EUADDR6(&curr_rp_addr, data_ptr);
		GET_HOSTSHORT(curr_rp_holdtime, data_ptr);
		GET_BYTE(curr_rp_priority, data_ptr);
		GET_BYTE(reserved_byte, data_ptr);
		MASKLEN_TO_MASK6(curr_group_addr.masklen, curr_group_mask);
		rpp_.sin6_addr = curr_rp_addr.unicast_addr;
		rpp_.sin6_scope_id=0;
		group_.sin6_addr = curr_group_addr.mcast_addr;
		group_.sin6_scope_id = inet6_uvif2scopeid(&group_,v);
		add_rp_grp_entry(&cand_rp_list,
				 &grp_mask_list,
				 &rpp_,
				 curr_rp_priority,
				 curr_rp_holdtime,
				 &group_,
				 curr_group_mask,
				 curr_bsr_hash_mask,
				 curr_bsr_fragment_tag);
	    }
	    /* Add the rest from the previously saved segments */
	    for (grp_rp_entry_ptr = grp_mask_ptr->grp_rp_next;
		 grp_rp_entry_ptr != (rp_grp_entry_t *) NULL;
		 grp_rp_entry_ptr = grp_rp_entry_ptr->grp_rp_next)
	    {
		group_.sin6_addr = curr_group_addr.mcast_addr;
		group_.sin6_scope_id = inet6_uvif2scopeid(&group_,v);
		add_rp_grp_entry(&cand_rp_list,
				 &grp_mask_list,
				 &grp_rp_entry_ptr->rp->rpentry->address,
				 grp_rp_entry_ptr->priority,
				 grp_rp_entry_ptr->holdtime,
				 &group_,
				 curr_group_mask,
				 curr_bsr_hash_mask,
				 curr_bsr_fragment_tag);
	    }
	    group_.sin6_addr = curr_group_addr.mcast_addr;
	    group_.sin6_scope_id = inet6_uvif2scopeid(&group_,v);
		delete_grp_mask(&segmented_cand_rp_list,
			    &segmented_grp_mask_list,
			    &group_,
			    curr_group_mask);
	}
	else
	{
	    /* Add the partially received RP-list to the group of pending RPs */
	    while (curr_frag_rp_count--)
	    {
		GET_EUADDR6(&curr_rp_addr, data_ptr);
		GET_HOSTSHORT(curr_rp_holdtime, data_ptr);
		GET_BYTE(curr_rp_priority, data_ptr);
		GET_BYTE(reserved_byte, data_ptr);
		MASKLEN_TO_MASK6(curr_group_addr.masklen, curr_group_mask);
		rpp_.sin6_addr = curr_rp_addr.unicast_addr;
		group_.sin6_addr = curr_group_addr.mcast_addr;
		group_.sin6_scope_id = inet6_uvif2scopeid(&group_,v);
		add_rp_grp_entry(&segmented_cand_rp_list,
				 &segmented_grp_mask_list,
				 &rpp_,
				 curr_rp_priority,
				 curr_rp_holdtime,
				 &group_,
				 curr_group_mask,
				 curr_bsr_hash_mask,
				 curr_bsr_fragment_tag);
	    }
	}
    }

    
 garbage_collect:
    /*
     * Garbage collection. Check all group prefixes and if the fragment_tag
     * for a group_prefix is the same as curr_bsr_fragment_tag, then remove
     * all RPs for this group_prefix which have different fragment tag.
     */

    for (grp_mask_ptr = grp_mask_list;
	 grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_next)
    {
	grp_mask_next = grp_mask_ptr->next;
	if (grp_mask_ptr->fragment_tag == curr_bsr_fragment_tag)
	{
	    for (grp_rp_entry_ptr = grp_mask_ptr->grp_rp_next;
		 grp_rp_entry_ptr != (rp_grp_entry_t *) NULL;
		 grp_rp_entry_ptr = grp_rp_entry_next)
	    {
		grp_rp_entry_next = grp_rp_entry_ptr->grp_rp_next;
		if (grp_rp_entry_ptr->fragment_tag != curr_bsr_fragment_tag)
		    delete_rp_grp_entry(&cand_rp_list, &grp_mask_list,
					grp_rp_entry_ptr);
	    }
	}
    }

    /* Cleanup also the list used by incompleted segments */

    for (grp_mask_ptr = segmented_grp_mask_list;
	 grp_mask_ptr != (grp_mask_t *) NULL;
	 grp_mask_ptr = grp_mask_next)
    {
	grp_mask_next = grp_mask_ptr->next;
	if (grp_mask_ptr->fragment_tag == curr_bsr_fragment_tag)
	{
	    for (grp_rp_entry_ptr = grp_mask_ptr->grp_rp_next;
		 grp_rp_entry_ptr != (rp_grp_entry_t *) NULL;
		 grp_rp_entry_ptr = grp_rp_entry_next)
	    {
		grp_rp_entry_next = grp_rp_entry_ptr->grp_rp_next;
		if (grp_rp_entry_ptr->fragment_tag != curr_bsr_fragment_tag)
		    delete_rp_grp_entry(&segmented_cand_rp_list,
					&segmented_grp_mask_list,
					grp_rp_entry_ptr);
	    }
	}
    }

    return (TRUE);
}

void
send_pim6_bootstrap()
{
    int             datalen;
    mifi_t          mifi;

    if ((datalen = create_pim6_bootstrap_message(pim6_send_buf)))
    {
	for (mifi = 0; mifi < numvifs; mifi++)
	{
	    if (uvifs[mifi].uv_flags & (VIFF_DISABLED | VIFF_DOWN |
					MIFF_REGISTER | VIFF_TUNNEL))
		continue;

   	     send_pim6(pim6_send_buf, &uvifs[mifi].uv_linklocal->pa_addr,
          	       &allpim6routers_group, PIM_BOOTSTRAP, datalen);
	     uvifs[mifi].uv_out_pim6_bootsrap++;
	}
    }
}

/************************************************************************
 *                        PIM_CAND_RP_ADV
 ************************************************************************/
/*
 * minimum length of a cand. RP adv. message;
 * length of PIM header + prefix-cnt(1) + priority(1) + holdtime(2) +
 *                        encoded unicast RP addr(18)
  */
#define PIM6_CAND_RP_ADV_MINLEN (PIM_MINLEN + PIM6_ENCODE_UNI_ADDR_LEN)

/*
 * If I am the Bootstrap router, process the advertisement, otherwise ignore
 * it.
 */
int
receive_pim6_cand_rp_adv(src, dst, pim_message, datalen)
    struct sockaddr_in6         *src,
                    			*dst;
    char           		*pim_message;
    register int    		datalen;
{
    u_int8          		prefix_cnt;
    u_int8          		priority;
    u_int16         		holdtime;
    pim6_encod_uni_addr_t 	cand_rp_addr;
    pim6_encod_grp_addr_t 	encod_grp_addr;
    u_int8         		*data_ptr;
    struct in6_addr         	grp_mask;
    struct sockaddr_in6		group_, rpp_;

    pim6dstat.in_pim6_cand_rp++;

    /* if I am not the bootstrap RP, then do not accept the message */
    if ((cand_bsr_flag != FALSE) && 
	!inet6_equal(&curr_bsr_address, &my_bsr_address))
    {
	log(LOG_NOTICE, 0,
	    "receive_pim6_cand_rp_adv: receive cand_RP from %s "
	    "but I'm not the BSR",
	    inet6_fmt(&src->sin6_addr));
	return (FALSE);
    }

    /* sanity check for the minimum length */
    if (datalen < PIM6_CAND_RP_ADV_MINLEN) {
	    log(LOG_NOTICE, 0,
		"receive_pim6_cand_rp_adv: cand_RP message size(%u) is"
		" too short from %s",
		datalen, inet6_fmt(&src->sin6_addr));
	    return(FALSE);
    }
    datalen -= PIM6_CAND_RP_ADV_MINLEN;

    data_ptr = (u_int8 *) (pim_message + sizeof(struct pim));
    /* Parse the CAND_RP_ADV message */
    GET_BYTE(prefix_cnt, data_ptr);
    GET_BYTE(priority, data_ptr);
    GET_HOSTSHORT(holdtime, data_ptr);
    GET_EUADDR6(&cand_rp_addr, data_ptr);

    /*
     * The RP Address field is set to the globally reachable IPv6 address
     * [draft-ietf-pim-ipv6].
     */
    if (IN6_IS_ADDR_LINKLOCAL(&cand_rp_addr.unicast_addr)) {
	    /* XXX: prohibit a site-local address as well? */
	    log(LOG_WARNING, 0,
		"receive_pim6_cand_rp_adv: non global address(%s) as RP",
		inet6_fmt(&cand_rp_addr.unicast_addr));
	    return(FALSE);
    }

    memset(&rpp_, 0, sizeof(rpp_));
    if (prefix_cnt == 0)
    {
	/* The default ff:: and masklen of 8 */
	MASKLEN_TO_MASK6(ALL_MCAST_GROUPS_LENGTH, grp_mask);
	rpp_.sin6_addr = cand_rp_addr.unicast_addr;
	/*
	 * note that we don't have to take care of scope id, since
	 * the address should be global(see above).
	 */
	add_rp_grp_entry(&cand_rp_list, &grp_mask_list,
			 &rpp_, priority, holdtime,
			 &sockaddr6_d, grp_mask,
			 my_bsr_hash_mask,
			 curr_bsr_fragment_tag);
	return (TRUE);
    }
    while (prefix_cnt--)
    {
	    /*
	     * Sanity check for the message length.
	     * XXX: do we have to do the check at an earlier stage and
	     *      discard the whole message (instead of adopting a part of it)
	     *      if it's bogus?
	     */
	    if (datalen < PIM6_ENCODE_GRP_ADDR_LEN) {
		    log(LOG_NOTICE, 0,
			"receive_pim6_cand_rp_adv: cand_RP message from %s is"
			" too short to contain enough groups",
			inet6_fmt(&src->sin6_addr));
		    return(FALSE);
	    }
	    datalen -= PIM6_ENCODE_GRP_ADDR_LEN;
	    
	    GET_EGADDR6(&encod_grp_addr, data_ptr);
	    MASKLEN_TO_MASK6(encod_grp_addr.masklen, grp_mask);
	    group_.sin6_addr = encod_grp_addr.mcast_addr;
	    group_.sin6_scope_id = 0; /* XXX: what if for scoped multicast addr? */
	    rpp_.sin6_addr = cand_rp_addr.unicast_addr;
	    /* see above note on scope id */

	    add_rp_grp_entry(&cand_rp_list, &grp_mask_list,
			     &rpp_, priority, holdtime,
			     &group_, grp_mask,
			     my_bsr_hash_mask,
			     curr_bsr_fragment_tag);
    }

    return (TRUE);
}

int
send_pim6_cand_rp_adv()
{
    u_int8          		prefix_cnt;
    struct in6_addr  		grp_mask;
    pim6_encod_grp_addr_t 	encod_grp_addr;
    u_int8         		*data_ptr;
    struct sockaddr_in6		group_;

    if (!inet6_valid_host(&curr_bsr_address))
	return (FALSE);		/* No BSR yet */

    if( inet6_equal(&curr_bsr_address, &my_bsr_address))
    {
	/* I am the BSR and have to include my own group_prefix stuff */
	prefix_cnt = *cand_rp_adv_message.prefix_cnt_ptr;
	if (prefix_cnt == 0)
	{
	    /* The default ff00:: and masklen of 8 */
	    MASKLEN_TO_MASK6(ALL_MCAST_GROUPS_LENGTH, grp_mask);
	    add_rp_grp_entry(&cand_rp_list, &grp_mask_list,
			     &my_cand_rp_address, my_cand_rp_priority,
			     my_cand_rp_holdtime,
			     &sockaddr6_d,
			     grp_mask,
			     my_bsr_hash_mask,
			     curr_bsr_fragment_tag);
	    return (TRUE);
	}
	/* TODO: hardcoding!! */
	/* 18 = sizeof(pim6_encod_uni_addr_t) without padding */
	data_ptr = cand_rp_adv_message.buffer + (4 + 18);

	while (prefix_cnt--)
	{
	    GET_EGADDR6(&encod_grp_addr, data_ptr);
	    MASKLEN_TO_MASK6(encod_grp_addr.masklen, grp_mask);
	    group_.sin6_addr = encod_grp_addr.mcast_addr;
	    group_.sin6_scope_id = 0;			/*XXX */
		add_rp_grp_entry(&cand_rp_list,
			     &grp_mask_list,
			     &my_cand_rp_address, my_cand_rp_priority,
			     my_cand_rp_holdtime,
			     &group_, grp_mask,
			     my_bsr_hash_mask,
			     curr_bsr_fragment_tag);
	}
	return (TRUE);
    }

    data_ptr = (u_int8 *) (pim6_send_buf + sizeof(struct pim));
    
    bcopy((char *)cand_rp_adv_message.buffer, (char *) data_ptr,
	  cand_rp_adv_message.message_size);

    send_pim6(pim6_send_buf, &my_cand_rp_address, &curr_bsr_address ,
            PIM_CAND_RP_ADV, cand_rp_adv_message.message_size);
    pim6dstat.out_pim6_cand_rp++;

    return TRUE;
}

/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: prune.c,v 1.2 1994/09/08 02:51:23 wollman Exp $
 */


#include "defs.h"

extern int cache_lifetime;
extern int max_prune_lifetime;

/*
 * dither cache lifetime to obtain a value between x and 2*x
 */
#define CACHE_LIFETIME(x) ((x) + (random() % (x)))

#define CHK_GS(x, y) {	\
		switch(x) { \
			case 2:	\
			case 4:	\
			case 8:	\
			case 16: \
			case 32: \
			case 64: \
			case 128: \
			case 256: y = 1; \
				  break; \
			default:  y = 0; \
		} \
	}
			    
static struct ktable *kernel_rtable;    /* ptr to list of kernel rt entries */
unsigned int kroutes;			/* current number of cache entries  */


/*
 * Initialize the kernel table structure
 */
void init_ktable()
{
    kernel_rtable 	= NULL;
    kroutes		= 0;
}

/* 
 * Determine if mcastgrp has a listener on vifi
 */
int grplst_mem(vifi, mcastgrp)
    vifi_t vifi;
    u_long mcastgrp;
{
    register struct listaddr *g;
    register struct uvif *v;
    
    v = &uvifs[vifi];
    
    for (g = v->uv_groups; g != NULL; g = g->al_next)
	if (mcastgrp == g->al_addr) 
	    return 1;
    
    return 0;
}

/* 
 * Updates the ttl values for each vif.
 */
void prun_add_ttls(kt)
    struct ktable *kt;
{
    struct uvif *v;
    vifi_t vifi;
    
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (VIFM_ISSET(vifi, kt->kt_grpmems))
	    kt->kt_ttls[vifi] = v->uv_threshold;
	else 
	    kt->kt_ttls[vifi] = NULL;
    }
}

/*
 * checks for scoped multicast addresses
 */
#define GET_SCOPE(kt) { \
	register int _i; \
	if (((kt)->kt_mcastgrp & 0xff000000) == 0xef000000) \
	    for (_i = 0; _i < numvifs; _i++) \
		if (scoped_addr(_i, (kt)->kt_mcastgrp)) \
		    VIFM_SET(_i, (kt)->kt_scope); \
	}

int scoped_addr(vifi, addr)
    vifi_t vifi;
    u_long addr;
{
    struct vif_acl *acl;

    for (acl = uvifs[vifi].uv_acl; acl; acl = acl->acl_next)
	if ((addr & acl->acl_mask) == acl->acl_addr)
	    return 1;

    return 0;
}

/* 
 * Add a new table entry for (origin, mcastgrp)
 */
void add_table_entry(origin, mcastgrp)
    u_long origin;
    u_long mcastgrp;
{
    struct rtentry *r;
    struct ktable *kt;
    int i;
    
    if ((kt = find_src_grp(origin, mcastgrp)) != NULL) {
	log(LOG_DEBUG, 0, "kernel entry exists for (%s %s)",
	    inet_fmt(origin, s1), inet_fmt(mcastgrp, s2));
	return;
    }
    
    r = determine_route(origin);
    
    /* allocate space for the new entry */
    kt = (struct ktable *)malloc(sizeof(struct ktable));
    if (kt == NULL)
	log(LOG_ERR, 0, "ran out of memory");   /* fatal */
    
    kroutes++;
    
    /* add the new values in */
    if (r == NULL) {
	kt->kt_origin 		= origin;
	kt->kt_mcastgrp		= mcastgrp;
	kt->kt_originmask	= 0xffffffff;
	kt->kt_parent		= NO_VIF;
	kt->kt_gateway		= 0;
	kt->kt_children 	= 0;
	kt->kt_leaves		= 0;
	kt->kt_timer		= CACHE_LIFETIME(cache_lifetime);
	kt->kt_grpmems 		= 0;
	kt->kt_rlist		= NULL;
	kt->kt_prsent_timer	= 0;
	kt->kt_grftsnt		= 0;
	kt->kt_prun_count	= 0;
	kt->kt_scope		= 0;
    }
    else {
	kt->kt_origin      	= r->rt_origin;
	kt->kt_mcastgrp    	= mcastgrp;
	kt->kt_originmask  	= r->rt_originmask;
	kt->kt_parent      	= r->rt_parent;
	kt->kt_gateway		= r->rt_gateway;
	kt->kt_timer	        = CACHE_LIFETIME(cache_lifetime);
	kt->kt_grpmems		= 0;
	kt->kt_rlist		= NULL;
	kt->kt_prsent_timer	= 0;
	kt->kt_grftsnt		= 0;
	kt->kt_prun_count	= 0;
	kt->kt_scope		= 0;

	VIFM_COPY(r->rt_children, kt->kt_children);
	VIFM_COPY(r->rt_leaves,   kt->kt_leaves);

	/* obtain the multicast group membership list */
	for (i = 0; i < numvifs; i++) {
	    if (VIFM_ISSET(i, kt->kt_children) && 
		!(VIFM_ISSET(i, kt->kt_leaves)))
		VIFM_SET(i, kt->kt_grpmems);
	    
	    if (VIFM_ISSET(i, kt->kt_leaves) && grplst_mem(i, mcastgrp))
		VIFM_SET(i, kt->kt_grpmems);
	}
	GET_SCOPE(kt);
	if (VIFM_ISSET(kt->kt_parent, kt->kt_scope))
	    kt->kt_grpmems = NULL;
	else
	    kt->kt_grpmems &= ~kt->kt_scope;
    }
    
    /* update the kernel_rtable pointer */
    kt->kt_next 	= kernel_rtable;
    kernel_rtable 	= kt;
    
    /* update ttls and add entry into kernel */
    prun_add_ttls(kt);
    k_add_rg(kt);
    
    log(LOG_DEBUG, 0, "add entry s:%x g:%x gm:%x",
	kt->kt_origin, kt->kt_mcastgrp, kt->kt_grpmems);
    
    /* If there are no leaf vifs
     * which have this group, then
     * mark this src-grp as a prune candidate. 
     * One thing to do is to check if parent vif is the source
     * and not send a prune to that.
     */
    if (!kt->kt_grpmems && kt->kt_gateway)
	send_prune(kt);
}

/*
 * An mrouter has gone down and come up on an interface
 * Forward on that interface immediately
 */
void reset_neighbor_state(vifi, addr)
    vifi_t vifi;
    u_long addr;
{
    struct ktable *prev_kt, *kt;
    struct prunlst *prev_krl, *krl;
    
    /* Check each src-grp entry to see if it was pruned on that interface
       If so, forward on that interface */
    for (prev_kt = (struct ktable *)&kernel_rtable,
	 kt = kernel_rtable; kt;
	 prev_kt = kt, kt = kt->kt_next) {
	for (prev_krl = (struct prunlst *)&kt->kt_rlist, 
	     krl = prev_krl->rl_next;
	     krl;
	     prev_krl = krl, krl = krl->rl_next) {
	    if (krl->rl_router == addr) {
		prev_krl->rl_next = krl->rl_next;
		free(krl);
		krl = prev_krl;
		kt->kt_prun_count--;
	    }
	}

	/*
	 * If neighbor was the parent, remove the prune sent state
	 * Don't send any grafts upstream.
	 */
	if (vifi == kt->kt_parent) {
	    k_del_rg(kt);
	    prev_kt->kt_next = kt->kt_next;
	    while (krl = kt->kt_rlist) {
		kt->kt_rlist = krl->rl_next;
		free((char *)krl);
	    }
	    free((char *)kt);
	    kt = prev_kt;
	    kroutes--;
	    continue;
	}

	/*
	 * Neighbor was not the parent, send grafts to join the groups
	 */
	if (kt->kt_prsent_timer) {
	    kt->kt_grftsnt = 1;
	    send_graft(kt);
	    kt->kt_prsent_timer = 0;
	}

	if (!VIFM_ISSET(vifi, kt->kt_grpmems)) {
	    if (VIFM_ISSET(vifi, kt->kt_children) && 
		!(VIFM_ISSET(vifi, kt->kt_leaves)))
		VIFM_SET(vifi, kt->kt_grpmems);
	    
	    if (VIFM_ISSET(vifi, kt->kt_leaves) && 
		grplst_mem(vifi, kt->kt_mcastgrp))
		VIFM_SET(vifi, kt->kt_grpmems);
	    
	    kt->kt_grpmems &= ~kt->kt_scope;
	    prun_add_ttls(kt);
	    k_add_rg(kt);
	}
    }
}

/*
 * Delete table entry from the kernel
 * del_flag determines how many entries to delete
 */
void del_table_entry(r, mcastgrp, del_flag)
    struct rtentry *r;
    u_long mcastgrp;
    u_int  del_flag;
{
    struct mfcctl  mc;
    struct ktable *kt, *prev_kt;
    struct prunlst *krl;
    
    if (del_flag == DEL_ALL_ROUTES) {
	for (prev_kt = (struct ktable *)&kernel_rtable;
	     kt = prev_kt->kt_next;
	     prev_kt = kt) {
	    if ((kt->kt_origin & r->rt_originmask) == r->rt_origin) {
		log(LOG_DEBUG, 0, "delete all rtes %x grp %x", 
		    kt->kt_origin, mcastgrp);
		
		k_del_rg(kt);
		
		/* free prun list entries */
		while (kt->kt_rlist) {
		    krl = kt->kt_rlist;
		    kt->kt_rlist = krl->rl_next;
		    free((char *)krl);
		}
		
		/* free the source mcastgrp entry */
		prev_kt->kt_next = kt->kt_next;
		free((char *)kt);
		kroutes--;
		kt = prev_kt;
	    }
	}
    }
    
    if (del_flag == DEL_RTE_GROUP) {
	for (prev_kt = (struct ktable *)&kernel_rtable;
	     (prev_kt) && (kt = prev_kt->kt_next);
	     prev_kt = kt) {
	    if ((kt->kt_origin & r->rt_originmask) == r->rt_origin &&
		kt->kt_mcastgrp == mcastgrp) {
		log(LOG_DEBUG, 0, "delete src %x grp %x", 
		    kt->kt_origin, mcastgrp);
		
		k_del_rg(kt);
		
		/* free prun list entries */
		while (kt->kt_rlist) {
		    krl = kt->kt_rlist;
		    kt->kt_rlist = krl->rl_next;
		    free((char *)krl);
		}
		
		/* free the source mcastgrp entry */
		prev_kt->kt_next = kt->kt_next;
		free((char *)kt);
		kroutes--;
		break;
	    }
	}
    }
}

/*
 * update kernel table entry when a route entry changes
 */
void update_table_entry(r)
    struct rtentry *r;
{
    struct ktable 	*kt;
    struct prunlst 	*krl;
    int i;
    int changed;
    
    for (kt = kernel_rtable; kt; kt = kt->kt_next)
	if ((kt->kt_origin & r->rt_originmask)== r->rt_origin) {
	    changed = 0;
	    
	    if (kt->kt_leaves != r->rt_leaves)
		changed++;
	    if (kt->kt_children != r->rt_children)
		changed++;
	    if (kt->kt_parent != r->rt_parent)
		changed++;
	    
	    if (!changed)
		continue;
	    
	    log(LOG_DEBUG, 0, "update entry: s %-15s g %-15s",
		inet_fmt(kt->kt_origin, s1), inet_fmt(kt->kt_mcastgrp, s2));
	    
	    /* free prun list entries */
	    while (kt->kt_rlist) {
		krl = kt->kt_rlist;
		kt->kt_rlist = krl->rl_next;
		free((char *)krl);
	    }
	    
	    kt->kt_parent	= r->rt_parent;
	    kt->kt_gateway	= r->rt_gateway;
	    kt->kt_grpmems 	= 0;
	    kt->kt_prun_count	= 0;
	    VIFM_COPY(r->rt_children, kt->kt_children);
	    VIFM_COPY(r->rt_leaves,   kt->kt_leaves);
	    
	    /* obtain the multicast group membership list */
	    for (i = 0; i < numvifs; i++) {
		if (VIFM_ISSET(i, kt->kt_children) && 
		    !(VIFM_ISSET(i, kt->kt_leaves)))
		    VIFM_SET(i, kt->kt_grpmems);
		
		if (VIFM_ISSET(i, kt->kt_leaves) && grplst_mem(i, kt->kt_mcastgrp))
		    VIFM_SET(i, kt->kt_grpmems);
	    }
	    if (VIFM_ISSET(kt->kt_parent, kt->kt_scope))
		kt->kt_grpmems = NULL;
	    else
		kt->kt_grpmems &= ~kt->kt_scope;
	    
	    if (kt->kt_grpmems && kt->kt_prsent_timer) {
		kt->kt_grftsnt = 1;
		send_graft(kt);
		kt->kt_prsent_timer = 0;
	    }
	    
	    /* update ttls and add entry into kernel */
	    prun_add_ttls(kt);
	    k_add_rg(kt);
	    
	    if (!kt->kt_grpmems && kt->kt_gateway) {
		kt->kt_timer = CACHE_LIFETIME(cache_lifetime);
		send_prune(kt);
	    }	    
	}
}



/*
 * set the forwarding flag for all mcastgrps on this vifi
 */
void update_lclgrp(vifi, mcastgrp)
    vifi_t vifi;
    u_long mcastgrp;
{
    struct ktable *kt;
    
    log(LOG_DEBUG, 0, "group %x joined at vif %d", mcastgrp, vifi);
    
    for (kt = kernel_rtable; kt; kt = kt->kt_next)
	if (kt->kt_mcastgrp == mcastgrp && VIFM_ISSET(vifi, kt->kt_children)) {
	    VIFM_SET(vifi, kt->kt_grpmems);
	    kt->kt_grpmems &= ~kt->kt_scope;
	    if (kt->kt_grpmems == NULL)
		continue;
	    prun_add_ttls(kt);
	    k_add_rg(kt);
	}	
}

/*
 * reset forwarding flag for all mcastgrps on this vifi
 */
void delete_lclgrp(vifi, mcastgrp)
    vifi_t vifi;
    u_long mcastgrp;
{
    
    struct ktable *kt;
    
    log(LOG_DEBUG, 0, "group %x left at vif %d", mcastgrp, vifi);
    
    for (kt = kernel_rtable; kt; kt = kt->kt_next)
	if (kt->kt_mcastgrp == mcastgrp)  {
	    VIFM_CLR(vifi, kt->kt_grpmems);
	    prun_add_ttls(kt);
	    k_add_rg(kt);
	    
	    /*
	     * If there are no more members of this particular group,
	     *  send prune upstream
	     */
	    if (kt->kt_grpmems == NULL && kt->kt_gateway)
		send_prune(kt);
	}	
}

/*
 * Check if the neighbor supports pruning
 */
int pruning_neighbor(vifi, addr)
    vifi_t vifi;
    u_long addr;
{
    struct listaddr *u;
    
    for (u = uvifs[vifi].uv_neighbors; u; u = u->al_next)
	if ((u->al_addr == addr) && (u->al_pv > 2))
	    return 1;

    return 0;
}

/*
 * Send a prune message to the upstream router
 * given by the kt->kt_gateway argument. The origin and 
 * multicast group can be determined from the kt 
 * structure.
 *
 * Also, record an entry that a prune was sent for this group
 */
void send_prune(kt)
    struct ktable *kt;
{
    struct prunlst *krl;
    char *p;
    int i;
    int datalen;
    u_long src;
    u_long dst;
    
    /* Don't process any prunes if router is not pruning */
    if (pruning == 0)
	return;
    
    /* Don't send a prune to a non-pruning router */
    if (!pruning_neighbor(kt->kt_parent, kt->kt_gateway))
	return;
    
    /* 
     * sends a prune message to the router upstream.
     */
    src = uvifs[kt->kt_parent].uv_lcl_addr;
    dst = kt->kt_gateway;
    
    p = send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN;
    datalen = 0;
    
    /*
     * determine prune lifetime
     */
    kt->kt_prsent_timer = kt->kt_timer;
    for (krl = kt->kt_rlist; krl; krl = krl->rl_next)
	if (krl->rl_timer < kt->kt_prsent_timer)
	    kt->kt_prsent_timer = krl->rl_timer;
    
    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(kt->kt_origin))[i];
    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(kt->kt_mcastgrp))[i];
#if BYTE_ORDER == BIG_ENDIAN
    for (i = 0; i < 4; i++)
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
    for (i = 3; i >= 0; i--)
#endif
	*p++ = ((char *)&(kt->kt_prsent_timer))[i];
    datalen += 12;
    
    send_igmp(src, dst, IGMP_DVMRP, DVMRP_PRUNE,
	      htonl(MROUTED_LEVEL), datalen);
    
    /*	log(LOG_DEBUG, 0, "send prune for src:%x, grp:%x up to %x",
	kt->kt_origin, kt->kt_mcastgrp, kt->kt_gateway);*/
}

/*
 * Takes the prune message received and then strips it to
 * determine the (src, grp) pair to be pruned.
 *
 * Adds the router to the (src, grp) entry then.
 *
 * Determines if further packets have to be sent down that vif
 *
 * Determines if a corresponding prune message has to be generated
 */
void accept_prune(src, dst, p, datalen)
    u_long src;
    u_long dst;
    char *p;
    int datalen;
{
    u_long prun_src;
    u_long prun_dst;
    u_long prun_tmr;
    vifi_t vifi;
    int i;
    int stop_sending; 
    struct ktable *kt;
    struct prunlst *pr_recv;
    struct prunlst *krl;
    struct listaddr *vr;
    
    /* Don't process any prunes if router is not pruning */
    if (pruning == 0)
	return;
    
    if ((vifi = find_vif(src, dst)) == NO_VIF) {
	log(LOG_INFO, 0,
    	    "ignoring prune report from non-neighbor %s", inet_fmt(src, s1));
	return;
    }
    
    if (datalen < 0  || datalen > 12)
	{
	    log(LOG_WARNING, 0,
		"received non-decipherable prune report from %s", inet_fmt(src, s1));
	    return;
	}
    
    for (i = 0; i< 4; i++)
	((char *)&prun_src)[i] = *p++;
    for (i = 0; i< 4; i++)
	((char *)&prun_dst)[i] = *p++;
#if BYTE_ORDER == BIG_ENDIAN
    for (i = 0; i< 4; i++)
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
    for (i = 3; i >= 0; i--)
#endif
	((char *)&prun_tmr)[i] = *p++;
    
    kt = find_src_grp(prun_src, prun_dst);
    
    if (kt == NULL) 
	{
	    log(LOG_WARNING, 0, "prune message received incorrectly");
	    return;
	}
    
    if (!VIFM_ISSET(vifi, kt->kt_children))
	{
	    log(LOG_INFO, 0,
		"ignoring prune report from non-child %s", inet_fmt(src, s1));
	    return;
	}
    if (VIFM_ISSET(vifi, kt->kt_scope)) {
	log(LOG_INFO, 0,
	    "ignoring prune report from %s on scoped vif %d",
	    inet_fmt(src, s1), vifi);
	return;
    }
    /* check if prune has been received from this source */
    if (!no_entry_exists(src, kt))
	{
	    log(LOG_INFO, 0, "duplicate prune from %s", inet_fmt(src, s1));
	    return;
	}
    
    log(LOG_DEBUG, 0, "%s on vif %d prunes (%s %s) tmr %d",
	inet_fmt(src, s1), vifi,
	inet_fmt(prun_src, s2), inet_fmt(prun_dst, s3), prun_tmr);
    
    /* allocate space for the prune structure */
    pr_recv = (struct prunlst *)(malloc(sizeof(struct prunlst)));
    
    if (pr_recv == NULL)
	log(LOG_ERR, 0, "pr_recv: ran out of memory");
    
    pr_recv->rl_vifi	= vifi;
    pr_recv->rl_router	= src;
    pr_recv->rl_timer	= prun_tmr;
    
    /* 
     * add this prune message to the list of prunes received 
     * for this src group pair 
     */
    pr_recv->rl_next = kt->kt_rlist;
    kt->kt_rlist = pr_recv;
    
    kt->kt_prun_count++;
    kt->kt_timer = CACHE_LIFETIME(cache_lifetime);
    if (kt->kt_timer < prun_tmr)
	kt->kt_timer = prun_tmr;
    
    /*
     * check if any more packets need to be sent on the 
     * vif which sent this message
     */
    for (vr = uvifs[vifi].uv_neighbors, stop_sending = 1;
	 vr; vr = vr->al_next)
	if (no_entry_exists(vr->al_addr, kt))  {
	    stop_sending = 0;
	    break;
	}
    
    if (stop_sending && !grplst_mem(vifi, prun_dst)) {
	VIFM_CLR(vifi, kt->kt_grpmems);
	prun_add_ttls(kt);
	k_add_rg(kt);
    }
    
    /*
     * check if all the child routers have expressed no interest
     * in this group and if this group does not exist in the 
     * interface
     * Send a prune message then upstream
     */
    if(kt->kt_grpmems == NULL && kt->kt_gateway) {
	log(LOG_DEBUG, 0, "snt prun up %d %d", kt->kt_prun_count, rtr_cnt(kt));
	send_prune(kt);
    }
}

/*
 * Returns 1 if router vr is not present in the prunlist of kt
 */
int no_entry_exists(vr, kt)
    u_long vr;
    struct ktable *kt;
{
    struct prunlst *krl;
    
    for (krl = kt->kt_rlist; krl; krl = krl->rl_next)
	if (krl->rl_router == vr)
	    return 0;
    
    return 1;
}

/*
 * Finds the entry for the source group pair in the table
 */
struct ktable *find_src_grp(src, grp)
    u_long src;
    u_long grp;
{
    struct ktable *kt;
    
    for (kt = kernel_rtable; kt; kt = kt->kt_next)
	if ((kt->kt_origin == (src & kt->kt_originmask)) && 
	    (kt->kt_mcastgrp == grp))
	    return kt;
    
    return NULL;
}

/* 
 * scans through the neighbor list of this router and then
 * determines the total no. of child routers present
 */
int rtr_cnt(kt)
    struct ktable *kt;
{
    int ri;
    int rcount = 0;
    struct listaddr *u;
    
    for (ri = 0; ri < numvifs; ri++)
	if (VIFM_ISSET(ri, kt->kt_children))
	    for(u = uvifs[ri].uv_neighbors; u; u = u->al_next)
		rcount++;
    
    return rcount;
}

/*
 * Checks if this mcastgrp is present in the kernel table
 * If so and if a prune was sent, it sends a graft upwards
 */
void chkgrp_graft(vifi, mcastgrp)
    vifi_t	vifi;
    u_long	mcastgrp;
{
    struct ktable *kt;
    
    for (kt = kernel_rtable; kt; kt = kt->kt_next)
	if (kt->kt_mcastgrp == mcastgrp && VIFM_ISSET(vifi, kt->kt_children))
	    if (kt->kt_prsent_timer) {
		VIFM_SET(vifi, kt->kt_grpmems);
		/*
		 * If the vif that was joined was a scoped vif,
		 * ignore it ; don't graft back
		 */
		kt->kt_grpmems &= ~kt->kt_scope;
		if (kt->kt_grpmems == NULL)
		    continue;

		/* set the flag for graft retransmission */
		kt->kt_grftsnt = 1;
		
		/* send graft upwards */
		send_graft(kt);
		
		/* reset the prune timer and update cache timer*/
		kt->kt_prsent_timer = 0;
		kt->kt_timer = max_prune_lifetime;
		
		prun_add_ttls(kt);
		k_add_rg(kt);
	    }
}

/* determine the multicast group and src
 * 
 * if it does, then determine if a prune was sent 
 * upstream.
 * if prune sent upstream, send graft upstream and send
 * ack downstream.
 * 
 * if no prune sent upstream, change the forwarding bit
 * for this interface and send ack downstream.
 *
 * if no entry exists for this group just ignore the message
 * [this may not be the right thing to do. but lets see what 
 * happens for the time being and then we might decide to do
 * a modification to the code depending on the type of behaviour 
 * that we see in this]
 */
void accept_graft(src, dst, p, datalen)
    u_long 	src;
    u_long  dst;
    char	*p;
    int		datalen;
{
    vifi_t 	vifi;
    u_long 	prun_src;
    u_long	prun_dst;
    struct ktable *kt;
    int 	i;
    struct 	prunlst *krl;
    struct	prunlst *prev_krl;
    
    if ((vifi = find_vif(src, dst)) == NO_VIF) {
	log(LOG_INFO, 0,
    	    "ignoring graft report from non-neighbor %s", inet_fmt(src, s1));
	return;
    }
    
    if (datalen < 0  || datalen > 8) {
	log(LOG_WARNING, 0,
	    "received non-decipherable graft report from %s", inet_fmt(src, s1));
	return;
    }
    
    for (i = 0; i< 4; i++)
	((char *)&prun_src)[i] = *p++;
    for (i = 0; i< 4; i++)
	((char *)&prun_dst)[i] = *p++;
    
    log(LOG_DEBUG, 0, "%s on vif %d grafts (%s %s)",
	inet_fmt(src, s1), vifi,
	inet_fmt(prun_src, s2), inet_fmt(prun_dst, s3));
    
    kt = find_src_grp(prun_src, prun_dst);
    if (kt == NULL) {
	log(LOG_DEBUG, 0, "incorrect graft received from %s", inet_fmt(src, s1));
	return;
    }
    
    if (VIFM_ISSET(vifi, kt->kt_scope)) {
	log(LOG_INFO, 0,
	    "incorrect graft received from %s on scoped vif %d",
	    inet_fmt(src, s1), vifi);
	return;
    }
    /* remove prune entry from the list 
     * allow forwarding on that vif, make change in the kernel
     */
    for (prev_krl = (struct prunlst *)&kt->kt_rlist;
	 krl = prev_krl->rl_next; 
	 prev_krl = krl)
	if ((krl->rl_vifi) == vifi && (krl->rl_router == src)) {
	    prev_krl->rl_next = krl->rl_next;
	    free((char *)krl);
	    krl = prev_krl;
	    
	    kt->kt_prun_count--;
	    VIFM_SET(vifi, kt->kt_grpmems);
	    prun_add_ttls(kt);
	    k_add_rg(kt);
	    break;				
	}
    
    /* send ack downstream */
    send_graft_ack(kt, src);
    kt->kt_timer = max_prune_lifetime;
    
    if (kt->kt_prsent_timer) {
	/* set the flag for graft retransmission */
	kt->kt_grftsnt = 1;

	/* send graft upwards */
	send_graft(kt);

	/* reset the prune sent timer */
	kt->kt_prsent_timer = 0;
    }
}

/*
 * Send an ack that a graft was received
 */
void send_graft_ack(kt, to)
    struct ktable *kt;
    u_long to;
{
    register char *p;
    register int i;
    int datalen;
    u_long src;
    u_long dst;
    
    src = uvifs[kt->kt_parent].uv_lcl_addr;
    dst = to;
    
    p = send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN;
    datalen = 0;
    
    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(kt->kt_origin))[i];
    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(kt->kt_mcastgrp))[i];
    datalen += 8;
    
    send_igmp(src, dst, IGMP_DVMRP, DVMRP_GRAFT_ACK,
	      htonl(MROUTED_LEVEL), datalen);
    
    log(LOG_DEBUG, 0, "send graft ack for src:%x, grp:%x to %x",
	kt->kt_origin, kt->kt_mcastgrp, dst);
}

/*
 * a prune was sent upstream
 * so, a graft has to be sent to annul the prune
 * set up a graft timer so that if an ack is not 
 * heard within that time, another graft request
 * is sent out.
 */
void send_graft(kt)
    struct ktable *kt;
{
    register char *p;
    register int i;
    int datalen;
    u_long src;
    u_long dst;
    
    src = uvifs[kt->kt_parent].uv_lcl_addr;
    dst = kt->kt_gateway;
    
    p = send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN;
    datalen = 0;
    
    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(kt->kt_origin))[i];
    for (i = 0; i < 4; i++)
	*p++ = ((char *)&(kt->kt_mcastgrp))[i];
    datalen += 8;
    
    if (datalen != 0) {
	send_igmp(src, dst, IGMP_DVMRP, DVMRP_GRAFT,
		  htonl(MROUTED_LEVEL), datalen);
    }
    log(LOG_DEBUG, 0, "send graft for src:%x, grp:%x up to %x",
	kt->kt_origin, kt->kt_mcastgrp, kt->kt_gateway);
}

/*
 * find out which group is involved first of all 
 * then determine if a graft was sent.
 * if no graft sent, ignore the message
 * if graft was sent and the ack is from the right 
 * source, remove the graft timer so that we don't 
 * have send a graft again
 */
void accept_g_ack(src, dst, p, datalen)
    u_long 	src;
    u_long  dst;
    char	*p;
    int		datalen;
{
    vifi_t 	vifi;
    u_long 	grft_src;
    u_long	grft_dst;
    struct ktable *kt;
    int 	i;
    
    if ((vifi = find_vif(src, dst)) == NO_VIF) {
	log(LOG_INFO, 0,
    	    "ignoring graft ack report from non-neighbor %s", inet_fmt(src, s1));
	return;
    }
    
    if (datalen < 0  || datalen > 8) {
	log(LOG_WARNING, 0,
	    "received non-decipherable graft ack report from %s", inet_fmt(src, s1));
	return;
    }
    
    for (i = 0; i< 4; i++)
	((char *)&grft_src)[i] = *p++;
    for (i = 0; i< 4; i++)
	((char *)&grft_dst)[i] = *p++;
    
    log(LOG_DEBUG, 0, "%s on vif %d acks graft (%s %s)",
	inet_fmt(src, s1), vifi,
	inet_fmt(grft_src, s2), inet_fmt(grft_dst, s3));
    
    kt = find_src_grp(grft_src, grft_dst);
    
    if (kt == NULL) {
	log(LOG_WARNING, 0, "received wrong graft ack from %s", inet_fmt(src, s1));
	return;
    }
    
    if (kt->kt_grftsnt)
	kt->kt_grftsnt = 0;
}


/*
 * free all prune entries
 */
void free_all_prunes()
{
    register struct ktable *kt;
    register struct prunlst *krl;
    
    while (kernel_rtable != NULL) {
	kt = kernel_rtable;
	kernel_rtable = kt->kt_next;

	while (kt->kt_rlist != NULL) {
	    krl = kt->kt_rlist;
	    kt->kt_rlist = krl->rl_next;
	    free((char *)krl);
	}

	free((char *)kt);
	kroutes--;
    }
}


/*
 * Advance the timers on all the cache entries.
 * If there are any entries whose timers have expired,
 * remove these entries from the kernel cache.
 */
void age_table_entry()
{
    struct ktable *kt;
    struct ktable *prev_kt;
    struct prunlst *krl;
    struct prunlst *prev_krl;
    
    log(LOG_DEBUG, 0, "kr:%x pr:%x",
	kernel_rtable, (struct ktable *)&kernel_rtable);
    
    for (prev_kt = (struct ktable *)&kernel_rtable;
	 kt = prev_kt->kt_next;
	 prev_kt = kt) {
	/* advance the timer for the kernel entry */
	kt->kt_timer -= ROUTE_MAX_REPORT_DELAY;

	/* decrement prune timer if need be */
	if (kt->kt_prsent_timer)
	    kt->kt_prsent_timer -= ROUTE_MAX_REPORT_DELAY;

	/* retransmit graft if graft sent flag is still set */
	if (kt->kt_grftsnt) {
	    register int y;
	    CHK_GS(kt->kt_grftsnt++, y);
	    if (y)
		send_graft(kt);
	}

	/* delete the entry only if there are no subordinate 
	   routers
	   
	   Now, if there are subordinate routers, then, what we 
	   have to do is to decrement each and every router's 
	   time entry too and decide if we want to forward on
	   that link basically
	   */
	for (prev_krl = (struct prunlst *)&kt->kt_rlist,
	     krl = prev_krl->rl_next;
	     krl;
	     prev_krl = krl, krl = krl->rl_next) {
	    if ((krl->rl_timer -= ROUTE_MAX_REPORT_DELAY) <= 0) {
              log(LOG_DEBUG, 0, "forw again s %x g %x on vif %d",
		    kt->kt_origin, kt->kt_mcastgrp, krl->rl_vifi);
		
		if (!VIFM_ISSET(krl->rl_vifi, kt->kt_grpmems)) {
		    VIFM_SET(krl->rl_vifi, kt->kt_grpmems);
		    prun_add_ttls(kt);
		    k_add_rg(kt);
		}
		
		kt->kt_prun_count--;
		prev_krl->rl_next = krl->rl_next;
		free((char *)krl);
		krl = prev_krl;
		
		if (krl == NULL)
		    break;
	    }
	}

	if (kt->kt_timer <= 0) {
	    /*
	     * If there are prune entries still outstanding, 
	     * update the cache timer otherwise expire entry.
	     */
	    if (kt->kt_rlist) {
		kt->kt_timer = CACHE_LIFETIME(cache_lifetime);
	    }
	    else {
		log(LOG_DEBUG, 0, "age route s %x g %x",
		    kt->kt_origin, kt->kt_mcastgrp);
		
		k_del_rg(kt);
		prev_kt->kt_next = kt->kt_next;
		
		/* free all the prune list entries */
		krl = kt->kt_rlist;
		while(krl) {
		    prev_krl = krl;
		    krl = krl->rl_next;
		    free((char *)prev_krl);
		}
		
		free((char *)kt);
		kroutes--;
		kt = prev_kt;
	    }
	}
    }
}

/*
 * Print the contents of the routing table on file 'fp'.
 */
void dump_cache(fp2)
    FILE *fp2;
{
    register struct ktable *kt;
    register struct prunlst *krl;
    register int i;
    register int count;

    fprintf(fp2,
	    "Multicast Routing Cache Table (%d entries)\n%s", kroutes,
	    " Origin-Subnet   Mcast-group      CTmr IVif Prcv# Psnt Forwvifs\n");
    
    for (kt = kernel_rtable, count = 0; kt != NULL; kt = kt->kt_next) {

	fprintf(fp2, " %-15s %-15s",
		inet_fmts(kt->kt_origin, kt->kt_originmask, s1),
		inet_fmt(kt->kt_mcastgrp, s2));

	if (VIFM_ISSET(kt->kt_parent, kt->kt_scope)) {
	    fprintf(fp2, " %5u  %2ub %3u    %c  ",
		    kt->kt_timer, kt->kt_parent, kt->kt_prun_count,
		    kt->kt_prsent_timer ? 'P' : ' ');
	    fprintf(fp2, "\n");
	    continue;
	}
	else
	fprintf(fp2, " %5u  %2u  %3u    %c  ",
		kt->kt_timer, kt->kt_parent, kt->kt_prun_count,
		kt->kt_prsent_timer ? 'P' : ' ');

	for (i = 0; i < numvifs; ++i) {
	    if (VIFM_ISSET(i, kt->kt_grpmems))
		fprintf(fp2, " %u ", i);
	    else if (VIFM_ISSET(i, kt->kt_children) &&
		     !VIFM_ISSET(i, kt->kt_leaves) &&
		     VIFM_ISSET(i, kt->kt_scope))
		fprintf(fp2, " %u%c", i, 'b');
	    else if (VIFM_ISSET(i, kt->kt_children) &&
		     !VIFM_ISSET(i, kt->kt_leaves))
		fprintf(fp2, " %u%c", i, 'p');
	}
	fprintf(fp2, "\n");
	count++;
    }
}


/*
 * Checks if there are any routers that can understand traceroute
 * downstream
 */
int can_forward(vifi)
    vifi_t vifi;
{
    struct listaddr *u;
    
    for (u = uvifs[vifi].uv_neighbors; u; u = u->al_next)
	if (((u->al_pv > 2) && (u->al_mv > 2)) ||
	    (u->al_pv > 3))
	    return 1;
    
    return 0;
}

/*
 * Traceroute function which returns traceroute replies to the requesting
 * router. Also forwards the request to downstream routers.
 */
void mtrace(src, dst, group, data, no, datalen)
    u_long src;
    u_long dst;
    u_long group;
    char *data;
    u_char no;
    int datalen;
{
    u_char type;
    struct rtentry *rt;
    struct tr_query *qry;
    struct tr_resp  *resp;
    struct uvif *v;
    int vifi;
    char *p;
    struct ktable *kt;
    int rcount;

    struct timeval tp;
    struct timezone tzp;
    struct sioc_vif_req v_req;
    struct sioc_sg_req sg_req;

    /* timestamp the request/response */
    gettimeofday(&tp, &tzp);

    /*
     * Check if it is a query or a response
     */
    if (datalen == QLEN) {
	type = QUERY;
	printf("Traceroute query rcvd\n");
    }
    else if ((datalen - QLEN)%RLEN == 0) {
	type = RESP;
	printf("Traceroute response rcvd\n");
    }
    else {
	printf("Non decipherable trace request %s", inet_fmt(src, s1));
	return;
    }

    qry = (struct tr_query *)data;

    /*
     * if it is a multicast packet with all reports filled, drop it
     */
    if ((rcount = (datalen - QLEN)/RLEN) == no) {
	printf("multicast packet with reports filled in\n");
	return;
    }

    printf("s: %s g: %s d: %s ", inet_fmt(qry->tr_src, s1),
	   inet_fmt(group, s2), inet_fmt(qry->tr_dst, s3));
    printf("rttl: %d rd: %s\n", qry->tr_rttl, inet_fmt(qry->tr_raddr, s1));
    printf("rcount:%d\n", rcount);

    /* determine the routing table entry for this traceroute */
    rt = determine_route(qry->tr_src);

    /*
     * Query type packet - check if rte exists 
     * Check if the query destination is a vif connected to me.
     * and if so, whether I should start response back
     */
    if (type == QUERY) {
	if (rt == NULL) {
	    printf("Mcast traceroute: no route entry %s\n",
		   inet_fmt(qry->tr_src, s1));
	    if (IN_MULTICAST(ntohl(dst)))
		return;
	}
	for (v = uvifs, vifi = 0; vifi < numvifs; ++vifi, ++v)
	    if (!(v->uv_flags & VIFF_TUNNEL) &&
		((qry->tr_dst & v->uv_subnetmask) == v->uv_subnet))
		break;
	
	if (vifi == numvifs) {
	    printf("Destination %s not an interface\n",
		   inet_fmt(qry->tr_dst, s1));
	    return;
	}
	if (rt != NULL && !VIFM_ISSET(vifi, rt->rt_children)) {
	    printf("Destination %s not on forwarding tree for src %s\n",
		   inet_fmt(qry->tr_dst, s1), inet_fmt(qry->tr_src, s2));
	    return;
	}
    }
    else {
	/*
	 * determine which interface the packet came in on
	 */
	if ((vifi = find_vif(src, dst)) == NO_VIF) {
	    printf("Wrong interface for packet\n");
	    return;
	}
    }   
    
    printf("Sending traceroute response\n");
    
    /* copy the packet to the sending buffer */
    p = send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN;
    
    bcopy(data, p, datalen);
    
    p += datalen;
    
    /*
     * fill in initial response fields
     */
    resp = (struct tr_resp *)p;
    resp->tr_qarr    = ((tp.tv_sec & 0xffff) << 16) + 
				((tp.tv_usec >> 4) & 0xffff);

    resp->tr_vifin   = 0;	/* default values */
    resp->tr_pktcnt  = 0;	/* default values */
    resp->tr_rproto  = PROTO_DVMRP;
    resp->tr_smask   = 0;
    resp->tr_outaddr = uvifs[vifi].uv_lcl_addr;
    resp->tr_fttl    = uvifs[vifi].uv_threshold;
    resp->tr_rflags  = TR_NO_ERR;

    /*
     * obtain # of packets out on interface
     */
    v_req.vifi = vifi;
    if (ioctl(udp_socket, SIOCGETVIFCNT, (char *)&v_req) >= 0)
	resp->tr_vifout  =  v_req.ocount;

    /*
     * fill in scoping & pruning information
     */
    kt = find_src_grp(qry->tr_src, group);

    if (kt != NULL) {
	sg_req.src.s_addr = qry->tr_src;
	sg_req.grp.s_addr = group;
	if (ioctl(udp_socket, SIOCGETSGCNT, (char *)&sg_req) >= 0)
	    resp->tr_pktcnt = sg_req.count;

	if (VIFM_ISSET(vifi, kt->kt_scope))
	    resp->tr_rflags = TR_SCOPED;
	else if (kt->kt_prsent_timer)
	    resp->tr_rflags = TR_PRUNED;
    }

    /*
     *  if no rte exists, set NO_RTE error
     */
    if (rt == NULL) {
	src = dst;		/* the dst address of resp. pkt */
	resp->tr_inaddr   = NULL;
	resp->tr_rflags   = TR_NO_RTE;
	resp->tr_rmtaddr = NULL;
    }
    else {
	/* get # of packets in on interface */
	v_req.vifi = rt->rt_parent;
	if (ioctl(udp_socket, SIOCGETVIFCNT, (char *)&v_req) >= 0)
	    resp->tr_vifin = v_req.icount;

	MASK_TO_VAL(rt->rt_originmask, resp->tr_smask);
	src = uvifs[rt->rt_parent].uv_lcl_addr;
	resp->tr_inaddr = src;
	resp->tr_rmtaddr = rt->rt_gateway;
	if (!VIFM_ISSET(vifi, rt->rt_children)) {
	    printf("Destination %s not on forwarding tree for src %s\n",
		   inet_fmt(qry->tr_dst, s1), inet_fmt(qry->tr_src, s2));
	    resp->tr_rflags = TR_WRONG_IF;
	}
    }

    /*
     * if metric is 1 or no. of reports is 1, send response to requestor
     * else send to upstream router.
     */
    printf("rcount:%d, no:%d\n", rcount, no);

    if ((rcount + 1 == no) || (rt->rt_metric == 1))
	dst = qry->tr_raddr;
    else
	dst = rt->rt_gateway;

    if (IN_MULTICAST(ntohl(dst))) {
	k_set_ttl(qry->tr_rttl);
	send_igmp(src, dst,
		  IGMP_MTRACE_RESP, no, group,
		  datalen + RLEN);
	k_set_ttl(1);
    }
    else
	send_igmp(src, dst,
		  IGMP_MTRACE, no, group,
		  datalen + RLEN);
    return;
}

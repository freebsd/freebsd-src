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
 *  $Id: mrt.h,v 1.2 1999/08/24 10:04:56 jinmei Exp $
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
 * $FreeBSD: src/usr.sbin/pim6dd/mrt.h,v 1.1.2.1 2000/07/15 07:36:29 kris Exp $
 */

#define MRTF_SPT                0x0001	/* iif toward source                */
#define MRTF_WC                 0x0002	/* (*,G) entry                      */
#define MRTF_RP                 0x0004	/* iif toward RP                    */
#define MRTF_NEW                0x0008	/* new created routing entry        */
#define MRTF_IIF_REGISTER	0x0020  /* ???                              */
#define MRTF_REGISTER		0x0080  /* ???                              */
#define MRTF_KERNEL_CACHE 	0x0200	/* a mirror for the kernel cache    */
#define MRTF_NULL_OIF 		0x0400	/* null oif cache..     ???         */
#define MRTF_REG_SUPP 		0x0800	/* register suppress    ???         */
#define MRTF_ASSERTED		0x1000	/* upstream is not that of src ???  */
#define MRTF_SG			0x2000	/* (S,G) pure, not hanging off of (*,G)*/
#define MRTF_PMBR               0x4000  /* (*,*,RP) entry (for interop)     */

/* Macro to duplicate oif info (oif bits, timers): XXX: unused */
#define VOIF_COPY(from, to)                                                \
	    do {                                                           \
                VIFM_COPY((from)->joined_oifs, (to)->joined_oifs);         \
                VIFM_COPY((from)->oifs, (to)->oifs);                       \
                VIFM_COPY((from)->leaves, (to)->leaves);                   \
                VIFM_COPY((from)->pruned_oifs, (to)->pruned_oifs);         \
                bcopy((from)->prune_timers, (to)->prune_timers,            \
		      numvifs*sizeof((from)->prune_timers[0]));            \
                bcopy((from)->prune_delay_timerids,                        \
		      (to)->prune_delay_timerids,                          \
		      numvifs*sizeof((from)->prune_delay_timerids[0]));    \
                (to)->join_delay_timerid = (from)->join_delay_timerid;     \
	    } while (0)

#ifdef SAVE_MEMORY
#define FREE_MRTENTRY(mrtentry_ptr)                                        \
             do {                                                          \
                  u_int16 i;                                               \
                  u_long *il_ptr;                                          \
		  free((char *)((mrtentry_ptr)->prune_timers));            \
		  for(i=0, il_ptr=(mrtentry_ptr)->prune_delay_timerids;    \
                      i<numvifs; i++, il_ptr++)                            \
		       timer_clearTimer(*il_ptr);                          \
		  free((char *)((mrtentry_ptr)->prune_delay_timerids));    \
                  timer_clearTimer((mrtentry_ptr)->join_delay_timerid);    \
		  delete_pim6_graft_entry((mrtentry_ptr));                  \
                  free((char *)(mrtentry_ptr));                            \
	     } while (0)
#else
#define FREE_MRTENTRY(mrtentry_ptr)                                        \
             do {                                                          \
                  u_int16 i;                                               \
                  u_long *il_ptr;                                          \
		  free((char *)((mrtentry_ptr)->prune_timers));            \
		  for(i=0, il_ptr=(mrtentry_ptr)->prune_delay_timerids;    \
                      i<total_interfaces; i++, il_ptr++)                   \
		       timer_clearTimer(*il_ptr);                          \
		  free((char *)((mrtentry_ptr)->prune_delay_timerids));    \
                  free((char *)((mrtentry_ptr)->last_assert));             \
                  free((char *)((mrtentry_ptr)->last_prune));              \
                  timer_clearTimer((mrtentry_ptr)->join_delay_timerid);    \
		  delete_pim6_graft_entry((mrtentry_ptr));                  \
                  free((char *)(mrtentry_ptr));                            \
	     } while (0)
#endif  /* SAVE_MEMORY */

typedef struct pim_nbr_entry {
    struct	pim_nbr_entry *next;	  /* link to next neighbor	    */
    struct	pim_nbr_entry *prev;	  /* link to prev neighbor	    */
    struct sockaddr_in6	address;	  /* neighbor address		    */
    vifi_t	vifi;			  /* which interface		    */
    u_int16	timer;			  /* for timing out neighbor	    */
} pim_nbr_entry_t;


/*
 * Used to get forwarded data related counts (number of packet, number of
 * bits, etc)
 */
struct sg_count {
    u_long pktcnt;     /*  Number of packets for (s,g) */
    u_long bytecnt;    /*  Number of bytes for (s,g)   */
    u_long wrong_if;   /*  Number of packets received on wrong iif for (s,g) */
};

typedef struct mrtentry mrtentry_t;
typedef struct pim_graft_entry {
    struct pim_graft_entry    *next;
    struct pim_graft_entry    *prev;
    mrtentry_t                *mrtlink;
} pim_graft_entry_t;


typedef struct srcentry {
    struct srcentry	*next;		/* link to next entry		    */
    struct srcentry	*prev;		/* link to prev entry		    */
    struct sockaddr_in6	address;	/* source or RP address 	    */
    struct mrtentry	*mrtlink;	/* link to routing entries	    */
    vifi_t		incoming;	/* incoming vif			    */
    struct pim_nbr_entry *upstream;	/* upstream router		    */
    u_int32             metric;     /* Unicast Routing Metric to the source */
    u_int32		preference;	/* The metric preference (for assers)*/
    u_int16		timer;		/* Entry timer??? Delete?      	    */
} srcentry_t;


typedef struct grpentry {
    struct grpentry	*next;	       /* link to next entry		    */
    struct grpentry	*prev;	       /* link to prev entry		    */
    struct sockaddr_in6	group;	       /* subnet group of multicasts	    */
    struct mrtentry	*mrtlink;      /* link to (S,G) routing entries	    */
} grpentry_t;

struct mrtentry {
    struct mrtentry	*grpnext;	/* next entry of same group	    */
    struct mrtentry	*grpprev;	/* prev entry of same group	    */
    struct mrtentry	*srcnext;	/* next entry of same source	    */
    struct mrtentry	*srcprev;	/* prev entry of same source	    */
    struct grpentry	*group;		/* pointer to group entry	    */
    struct srcentry	*source;	/* pointer to source entry (or RP)  */
    vifi_t              incoming;       /* the iif (either toward S or RP)  */

    if_set		oifs;           /* The current result oifs          */
    if_set		pruned_oifs;    /* The pruned oifs (Prune received) */
    if_set		asserted_oifs;	/* The asserted oifs (Lost Assert)  */
    if_set		filter_oifs;	/* The filtered oifs */
    if_set		leaves;		/* Has directly connected members   */
    struct pim_nbr_entry *upstream;	/* upstream router, needed because
					 * of the asserts it may be different
					 * than the source (or RP) upstream
					 * router.
					 */
    u_int32             metric;         /* Metric for the upstream          */
    u_int32		preference;	/* preference for the upstream      */
    u_int16	        *prune_timers;  /* prune timer list		    */
    u_long              *prune_delay_timerids; /* timers for LAN prunes     */
    u_long              join_delay_timerid; /* timer for delay joins        */
    u_int16	        flags;	        /* The MRTF_* flags                 */
    u_int16	        timer;	        /* entry timer			    */
    u_int	        assert_timer;
    struct sg_count     sg_count;
    u_long              *last_assert;   /* time for last data-driven assert */
    u_long              *last_prune;    /* time for last data-driven prune  */
    pim_graft_entry_t   *graft;         /* Pointer into graft entry list    */
#ifdef RSRR
    struct rsrr_cache   *rsrr_cache;    /* Used to save RSRR requests for
                                         * routes change notification.
                                         */
#endif /* RSRR */
};


struct vif_count {
    u_long icount;        /* Input packet count on vif            */
    u_long ocount;        /* Output packet count on vif           */
    u_long ibytes;        /* Input byte count on vif              */ 
    u_long obytes;        /* Output byte count on vif             */ 
};

#define FILTER_RANGE 0
#define FILTER_PREFIX 1
struct mrtfilter {
	struct mrtfilter *next;	/* link to the next entry */
	int type;		/* filter type: RANGE or PREFIX */
	union {			/* type specific data structure */
		struct {
			struct sockaddr_in6 from;
			struct sockaddr_in6 to;
		} mrtfu_range;
		struct {
			struct sockaddr_in6 prefix;
			struct in6_addr mask;
		} mrtfu_prefix;
	} mrtu;
	if_set ifset;		/* interface list */
};
#define mrtf_from mrtu.mrtfu_range.from
#define mrtf_to mrtu.mrtfu_range.to
#define mrtf_prefix mrtu.mrtfu_prefix.prefix
#define mrtf_mask mrtu.mrtfu_prefix.mask

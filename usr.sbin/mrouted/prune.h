/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: prune.h,v 1.3 1994/08/24 23:54:40 thyagara Exp $
 */

/* 
 * Macro for copying the user-level cache table to the kernel
 * level table variable passed on by the setsock option 
 */

#define COPY_TABLES(from, to) { \
	register u_int _i; \
	(to).mfcc_origin.s_addr     = (from)->kt_origin; \
	(to).mfcc_mcastgrp.s_addr   = (from)->kt_mcastgrp; \
	(to).mfcc_originmask.s_addr = (from)->kt_originmask; \
	(to).mfcc_parent            = (from)->kt_parent; \
	for (_i = 0; _i < numvifs; _i++) \
	    (to).mfcc_ttls[_i]	  = (from)->kt_ttls[_i]; \
};


/*
 * User level Kernel Cache Table structure
 *
 * A copy of the kernel table is kept at the user level. Modifications are
 * made to this table and then passed on to the kernel. A timeout value is
 * an extra field in the user level table.
 *
 */
struct ktable 
{
    struct ktable  *kt_next;       	/* pointer to the next entry        */
    u_long	    kt_origin;		/* subnet origin of multicasts      */
    u_long	    kt_mcastgrp;    	/* multicast group associated       */
    u_long	    kt_originmask;	/* subnet mask for origin           */
    vifi_t	    kt_parent; 	    	/* incoming vif                     */
    u_long	    kt_gateway;		/* upstream router                  */
    vifbitmap_t	    kt_children;	/* outgoing children vifs           */
    vifbitmap_t	    kt_leaves;		/* subset of outgoing children vifs */
    vifbitmap_t     kt_scope;		/* scoped interfaces                */
    u_char	    kt_ttls[MAXVIFS];	/* ttl vector for forwarding        */
    vifbitmap_t	    kt_grpmems;		/* forw. vifs for src, grp          */
    int		    kt_timer;		/* for timing out entry in cache    */
    struct prunlst *kt_rlist;		/* router list nghboring this rter  */
    u_short	    kt_prun_count;	/* count of total no. of prunes     */
    int		    kt_prsent_timer;	/* prune lifetime timer             */
    u_int	    kt_grftsnt;		/* graft sent upstream              */
};

/*
 * structure to store incoming prunes
 */
struct prunlst 
{
    struct prunlst *rl_next;
    u_long	    rl_router;
    u_long	    rl_router_subnet;
    vifi_t	    rl_vifi;
    int		    rl_timer;
};

struct tr_query {
    u_long  tr_src;		/* traceroute source */
    u_long  tr_dst;		/* traceroute destination */
    u_long  tr_raddr;		/* traceroute response address */
    struct {
	u_int   ttl : 8;	/* traceroute response ttl */
	u_int   qid : 24;	/* traceroute query id */
    } q;
} tr_query;

#define tr_rttl q.ttl
#define tr_qid  q.qid

struct tr_resp {
    u_long  tr_qarr;		/* query arrival time */
    u_long  tr_inaddr;		/* incoming interface address */
    u_long  tr_outaddr;		/* outgoing interface address */
    u_long  tr_rmtaddr;		/* parent address in source tree */
    u_long  tr_vifin;		/* input packet count on interface */
    u_long  tr_vifout;		/* output packet count on interface */
    u_long  tr_pktcnt;		/* total incoming packets for src-grp */
    u_char  tr_rproto;		/* routing protocol deployed on router */
    u_char  tr_fttl;		/* ttl required to forward on outvif */
    u_char  tr_smask;		/* subnet mask for src addr */
    u_char  tr_rflags;		/* forwarding error codes */
} tr_resp;

/* defs within mtrace */
#define QUERY	1
#define RESP	2
#define QLEN	sizeof(struct tr_query)
#define RLEN	sizeof(struct tr_resp)

/* fields for tr_rflags (forwarding error codes) */
#define TR_NO_ERR	0x0
#define TR_WRONG_IF	0x1
#define TR_PRUNED	0x2
#define TR_SCOPED	0x4
#define TR_NO_RTE	0x5

/* fields for tr_rproto (routing protocol) */
#define PROTO_DVMRP	0x1
#define PROTO_MOSPF	0x2
#define PROTO_PIM	0x3
#define PROTO_CBT	0x4

#define MASK_TO_VAL(x, i) { \
			(i) = 0; \
			while ((x) << (i)) \
				(i)++; \
			}

#define VAL_TO_MASK(x, i) { \
			x = ~((1 << (32 - (i))) - 1); \
			}				

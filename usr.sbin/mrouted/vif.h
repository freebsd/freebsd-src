/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $FreeBSD: src/usr.sbin/mrouted/vif.h,v 1.10 1999/08/28 01:17:09 peter Exp $
 * vif.h,v 3.8.4.26 1998/01/14 21:21:19 fenner Exp
 */


/*
 * Bitmap handling functions.
 * These should be fast but generic.  bytes can be slow to zero and compare,
 * words are hard to make generic.  Thus two sets of macros (yuk).
 */

/*
 * The VIFM_ functions should migrate out of <netinet/ip_mroute.h>, since
 * the kernel no longer uses vifbitmaps.
 */
#ifndef VIFM_SET
typedef	u_long vifbitmap_t;

#define	VIFM_SET(n, m)			((m) |=  (1 << (n)))
#define	VIFM_CLR(n, m)			((m) &= ~(1 << (n)))
#define	VIFM_ISSET(n, m)		((m) &   (1 << (n)))
#define VIFM_CLRALL(m)			((m) = 0x00000000)
#define VIFM_COPY(mfrom, mto)		((mto) = (mfrom))
#define VIFM_SAME(m1, m2)		((m1) == (m2))
#endif
/*
 * And <netinet/ip_mroute.h> was missing some required functions anyway
 */
#ifndef VIFM_SETALL
#define	VIFM_SETALL(m)			((m) = ~0)
#endif
#define	VIFM_ISSET_ONLY(n, m)		((m) == (1 << (n)))
#define	VIFM_ISEMPTY(m)			((m) == 0)
#define	VIFM_CLR_MASK(m, mask)		((m) &= ~(mask))
#define	VIFM_SET_MASK(m, mask)		((m) |= (mask))

/*
 * Neighbor bitmaps are, for efficiency, implemented as a struct
 * containing two variables of a native machine type.  If you
 * have a native type that's bigger than a long, define it below.
 */
#define	NBRTYPE		u_long
#define NBRBITS		sizeof(NBRTYPE) * 8

typedef struct {
    NBRTYPE hi;
    NBRTYPE lo;
} nbrbitmap_t;
#define	MAXNBRS		2 * NBRBITS
#define	NO_NBR		MAXNBRS

#define	NBRM_SET(n, m)		(((n) < NBRBITS) ? ((m).lo |= (1 << (n))) : \
				      ((m).hi |= (1 << (n - NBRBITS))))
#define	NBRM_CLR(n, m)		(((n) < NBRBITS) ? ((m).lo &= ~(1 << (n))) : \
				      ((m).hi &= ~(1 << (n - NBRBITS))))
#define	NBRM_ISSET(n, m)	(((n) < NBRBITS) ? ((m).lo & (1 << (n))) : \
				      ((m).hi & (1 << ((n) - NBRBITS))))
#define	NBRM_CLRALL(m)		((m).lo = (m).hi = 0)
#define	NBRM_COPY(mfrom, mto)	((mto).lo = (mfrom).lo, (mto).hi = (mfrom).hi)
#define	NBRM_SAME(m1, m2)	(((m1).lo == (m2).lo) && ((m1).hi == (m2).hi))
#define	NBRM_ISEMPTY(m)		(((m).lo == 0) && ((m).hi == 0))
#define	NBRM_SETMASK(m, mask)	(((m).lo |= (mask).lo),((m).hi |= (mask).hi))
#define	NBRM_CLRMASK(m, mask)	(((m).lo &= ~(mask).lo),((m).hi &= ~(mask).hi))
#define	NBRM_MASK(m, mask)	(((m).lo &= (mask).lo),((m).hi &= (mask).hi))
#define	NBRM_ISSETMASK(m, mask)	(((m).lo & (mask).lo) || ((m).hi & (mask).hi))
#define	NBRM_ISSETALLMASK(m, mask)\
				((((m).lo & (mask).lo) == (mask).lo) && \
				 (((m).hi & (mask).hi) == (mask).hi))
/*
 * This macro is TRUE if all the subordinates have been pruned, or if
 * there are no subordinates on this vif.
 * The arguments is the map of subordinates, the map of neighbors on the
 * vif, and the map of received prunes.
 */
#define	SUBS_ARE_PRUNED(sub, vifmask, prunes)	\
    (((sub).lo & (vifmask).lo) == ((prunes).lo & (vifmask).lo & (sub).lo) && \
     ((sub).hi & (vifmask).hi) == ((prunes).hi & (vifmask).hi & (sub).hi))

struct blastinfo {
    char *	     bi_buf;	    /* Pointer to malloced storage	    */
    char *	     bi_cur;	    /* The update to process next	    */
    char *	     bi_end;	    /* The place to put the next update	    */
    int		     bi_len;	    /* Size of malloced storage		    */
    int		     bi_timer;	    /* Timer to run process_blaster_report  */
};

/*
 * User level Virtual Interface structure
 *
 * A "virtual interface" is either a physical, multicast-capable interface
 * (called a "phyint") or a virtual point-to-point link (called a "tunnel").
 * (Note: all addresses, subnet numbers and masks are kept in NETWORK order.)
 */
struct uvif {
    u_int	     uv_flags;	    /* VIFF_ flags defined below            */
    u_char	     uv_metric;     /* cost of this vif                     */
    u_char	     uv_admetric;   /* advertised cost of this vif          */
    u_char	     uv_threshold;  /* min ttl required to forward on vif   */
    u_int	     uv_rate_limit; /* rate limit on this vif               */
    u_int32	     uv_lcl_addr;   /* local address of this vif            */
    u_int32	     uv_rmt_addr;   /* remote end-point addr (tunnels only) */
    u_int32	     uv_dst_addr;   /* destination for DVMRP messages       */
    u_int32	     uv_subnet;     /* subnet number         (phyints only) */
    u_int32	     uv_subnetmask; /* subnet mask           (phyints only) */
    u_int32	     uv_subnetbcast;/* subnet broadcast addr (phyints only) */
    char	     uv_name[IFNAMSIZ]; /* interface name                   */
    struct listaddr *uv_groups;     /* list of local groups  (phyints only) */
    struct listaddr *uv_neighbors;  /* list of neighboring routers          */
    nbrbitmap_t	     uv_nbrmap;	    /* bitmap of active neighboring routers */
    struct listaddr *uv_querier;    /* IGMP querier on vif                  */
    int		     uv_igmpv1_warn;/* To rate-limit IGMPv1 warnings	    */
    int		     uv_prune_lifetime; /* Prune lifetime or 0 for default  */
    struct vif_acl  *uv_acl;	    /* access control list of groups        */
    int		     uv_leaf_timer; /* time until this vif is considrd leaf */
    struct phaddr   *uv_addrs;	    /* Additional subnets on this vif       */
    struct vif_filter *uv_filter;   /* Route filters on this vif	    */
    struct blastinfo uv_blaster;    /* Info about route blasters	    */
    int		     uv_nbrup;	    /* Counter for neighbor up events       */
    int		     uv_icmp_warn;  /* To rate-limit ICMP warnings	    */
    u_int	     uv_nroutes;    /* # of routes with this vif as parent  */
    struct ip 	    *uv_encap_hdr;  /* Pre-formed header to encapsulate msgs*/
};

#define uv_blasterbuf	uv_blaster.bi_buf
#define	uv_blastercur	uv_blaster.bi_cur
#define	uv_blasterend	uv_blaster.bi_end
#define uv_blasterlen	uv_blaster.bi_len
#define uv_blastertimer	uv_blaster.bi_timer

#define VIFF_KERNEL_FLAGS	(VIFF_TUNNEL|VIFF_SRCRT)
#define VIFF_DOWN		0x000100	/* kernel state of interface */
#define VIFF_DISABLED		0x000200	/* administratively disabled */
#define VIFF_QUERIER		0x000400	/* I am the subnet's querier */
#define VIFF_ONEWAY		0x000800	/* Maybe one way interface   */
#define VIFF_LEAF		0x001000	/* all neighbors are leaves  */
#define VIFF_IGMPV1		0x002000	/* Act as an IGMPv1 Router   */
#define	VIFF_REXMIT_PRUNES	0x004000	/* retransmit prunes         */
#define VIFF_PASSIVE		0x008000	/* passive tunnel	     */
#define	VIFF_ALLOW_NONPRUNERS	0x010000	/* ok to peer with nonprunrs */
#define VIFF_NOFLOOD		0x020000	/* don't flood on this vif   */
#define	VIFF_NOTRANSIT		0x040000	/* don't transit these vifs  */
#define	VIFF_BLASTER		0x080000	/* nbr on vif blasts routes  */
#define	VIFF_FORCE_LEAF		0x100000	/* ignore nbrs on this vif   */
#define	VIFF_OTUNNEL		0x200000	/* DVMRP msgs "beside" tunnel*/

#define	AVOID_TRANSIT(v, r)	\
		(((r)->rt_parent != NO_VIF) && \
		 ((r)->rt_gateway != 0) && \
		 (uvifs[(v)].uv_flags & VIFF_NOTRANSIT) && \
		 (uvifs[(r)->rt_parent].uv_flags & VIFF_NOTRANSIT))

struct phaddr {
    struct phaddr   *pa_next;
    u_int32	     pa_subnet;		/* extra subnet			*/
    u_int32	     pa_subnetmask;	/* netmask of extra subnet	*/
    u_int32	     pa_subnetbcast;	/* broadcast of extra subnet	*/
};

struct vif_acl {
    struct vif_acl  *acl_next;	    /* next acl member         */
    u_int32	     acl_addr;	    /* Group address           */
    u_int32	     acl_mask;	    /* Group addr. mask        */
};

struct vif_filter {
    int			vf_type;
#define	VFT_ACCEPT	1
#define	VFT_DENY	2
    int			vf_flags;
#define	VFF_BIDIR	1
    struct vf_element  *vf_filter;
};

struct vf_element {
    struct vf_element  *vfe_next;
    u_int32		vfe_addr;
    u_int32		vfe_mask;
    int			vfe_flags;
#define	VFEF_EXACT	0x0001
};

struct listaddr {
    struct listaddr *al_next;		/* link to next addr, MUST BE FIRST */
    u_int32	     al_addr;		/* local group or neighbor address  */
    u_long	     al_timer;		/* for timing out group or neighbor */
    time_t	     al_ctime;		/* entry creation time		    */
    union {
	struct {
    	    u_int32  alur_genid;	/* generation id for neighbor       */
	    u_int    alur_nroutes;	/* # of routes w/ nbr as parent	    */
    	    u_char   alur_pv;		/* router protocol version	    */
    	    u_char   alur_mv;		/* router mrouted version	    */
    	    u_char   alur_index;	/* neighbor index		    */
	} alu_router;
	struct {
    	    u_int32  alug_reporter;	/* a host which reported membership */
    	    u_long   alug_timerid;	/* timer for group membership	    */
    	    u_long   alug_query;	/* timer for repeated leave query   */
    	    u_char   alug_old;		/* time since heard old report      */
	} alu_group;
    } al_alu;
    u_short	     al_flags;		/* flags related to this neighbor   */
};
#define	al_genid	al_alu.alu_router.alur_genid
#define	al_nroutes	al_alu.alu_router.alur_nroutes
#define al_pv		al_alu.alu_router.alur_pv
#define al_mv		al_alu.alu_router.alur_mv
#define	al_index	al_alu.alu_router.alur_index
#define	al_reporter	al_alu.alu_group.alug_reporter
#define	al_old		al_alu.alu_group.alug_old
#define	al_timerid	al_alu.alu_group.alug_timerid
#define	al_query	al_alu.alu_group.alug_query

#define	NBRF_LEAF		0x0001	/* This neighbor is a leaf 	    */
#define	NBRF_GENID		0x0100	/* I know this neighbor's genid	    */
#define	NBRF_WAITING		0x0200	/* Waiting for peering to come up   */
#define	NBRF_ONEWAY		0x0400	/* One-way peering 		    */
#define	NBRF_TOOOLD		0x0800	/* Too old (policy decision) 	    */
#define	NBRF_TOOMANYROUTES	0x1000	/* Neighbor is spouting routes 	    */
#define	NBRF_NOTPRUNING		0x2000	/* Neighbor doesn't appear to prune */

/*
 * Don't peer with neighbors with any of these flags set
 */
#define	NBRF_DONTPEER		(NBRF_WAITING|NBRF_ONEWAY|NBRF_TOOOLD| \
				 NBRF_TOOMANYROUTES|NBRF_NOTPRUNING)

#define NO_VIF		((vifi_t)MAXVIFS)  /* An invalid vif index */

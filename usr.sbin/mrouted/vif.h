/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: vif.h,v 1.6 1994/08/24 23:54:47 thyagara Exp $
 */

/*
 * User level Virtual Interface structure
 *
 * A "virtual interface" is either a physical, multicast-capable interface
 * (called a "phyint") or a virtual point-to-point link (called a "tunnel").
 * (Note: all addresses, subnet numbers and masks are kept in NETWORK order.)
 */
struct uvif {
    u_short	     uv_flags;	    /* VIFF_ flags defined below            */
    u_char	     uv_metric;     /* cost of this vif                     */
    u_int	     uv_rate_limit; /* rate limit on this vif               */
    u_char	     uv_threshold;  /* min ttl required to forward on vif   */
    u_long	     uv_lcl_addr;   /* local address of this vif            */
    u_long	     uv_rmt_addr;   /* remote end-point addr (tunnels only) */
    u_long	     uv_subnet;     /* subnet number         (phyints only) */
    u_long	     uv_subnetmask; /* subnet mask           (phyints only) */
    u_long	     uv_subnetbcast;/* subnet broadcast addr (phyints only) */
    char	     uv_name[IFNAMSIZ]; /* interface name                   */
    struct listaddr *uv_groups;     /* list of local groups  (phyints only) */
    struct listaddr *uv_neighbors;  /* list of neighboring routers          */
    struct vif_acl  *uv_acl;	    /* access control list of groups        */
};

#define VIFF_KERNEL_FLAGS	(VIFF_TUNNEL|VIFF_SRCRT)
#define VIFF_DOWN		0x0100	       /* kernel state of interface */
#define VIFF_DISABLED		0x0200	       /* administratively disabled */
#define VIFF_QUERIER		0x0400	       /* I am the subnet's querier */
#define VIFF_ONEWAY		0x0800         /* Maybe one way interface   */

struct vif_acl {
    struct vif_acl  *acl_next;	    /* next acl member         */
    u_long	     acl_addr;	    /* Group address           */
    u_long	     acl_mask;	    /* Group addr. mask        */
};

struct listaddr {
    struct listaddr *al_next;		/* link to next addr, MUST BE FIRST */
    u_long	     al_addr;		/* local group or neighbor address  */
    u_long	     al_timer;		/* for timing out group or neighbor */
    u_long	     al_genid;		/* generation id for neighbor       */
    u_char	     al_pv;		/* router protocol version	    */
    u_char	     al_mv;		/* router mrouted version	    */
    u_long           al_timerid;        /* returned by set timer */
    u_long	     al_query;		/* second query in case of leave*/
    u_short          al_old;            /* if old memberships are present */
    u_short          al_last;		/* # of query's since last old rep */
};


#define NO_VIF		((vifi_t)MAXVIFS)  /* An invalid vif index */

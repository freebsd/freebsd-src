/*
 * Copyright (c) 1989 Stephen Deering.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ip_mroute.h	8.1 (Berkeley) 6/10/93
 * $Id: ip_mroute.h,v 1.5 1994/09/14 03:10:12 wollman Exp $
 */

#ifndef _NETINET_IP_MROUTE_H_
#define _NETINET_IP_MROUTE_H_

/*
 * Definitions for the kernel part of DVMRP,
 * a Distance-Vector Multicast Routing Protocol.
 * (See RFC-1075.)
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Ajit Thyagarajan, PARC, August 1993.
 * Modified by Ajit Thyagarajan, PARC, August 1994.
 *
 * MROUTING 1.5
 */


/*
 * DVMRP-specific setsockopt commands.
 */
#define	DVMRP_INIT	100	/* initialize forwarder */
#define	DVMRP_DONE	101	/* shut down forwarder */
#define	DVMRP_ADD_VIF	102	/* create virtual interface */
#define	DVMRP_DEL_VIF	103	/* delete virtual interface */
#define DVMRP_ADD_MFC	104	/* insert forwarding cache entry */
#define DVMRP_DEL_MFC	105	/* delete forwarding cache entry */

#define GET_TIME(t)	microtime(&t)

/*
 * Types and macros for handling bitmaps with one bit per virtual interface.
 */
#define	MAXVIFS 32
typedef u_long vifbitmap_t;
typedef u_short vifi_t;		/* type of a vif index */

#define	VIFM_SET(n, m)		((m) |= (1 << (n)))
#define	VIFM_CLR(n, m)		((m) &= ~(1 << (n)))
#define	VIFM_ISSET(n, m)	((m) & (1 << (n)))
#define	VIFM_CLRALL(m)		((m) = 0x00000000)
#define	VIFM_COPY(mfrom, mto)	((mto) = (mfrom))
#define	VIFM_SAME(m1, m2)	((m1) == (m2))


/*
 * Argument structure for DVMRP_ADD_VIF.
 * (DVMRP_DEL_VIF takes a single vifi_t argument.)
 */
struct vifctl {
	vifi_t	    vifc_vifi;	    	/* the index of the vif to be added */
	u_char	    vifc_flags;     	/* VIFF_ flags defined below */
	u_char	    vifc_threshold; 	/* min ttl required to forward on vif */
	u_int	vifc_rate_limit; /* max tate */
	struct	in_addr vifc_lcl_addr;	/* local interface address */
	struct	in_addr vifc_rmt_addr;	/* remote address (tunnels only) */
};

#define	VIFF_TUNNEL	0x1		/* vif represents a tunnel end-point */
#define VIFF_SRCRT	0x2	/* tunnel uses IP source routing */

/*
 * Argument structure for DVMRP_ADD_MFC
 * (mfcc_tos to be added at a future point)
 */
struct mfcctl {
    struct in_addr  mfcc_origin;		/* subnet origin of mcasts   */
    struct in_addr  mfcc_mcastgrp; 		/* multicast group associated*/
    struct in_addr  mfcc_originmask;		/* subnet mask for origin    */
    vifi_t	    mfcc_parent;   		/* incoming vif              */
    u_char	    mfcc_ttls[MAXVIFS]; 	/* forwarding ttls on vifs   */
};

/*
 * Argument structure for DVMRP_DEL_MFC
 */
struct delmfcctl {
    struct in_addr  mfcc_origin;    /* subnet origin of multicasts      */
    struct in_addr  mfcc_mcastgrp;  /* multicast group assoc. w/ origin */
};

/*
 * Argument structure used by RSVP daemon to get vif information
 */
struct vif_req {
    u_char         v_flags;         /* VIFF_ flags defined above           */
    u_char         v_threshold;     /* min ttl required to forward on vif  */
    struct in_addr v_lcl_addr;      /* local interface address             */
    struct in_addr v_rmt_addr; 
    char           v_if_name[IFNAMSIZ];  /* if name */
};

struct vif_conf {
    u_int          vifc_len;
    u_int          vifc_num;
    struct vif_req *vifc_req;
};

/*
 * The kernel's multicast routing statistics.
 */
struct mrtstat {
    u_long	mrts_mfc_lookups;	/* # forw. cache hash table hits   */
    u_long	mrts_mfc_misses;	/* # forw. cache hash table misses */
    u_long	mrts_upcalls;		/* # calls to mrouted              */
    u_long	mrts_no_route;		/* no route for packet's origin    */
    u_long	mrts_bad_tunnel;	/* malformed tunnel options        */
    u_long	mrts_cant_tunnel;	/* no room for tunnel options      */
    u_long	mrts_wrong_if;		/* arrived on wrong interface	   */
    u_long	mrts_upq_ovflw;		/* upcall Q overflow		   */
    u_long	mrts_cache_cleanups;	/* # entries with no upcalls 	   */
    u_long  	mrts_drop_sel;     	/* pkts dropped selectively        */
    u_long  	mrts_q_overflow;    	/* pkts dropped - Q overflow       */
    u_long  	mrts_pkt2large;     	/* pkts dropped - size > BKT SIZE  */
};

/*
 * Argument structure used by mrouted to get src-grp pkt counts
 */
struct sioc_sg_req {
    struct in_addr src;
    struct in_addr grp;
    u_long count;
};

/*
 * Argument structure used by mrouted to get vif pkt counts
 */
struct sioc_vif_req {
    vifi_t vifi;
    u_long icount;
    u_long ocount;
};
    

#ifdef KERNEL

struct vif {
    u_char   		v_flags;     	/* VIFF_ flags defined above         */
    u_char   		v_threshold;	/* min ttl required to forward on vif*/
    u_int      		v_rate_limit; 	/* max rate			     */
    struct tbf 	       *v_tbf;       	/* token bucket structure at intf.   */
    struct in_addr 	v_lcl_addr;   	/* local interface address           */
    struct in_addr 	v_rmt_addr;   	/* remote address (tunnels only)     */
    struct ifnet       *v_ifp;	     	/* pointer to interface              */
    u_long		v_pkt_in;	/* # pkts in on interface            */
    u_long		v_pkt_out;	/* # pkts out on interface           */
};

/*
 * The kernel's multicast forwarding cache entry structure 
 * (A field for the type of service (mfc_tos) is to be added 
 * at a future point)
 */
struct mfc {
    struct in_addr  mfc_origin;	 		/* subnet origin of mcasts   */
    struct in_addr  mfc_mcastgrp;  		/* multicast group associated*/
    struct in_addr  mfc_originmask;		/* subnet mask for origin    */
    vifi_t	    mfc_parent; 		/* incoming vif              */
    u_char	    mfc_ttls[MAXVIFS]; 		/* forwarding ttls on vifs   */
    u_long	    mfc_pkt_cnt;		/* pkt count for src-grp     */
};

/*
 * Argument structure used for pkt info. while upcall is made
 */
struct rtdetq {
    struct mbuf 	*m;
    struct ifnet	*ifp;
    u_long		tunnel_src;
    struct ip_moptions *imo;
};

#define MFCTBLSIZ	256
#if (MFCTBLSIZ & (MFCTBLSIZ - 1)) == 0	  /* from sys:route.h */
#define MFCHASHMOD(h)	((h) & (MFCTBLSIZ - 1))
#else
#define MFCHASHMOD(h)	((h) % MFCTBLSIZ)
#endif

#define MAX_UPQ	4		/* max. no of pkts in upcall Q */

/*
 * Token Bucket filter code 
 */
#define MAX_BKT_SIZE    10000             /* 10K bytes size 		*/
#define MAXQSIZE        10                /* max # of pkts in queue 	*/

/*
 * queue structure at each vif
 */
struct pkt_queue 
{
    u_long pkt_len;               /* length of packet in queue 	*/
    struct mbuf *pkt_m;           /* pointer to packet mbuf	*/
    struct ip  *pkt_ip;           /* pointer to ip header	*/
    struct ip_moptions *pkt_imo; /* IP multicast options assoc. with pkt */
};

/*
 * the token bucket filter at each vif
 */
struct tbf
{
    u_long last_pkt_t;	/* arr. time of last pkt 	*/
    u_long n_tok;      	/* no of tokens in bucket 	*/
    u_long q_len;    	/* length of queue at this vif	*/
};

extern int	(*ip_mrouter_cmd) __P((int, struct socket *, struct mbuf *));
extern int	(*ip_mrouter_done) __P((void));
extern int	(*mrt_ioctl) __P((int, caddr_t, struct proc *));

#endif /* KERNEL */

#endif /* _NETINET_IP_MROUTE_H_ */

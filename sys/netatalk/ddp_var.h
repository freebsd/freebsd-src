/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 *
 * $FreeBSD: src/sys/netatalk/ddp_var.h,v 1.4 1999/12/29 04:45:58 peter Exp $
 */

#ifndef _NETATALK_DDP_VAR_H_
#define _NETATALK_DDP_VAR_H_ 1
struct ddpcb {
    struct sockaddr_at	ddp_fsat, ddp_lsat;
    struct route	ddp_route;
    struct socket	*ddp_socket;
    struct ddpcb	*ddp_prev, *ddp_next;
    struct ddpcb	*ddp_pprev, *ddp_pnext;
};

#define sotoddpcb(so)	((struct ddpcb *)(so)->so_pcb)

struct ddpstat {
    long	ddps_short;		/* short header packets received */
    long	ddps_long;		/* long header packets received */
    long	ddps_nosum;		/* no checksum */
    long	ddps_badsum;		/* bad checksum */
    long	ddps_tooshort;		/* packet too short */
    long	ddps_toosmall;		/* not enough data */
    long	ddps_forward;		/* packets forwarded */
    long	ddps_encap;		/* packets encapsulated */
    long	ddps_cantforward;	/* packets rcvd for unreachable dest */
    long	ddps_nosockspace;	/* no space in sockbuf for packet */
};

#ifdef _KERNEL
extern int	ddp_cksum;
extern struct ddpcb		*ddp_ports[ ];
extern struct ddpcb		*ddpcb;
extern struct pr_usrreqs	ddp_usrreqs;
#endif
#endif /* _NETATALK_DDP_VAR_H_ */

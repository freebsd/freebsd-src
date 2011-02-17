/*-
 * Copyright (c) 1990, 1994 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 *
 * $FreeBSD$
 */

#ifndef _NETATALK_DDP_VAR_H_
#define	_NETATALK_DDP_VAR_H_

struct ddpcb {
	struct sockaddr_at	 ddp_fsat, ddp_lsat;
	struct route		 ddp_route;
	struct socket		*ddp_socket;
	struct ddpcb		*ddp_prev, *ddp_next;
	struct ddpcb		*ddp_pprev, *ddp_pnext;
	struct mtx		 ddp_mtx;
};

#define	sotoddpcb(so)	((struct ddpcb *)(so)->so_pcb)

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
extern int			 ddp_cksum;
extern struct ddpcb		*ddpcb_list;
extern struct pr_usrreqs	 ddp_usrreqs;
extern struct mtx		 ddp_list_mtx;
#endif

#endif /* _NETATALK_DDP_VAR_H_ */

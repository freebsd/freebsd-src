/*	$NetBSD: if_tun.h,v 1.5 1994/06/29 06:36:27 cgd Exp $	*/

/*
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 *
 * This source may be freely distributed, however I would be interested
 * in any changes that are made.
 *
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have it's wicked way with. This driver has it's
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/select mode of
 * operation though.
 *
 * : $Header: if_tnreg.h,v 1.1.2.1 1992/07/16 22:39:16 friedl Exp
 */

#ifndef _NET_IF_TUN_H_
#define _NET_IF_TUN_H_

struct tun_softc {
	u_short	tun_flags;		/* misc flags */
#define	TUN_OPEN	0x0001
#define	TUN_INITED	0x0002
#define	TUN_RCOLL	0x0004
#define	TUN_IASET	0x0008
#define	TUN_DSTADDR	0x0010
#define	TUN_RWAIT	0x0040
#define	TUN_ASYNC	0x0080
#define	TUN_NBIO	0x0100

#define	TUN_READY	(TUN_OPEN | TUN_INITED | TUN_IASET)

	struct	ifnet tun_if;		/* the interface */
	int	tun_pgrp;		/* the process group - if any */
	struct	selinfo	tun_rsel;	/* read select */
	struct	selinfo	tun_wsel;	/* write select (not used) */
#if NBPFILTER > 0
	caddr_t		tun_bpf;
#endif
};

/* Maximum packet size */
#define	TUNMTU		1500

struct tuninfo {
	int	baudrate;		/* linespeed */
	short	mtu;			/* maximum transmission unit */
	u_char	type;			/* ethernet, tokenring, etc. */
	u_char	dummy;			/* place holder */
};

/* ioctl's for get/set debug */
#define	TUNSDEBUG	_IOW('t', 90, int)
#define	TUNGDEBUG	_IOR('t', 89, int)
#define	TUNSIFINFO	_IOW('t', 91, struct tuninfo)
#define	TUNGIFINFO	_IOR('t', 92, struct tuninfo)

#endif /* !_NET_IF_TUN_H_ */

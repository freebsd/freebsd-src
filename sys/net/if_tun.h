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
 * from: $Header: if_tnreg.h,v 1.1.2.1 1992/07/16 22:39:16 friedl Exp
 * $Id: if_tun.h,v 1.3 1993/12/13 14:27:01 deraadt Exp $
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
#ifdef notdef
#define	TUN_READY	0x0020
#else
#define TUN_READY       (TUN_IASET|TUN_OPEN|TUN_DSTADDR)
#endif
#define	TUN_RWAIT	0x0040
#define	TUN_ASYNC	0x0080
#define	TUN_NBIO	0x0100
	struct	ifnet tun_if;		/* the interface */
	int	tun_pgrp;		/* the process group - if any */
#if BSD < 199103
	struct  proc    *tun_rsel;
	struct  proc	*tun_wsel;
#else
	u_char		tun_pad;	/* explicit alignment */
	struct selinfo  tun_sel;	/* bsd select info */
#endif
#if NBPFILTER > 0
	caddr_t		tun_bpf;
#endif
};

/* Maximum packet size */
#define	TUNMTU		1500

/* ioctl's for get/set debug */
#ifdef __NetBSD__
#define	TUNSDEBUG	_IOW('t', 90, int)
#define	TUNGDEBUG	_IOR('t', 89, int)
#define	TUNSIFINFO	_IOW('t', 91, struct tuninfo)
#define	TUNGIFINFO	_IOR('t', 92, struct tuninfo)
#else	/* Assume BSDI */
#define	TUNSDEBUG	_IOW('T', 90, int)
#define	TUNGDEBUG	_IOR('T', 89, int)
#define	TUNSIFINFO	_IOW('T', 91, struct tuninfo)
#define	TUNGIFINFO	_IOR('T', 92, struct tuninfo)
#endif

struct tuninfo {
	int	tif_baudrate;		/* linespeed */
	short	tif_mtu;		/* maximum transmission unit */
	u_char	tif_type;		/* ethernet, tokenring, etc. */
	u_char	tif_dummy;		/* place holder */
};
#endif /* !_NET_IF_TUN_H_ */

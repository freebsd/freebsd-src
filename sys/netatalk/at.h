/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
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
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

#ifndef __AT_HEADER__
#define __AT_HEADER__
/*
 * Supported protocols
 */
#define ATPROTO_DDP	0
#define ATPROTO_AARP	254

/*
 * Ethernet types, for DIX.
 * These should really be in some global header file, but we can't
 * count on them being there, and it's annoying to patch system files.
 */
#define ETHERTYPE_AT	0x809B		/* AppleTalk protocol */
#define ETHERTYPE_AARP	0x80F3		/* AppleTalk ARP */

#define DDP_MAXSZ	587

/*
 * If ATPORT_FIRST <= Port < ATPORT_RESERVED,
 * Port was created by a privileged process.
 * If ATPORT_RESERVED <= Port < ATPORT_LAST,
 * Port was not necessarily created by a
 * privileged process.
 */
#define ATPORT_FIRST	1
#define ATPORT_RESERVED	128
#define ATPORT_LAST	255

/*
 * AppleTalk address.
 */
struct at_addr {
    u_short	s_net;
    u_char	s_node;
};

#if defined( BSD4_4 ) && !defined( __FreeBSD__ )
#define ATADDR_ANYNET	(u_short)0xffff
#else
#define ATADDR_ANYNET	(u_short)0x0000
#endif
#define ATADDR_ANYNODE	(u_char)0x00
#define ATADDR_ANYPORT	(u_char)0x00
#define ATADDR_BCAST	(u_char)0xff		/* There is no BCAST for NET */

/*
 * Socket address, AppleTalk style.  We keep magic information in the 
 * zero bytes.  There are three types, NONE, CONFIG which has the phase
 * and a net range, and IFACE which has the network address of an
 * interface.  IFACE may be filled in by the client, and is filled in
 * by the kernel.
 */
struct sockaddr_at {
#ifdef BSD4_4
    u_char		sat_len;
    u_char		sat_family;
#else BSD4_4
    short		sat_family;
#endif BSD4_4
    u_char		sat_port;
    struct at_addr	sat_addr;
#ifdef notdef
    struct {
	u_char		sh_type;
# define SATHINT_NONE	0
# define SATHINT_CONFIG	1
# define SATHINT_IFACE	2
	union {
	    char		su_zero[ 7 ];	/* XXX check size */
	    struct {
		u_char		sr_phase;
		u_short		sr_firstnet, sr_lastnet;
	    } su_range;
	    u_short		su_interface;
	} sh_un;
    } sat_hints;
#else notdef
    char		sat_zero[ 8 ];
#endif notdef
};

struct netrange {
    u_char		nr_phase;
    u_short		nr_firstnet;
    u_short		nr_lastnet;
};

#ifdef KERNEL
extern struct domain	atalkdomain;
extern struct protosw	atalksw[];
#endif

#endif __AT_HEADER__

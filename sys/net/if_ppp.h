/*
 * if_ppp.h - Point-to-Point Protocol definitions.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Modified by Paul Mackerras (paulus@cs.anu.edu.au)
 * Added PPP_MRU, sc_outm, sc_fastq, sc_bpf.
 *
 *	$Id: if_ppp.h,v 1.3 1993/11/07 17:46:59 wollman Exp $
 *	From: if_ppp.h,v 1.4 1993/08/29 11:22:37 paulus Exp $
 */

#ifndef _NET_IF_PPP_H_
#define _NET_IF_PPP_H_ 1

/*
 * Standard PPP header.
 */
struct ppp_header {
	u_char	ph_address;	/* Address Field */
	u_char	ph_control;	/* Control Field */
	u_short	ph_protocol;	/* Protocol Field */
};

#define PPP_HEADER_LEN	4	/* octets, must == sizeof(struct ppp_header) */
#define PPP_FCS_LEN	2	/* octets for FCS */

#define	PPP_ALLSTATIONS	0xff	/* All-Stations broadcast address */
#define	PPP_UI		0x03	/* Unnumbered Information */
#define	PPP_FLAG	0x7e	/* Flag Sequence */
#define	PPP_ESCAPE	0x7d	/* Asynchronous Control Escape */
#define	PPP_TRANS	0x20	/* Asynchronous transparency modifier */

/*
 * Protocol types.
 */
#define PPP_IP		0x21	/* Internet Protocol */
#define	PPP_XNS		0x25	/* Xerox NS */
#define	PPP_VJC_COMP	0x2d	/* VJ compressed TCP */
#define	PPP_VJC_UNCOMP	0x2f	/* VJ uncompressed TCP */
#define PPP_LCP		0xc021	/* Link Control Protocol */

/*
 * Important FCS values.
 */
#define PPP_INITFCS	0xffff	/* Initial FCS value */
#define PPP_GOODFCS	0xf0b8	/* Good final FCS value */
#define PPP_FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

#define	PPP_MTU		1500	/* Default MTU (size of Info field) */
#define PPP_MRU		1500	/* Default MRU (max receive unit) */
#define PPP_MAXMRU	65000	/* Largest MRU we allow */
#define	PPP_HIWAT	400	/* Don't start a new packet if HIWAT on que */

struct ppp_softc {
	struct ifnet sc_if;	/* network-visible interface */
	u_int	sc_flags;	/* see below */
	struct	tty *sc_ttyp;	/* pointer to tty structure */
	struct	mbuf *sc_outm;	/* mbuf chain being output currently */
	struct	mbuf *sc_m;	/* pointer to input mbuf chain */
	struct	mbuf *sc_mc;	/* pointer to current input mbuf */
	char	*sc_mp;		/* pointer to next char in input mbuf */
	short	sc_ilen;	/* length of input-packet-so-far */
	u_short	sc_fcs;		/* FCS so far (input) */
	u_short	sc_outfcs;	/* FCS so far for output packet */
	short	sc_mru;		/* max receive unit */
	u_long	sc_asyncmap;	/* async control character map */
	u_long	sc_rasyncmap;	/* receive async control char map */
	struct	ifqueue sc_inq;	/* TTY side input queue */
	struct	ifqueue sc_fastq; /* IP interactive output packet queue */
#ifdef	VJC
	struct	slcompress sc_comp; /* vjc control buffer */
#endif
	u_int	sc_bytessent;
	u_int	sc_bytesrcvd;
	caddr_t	sc_bpf;
};

/* flags */
#define SC_COMP_PROT	0x00000001	/* protocol compression (output) */
#define SC_COMP_AC	0x00000002	/* header compression (output) */
#define	SC_COMP_TCP	0x00000004	/* TCP (VJ) compression (output) */
#define SC_NO_TCP_CCID	0x00000008	/* disable VJ connection-id comp. */
#define SC_REJ_COMP_AC	0x00000010	/* reject adrs/ctrl comp. on input */
#define SC_REJ_COMP_TCP	0x00000020	/* reject TCP (VJ) comp. on input */
#define	SC_MASK		0x0000ffff	/* bits that user can change */

/* state bits */
#define	SC_ESCAPED	0x00010000	/* saw a PPP_ESCAPE */
#define	SC_FLUSH	0x00020000	/* flush input until next PPP_FLAG */

#define t_sc T_LINEP

/* this stuff doesn't belong here... */
#define	PPPIOCGFLAGS	_IOR('t', 90, int)	/* get configuration flags */
#define	PPPIOCSFLAGS	_IOW('t', 89, int)	/* set configuration flags */
#define	PPPIOCGASYNCMAP	_IOR('t', 88, int)	/* get async map */
#define	PPPIOCSASYNCMAP	_IOW('t', 87, int)	/* set async map */
#define	PPPIOCGUNIT	_IOR('t', 86, int)	/* get ppp unit number */
#define	PPPIOCGRASYNCMAP _IOR('t', 85, int)	/* get receive async map */
#define	PPPIOCSRASYNCMAP _IOW('t', 84, int)	/* set receive async map */
#define	PPPIOCGMRU	_IOR('t', 83, int)	/* get max receive unit */
#define	PPPIOCSMRU	_IOW('t', 82, int)	/* set max receive unit */

/* old copies of PPP may have defined this */
#if !defined(ifr_mtu)
#define ifr_mtu	ifr_metric
#endif

#endif /* _NET_IF_PPP_H_ */

/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD: src/usr.sbin/ppp/proto.h,v 1.2 1999/08/28 01:18:41 peter Exp $
 */

/*
 *  Definition of protocol numbers
 */
#define	PROTO_IP	0x0021	/* IP */
#define	PROTO_VJUNCOMP	0x002f	/* VJ Uncompressed */
#define	PROTO_VJCOMP	0x002d	/* VJ Compressed */
#define	PROTO_MP	0x003d	/* Multilink fragment */
#define	PROTO_ICOMPD	0x00fb	/* Individual link compressed */
#define	PROTO_COMPD	0x00fd	/* Compressed datagram */

#define PROTO_COMPRESSIBLE(p) (((p) & 0xffe1) == 0x21)

#define	PROTO_IPCP	0x8021
#define	PROTO_ICCP	0x80fb
#define	PROTO_CCP	0x80fd

#define	PROTO_LCP	0xc021
#define	PROTO_PAP	0xc023
#define	PROTO_CBCP	0xc029
#define	PROTO_LQR	0xc025
#define	PROTO_CHAP	0xc223

struct lcp;

extern int proto_WrapperOctets(struct lcp *, u_short);
struct mbuf *proto_Prepend(struct mbuf *, u_short, unsigned, int);

extern struct layer protolayer;

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
 * $Id: lcpproto.h,v 1.7 1997/06/09 03:27:25 brian Exp $
 *
 *	TODO:
 */

#ifndef _LCPPROTO_H_
#define _LCPPROTO_H_

/*
 *  Definition of protocol numbers
 */
#define	PROTO_IP	0x0021	/* IP */
#define	PROTO_VJUNCOMP	0x002f	/* VJ Uncompressed */
#define	PROTO_VJCOMP	0x002d	/* VJ Compressed */
#define	PROTO_ICOMPD	0x00fb	/* Individual link compressed */
#define	PROTO_COMPD	0x00fd	/* Compressed datagram */

#define	PROTO_IPCP	0x8021
#define	PROTO_ICCP	0x80fb
#define	PROTO_CCP	0x80fd

#define	PROTO_LCP	0xc021
#define	PROTO_PAP	0xc023
#define	PROTO_LQR	0xc025
#define	PROTO_CHAP	0xc223

extern void LcpInput(struct mbuf * bp);
extern void PapInput(struct mbuf * bp);
extern void LqpInput(struct mbuf * bp);
extern void ChapInput(struct mbuf * bp);
extern void IpInput(struct mbuf * bp);
extern struct mbuf *VjCompInput(struct mbuf * bp, int proto);
extern void IpcpInput(struct mbuf * bp);
extern void LqrInput(struct mbuf * bp);

#endif

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
 * $Id: pap.h,v 1.5.2.5 1998/05/01 19:25:32 brian Exp $
 *
 *	TODO:
 */

#define	PAP_REQUEST	1
#define	PAP_ACK		2
#define	PAP_NAK		3

struct mbuf;
struct physical;
struct authinfo;
struct bundle;

extern void pap_Input(struct bundle *, struct mbuf *, struct physical *);
extern void pap_SendChallenge(struct authinfo *, int, struct physical *);

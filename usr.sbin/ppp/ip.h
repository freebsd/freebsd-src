/*
 *			User Process PPP
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: ip.h,v 1.8.2.9 1998/05/01 19:24:46 brian Exp $
 *
 */

struct mbuf;
struct filter;
struct link;
struct bundle;

extern int ip_FlushPacket(struct link *, struct bundle *);
extern int  PacketCheck(struct bundle *, char *, int, struct filter *);
extern void ip_Enqueue(int, char *, int);
extern void ip_Input(struct bundle *, struct mbuf *);
extern int  ip_QueueLen(void);

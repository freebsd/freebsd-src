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
 * $FreeBSD$
 *
 */

struct mbuf;
struct filter;
struct link;
struct bundle;

extern int ip_PushPacket(struct link *, struct bundle *);
extern int PacketCheck(struct bundle *, unsigned char *, int, struct filter *,
                       const char *, unsigned *secs);
extern void ip_Enqueue(struct ipcp *, int, char *, int);
extern struct mbuf *ip_Input(struct bundle *, struct link *, struct mbuf *);
extern void ip_DeleteQueue(struct ipcp *);
extern size_t ip_QueueLen(struct ipcp *);

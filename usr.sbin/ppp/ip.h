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
 * $Id: ip.h,v 1.8.2.1 1998/01/29 00:49:23 brian Exp $
 *
 */

extern void IpStartOutput(struct link *);
extern int  PacketCheck(char *, int, int);
extern void IpEnqueue(int, char *, int);
extern void IpInput(struct mbuf *);
extern void StartIdleTimer(void);
extern void StopIdleTimer(void);
extern void UpdateIdleTimer(void);
extern int  RemainingIdleTime(void);

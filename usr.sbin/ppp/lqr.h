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
 * $Id: lqr.h,v 1.8 1997/10/26 01:03:10 brian Exp $
 *
 *	TODO:
 */

/*
 *  Structure of LQR packet defined in RFC1333
 */
struct lqrdata {
  u_long MagicNumber;
  u_long LastOutLQRs;
  u_long LastOutPackets;
  u_long LastOutOctets;
  u_long PeerInLQRs;
  u_long PeerInPackets;
  u_long PeerInDiscards;
  u_long PeerInErrors;
  u_long PeerInOctets;
  u_long PeerOutLQRs;
  u_long PeerOutPackets;
  u_long PeerOutOctets;
};

struct lqrsave {
  u_long SaveInLQRs;
  u_long SaveInPackets;
  u_long SaveInDiscards;
  u_long SaveInErrors;
  u_long SaveInOctets;
};

struct lqrdata MyLqrData, HisLqrData;
struct lqrsave HisLqrSave;

/*
 *  We support LQR and ECHO as LQM method
 */
#define	LQM_LQR	  1
#define	LQM_ECHO  2

extern void LqrDump(char *, struct lqrdata *);
extern void LqrChangeOrder(struct lqrdata *, struct lqrdata *);
extern void StartLqm(void);
extern void StopLqr(int);
extern void StopLqrTimer(void);
extern void RecvEchoLqr(struct mbuf *);
extern void LqrInput(struct mbuf *);

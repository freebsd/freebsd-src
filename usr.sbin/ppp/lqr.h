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
 * $Id: lqr.h,v 1.12.2.6 1998/05/08 01:15:09 brian Exp $
 *
 *	TODO:
 */

/*
 *  Structure of LQR packet defined in RFC1989
 */
struct lqrdata {
  u_int32_t MagicNumber;
  u_int32_t LastOutLQRs;	/* most recently received PeerOutLQRs */
  u_int32_t LastOutPackets;	/* most recently received PeerOutPackets */
  u_int32_t LastOutOctets;	/* most recently received PeerOutOctets */
  u_int32_t PeerInLQRs;		/* Peers SaveInLQRs */
  u_int32_t PeerInPackets;	/* Peers SaveInPackets */
  u_int32_t PeerInDiscards;	/* Peers SaveInDiscards */
  u_int32_t PeerInErrors;	/* Peers SaveInErrors */
  u_int32_t PeerInOctets;	/* Peers SaveInOctets */
  u_int32_t PeerOutLQRs;	/* Peers OutLQRs (hdlc.h) */
  u_int32_t PeerOutPackets;	/* Peers OutPackets (hdlc.h) */
  u_int32_t PeerOutOctets;	/* Peers OutOctets (hdlc.h) */
};

/*
 *  We support LQR and ECHO as LQM method
 */
#define	LQM_LQR	  1
#define	LQM_ECHO  2

struct mbuf;
struct physical;
struct lcp;
struct fsm;

extern void lqr_Dump(const char *, const char *, const struct lqrdata *);
extern void lqr_ChangeOrder(struct lqrdata *, struct lqrdata *);
extern void lqr_Start(struct lcp *);
extern void lqr_reStart(struct lcp *);
extern void lqr_Stop(struct physical *, int);
extern void lqr_StopTimer(struct physical *);
extern void lqr_RecvEcho(struct fsm *, struct mbuf *);
extern void lqr_Input(struct physical *, struct mbuf *);

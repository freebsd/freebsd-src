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
 * $Id: lqr.h,v 1.3.2.2 1997/08/25 00:34:31 brian Exp $
 *
 *	TODO:
 */

/*
 *  Structure of LQR packet defined in RFC1333
 */
struct lqrdata {
  u_int32_t MagicNumber;
  u_int32_t LastOutLQRs;
  u_int32_t LastOutPackets;
  u_int32_t LastOutOctets;
  u_int32_t PeerInLQRs;
  u_int32_t PeerInPackets;
  u_int32_t PeerInDiscards;
  u_int32_t PeerInErrors;
  u_int32_t PeerInOctets;
  u_int32_t PeerOutLQRs;
  u_int32_t PeerOutPackets;
  u_int32_t PeerOutOctets;
};

struct lqrsave {
  u_int32_t SaveInLQRs;
  u_int32_t SaveInPackets;
  u_int32_t SaveInDiscards;
  u_int32_t SaveInErrors;
  u_int32_t SaveInOctets;
};

extern struct lqrdata MyLqrData, HisLqrData;
extern struct lqrsave HisLqrSave;

/*
 *  We support LQR and ECHO as LQM method
 */
#define	LQM_LQR	  1
#define	LQM_ECHO  2

extern void LqrDump(const char *, const struct lqrdata *);
extern void LqrChangeOrder(struct lqrdata *, struct lqrdata *);
extern void StartLqm(void);
extern void StopLqr(int);
extern void StopLqrTimer(void);
extern void RecvEchoLqr(struct mbuf *);
extern void LqrInput(struct mbuf *);

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
 * $Id: lqr.h,v 1.2 1995/02/26 12:17:40 amurai Exp $
 *
 *	TODO:
 */

#ifndef _LQR_H_
#define	_LQR_H_

/*
 *  Structure of LQR packet defined in RFC1333
 */
struct lqrdata {
  u_long  MagicNumber;
  u_long  LastOutLQRs;
  u_long  LastOutPackets;
  u_long  LastOutOctets;
  u_long  PeerInLQRs;
  u_long  PeerInPackets;
  u_long  PeerInDiscards;
  u_long  PeerInErrors;
  u_long  PeerInOctets;
  u_long  PeerOutLQRs;
  u_long  PeerOutPackets;
  u_long  PeerOutOctets;
};

struct lqrsave {
  u_long  SaveInLQRs;
  u_long  SaveInPackets;
  u_long  SaveInDiscards;
  u_long  SaveInErrors;
  u_long  SaveInOctets;
};

struct lqrdata MyLqrData, HisLqrData;
struct lqrsave HisLqrSave;

/*
 *  We support LQR and ECHO as LQM method
 */
#define	LQM_LQR	  1
#define	LQM_ECHO  2

extern void LqrDump __P((char *, struct lqrdata *));
extern void LqrChangeOrder __P((struct lqrdata *, struct lqrdata *));
extern void StartLqm __P((void));
extern void StopLqr __P((int));
extern void StopLqrTimer __P((void));
extern void RecvEchoLqr __P((struct mbuf *));
#endif

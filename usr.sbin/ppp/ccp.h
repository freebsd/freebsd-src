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
 * $FreeBSD$
 *
 *	TODO:
 */

#ifndef _CCP_H_
#define	_CCP_H_

#define	CCP_MAXCODE	CODE_RESETACK

#define	TY_OUI		0	/* OUI */
#define	TY_PRED1	1	/* Predictor type 1 */
#define	TY_PRED2	2	/* Predictor type 2 */
#define	TY_PUDDLE	3	/* Puddle Jumper */
#define	TY_HWPPC	16	/* Hewlett-Packard PPC */
#define	TY_STAC		17	/* Stac Electronics LZS */
#define	TY_MSPPC	18	/* Microsoft PPC */
#define	TY_GAND		19	/* Gandalf FZA */
#define	TY_V42BIS	20	/* V.42bis compression */
#define	TY_BSD		21	/* BSD LZW Compress */

struct ccpstate {
  u_long  his_proto;		/* peer's compression protocol */
  u_long  want_proto;		/* my compression protocol */

  u_long  his_reject;		/* Request codes rejected by peer */
  u_long  my_reject;		/* Request codes I have rejected */

  u_long  orgout, compout;
  u_long  orgin, compin;
};

extern struct ccpstate CcpInfo;

void CcpRecvResetReq __P((struct fsm *));
void CcpSendResetReq __P((struct fsm *));
void CcpInput __P((struct mbuf *));
void CcpUp __P((void));
void CcpOpen __P((void));
void CcpInit __P((void));
#endif

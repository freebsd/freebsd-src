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
 * $Id: chap.h,v 1.9.2.6 1998/05/01 19:24:05 brian Exp $
 *
 *	TODO:
 */

struct mbuf;
struct physical;
struct bundle;

#define	CHAP_CHALLENGE	1
#define	CHAP_RESPONSE	2
#define	CHAP_SUCCESS	3
#define	CHAP_FAILURE	4

struct chap {
  struct authinfo auth;
  char challenge_data[80];
  int challenge_len;
  unsigned using_MSChap : 1;	/* A combination of MD4 & DES */
};

#define auth2chap(a) ((struct chap *)(a))

extern void chap_Input(struct bundle *, struct mbuf *, struct physical *);
extern void chap_SendChallenge(struct authinfo *, int, struct physical *);

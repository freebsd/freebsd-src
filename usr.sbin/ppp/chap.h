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
 * $FreeBSD: src/usr.sbin/ppp/chap.h,v 1.17.2.1 2000/03/21 10:23:02 brian Exp $
 *
 *	TODO:
 */

struct mbuf;
struct physical;

#define	CHAP_CHALLENGE	1
#define	CHAP_RESPONSE	2
#define	CHAP_SUCCESS	3
#define	CHAP_FAILURE	4

struct chap {
  struct fdescriptor desc;
  struct {
    pid_t pid;
    int fd;
    struct {
      char ptr[AUTHLEN * 2 + 3];	/* Allow for \r\n at the end (- NUL) */
      int len;
    } buf;
  } child;
  struct authinfo auth;
  struct {
    u_char local[CHAPCHALLENGELEN + AUTHLEN];	/* I invented this one */
    u_char peer[CHAPCHALLENGELEN + AUTHLEN];	/* Peer gave us this one */
  } challenge;
#ifdef HAVE_DES
  unsigned NTRespSent : 1;		/* Our last response */
  int peertries;
#endif
};

#define descriptor2chap(d) \
  ((d)->type == CHAP_DESCRIPTOR ? (struct chap *)(d) : NULL)
#define auth2chap(a) (struct chap *)((char *)a - (int)&((struct chap *)0)->auth)

extern void chap_Init(struct chap *, struct physical *);
extern void chap_ReInit(struct chap *);
extern struct mbuf *chap_Input(struct bundle *, struct link *, struct mbuf *);

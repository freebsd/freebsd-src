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
 * $FreeBSD: src/usr.sbin/ppp/auth.h,v 1.18 1999/08/28 01:18:16 peter Exp $
 *
 *	TODO:
 */

struct physical;
struct bundle;
struct authinfo;
typedef void (*auth_func)(struct authinfo *);

struct authinfo {
  struct {
    auth_func req;
    auth_func success;
    auth_func failure;
  } fn;
  struct {
    struct fsmheader hdr;
    char name[AUTHLEN];
  } in;
  struct pppTimer authtimer;
  int retry;
  int id;
  struct physical *physical;
  struct {
    struct fsm_retry fsm;	/* How often/frequently to resend requests */
  } cfg;
};

#define auth_Failure(a) (*a->fn.failure)(a);
#define auth_Success(a) (*a->fn.success)(a);

extern const char *Auth2Nam(u_short, u_char);
extern void auth_Init(struct authinfo *, struct physical *,
                      auth_func, auth_func, auth_func);
extern void auth_StopTimer(struct authinfo *);
extern void auth_StartReq(struct authinfo *);
extern int auth_Validate(struct bundle *, const char *, const char *,
                         struct physical *);
extern char *auth_GetSecret(struct bundle *, const char *, int,
                            struct physical *);
extern int auth_SetPhoneList(const char *, char *, int);
extern int auth_Select(struct bundle *, const char *);
extern struct mbuf *auth_ReadHeader(struct authinfo *, struct mbuf *);
extern struct mbuf *auth_ReadName(struct authinfo *, struct mbuf *, int);

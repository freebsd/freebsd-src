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
 * $Id: auth.h,v 1.10.2.9 1998/05/01 19:23:54 brian Exp $
 *
 *	TODO:
 */

struct physical;
struct bundle;

struct authinfo {
  void (*ChallengeFunc)(struct authinfo *, int, struct physical *);
  struct pppTimer authtimer;
  int retry;
  int id;
  struct physical *physical;
  struct {
    u_int fsmretry;
  } cfg;
};

extern const char *Auth2Nam(u_short);

extern void auth_Init(struct authinfo *);
extern void auth_StopTimer(struct authinfo *);
extern void auth_StartChallenge(struct authinfo *, struct physical *,
                                void (*fn)(struct authinfo *, int,
                                struct physical *));
extern int auth_Validate(struct bundle *, const char *, const char *,
                         struct physical *);
extern char *auth_GetSecret(struct bundle *, const char *, int,
                            struct physical *);
extern int auth_Select(struct bundle *, const char *, struct physical *);

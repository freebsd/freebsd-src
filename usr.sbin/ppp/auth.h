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
 * $Id: auth.h,v 1.10.2.3 1998/02/07 20:49:20 brian Exp $
 *
 *	TODO:
 */

struct physical;

typedef enum {
  VALID,
  INVALID,
  NOT_FOUND
} LOCAL_AUTH_VALID;

struct authinfo {
  void (*ChallengeFunc)(struct authinfo *, int, struct physical *);
  struct pppTimer authtimer;
  int retry;
  int id;
  struct physical *physical;
};

extern void authinfo_Init(struct authinfo *);

extern const char *Auth2Nam(u_short);
extern LOCAL_AUTH_VALID LocalAuthValidate(const char *, const char *, const char *);
extern void StopAuthTimer(struct authinfo *);
extern void StartAuthChallenge(struct authinfo *, struct physical *,
                   void (*fn)(struct authinfo *, int, struct physical *));
extern void LocalAuthInit(void);
extern int AuthValidate(struct bundle *, const char *, const char *,
                        const char *, struct physical *);
extern char *AuthGetSecret(struct bundle *, const char *, const char *, int,
                           int, struct physical *);

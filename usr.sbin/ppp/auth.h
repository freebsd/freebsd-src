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
 * $Id: auth.h,v 1.9 1997/10/26 01:02:09 brian Exp $
 *
 *	TODO:
 */

typedef enum {
  VALID,
  INVALID,
  NOT_FOUND
} LOCAL_AUTH_VALID;

struct authinfo {
  void (*ChallengeFunc) (int);
  struct pppTimer authtimer;
  int retry;
  int id;
};

extern LOCAL_AUTH_VALID LocalAuthValidate(const char *, const char *, const char *);
extern void StopAuthTimer(struct authinfo *);
extern void StartAuthChallenge(struct authinfo *);
extern void LocalAuthInit(void);
extern int AuthValidate(const char *, const char *, const char *);
extern char *AuthGetSecret(const char *, const char *, int, int);

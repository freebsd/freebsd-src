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
 * $Id: auth.h,v 1.5 1997/02/22 16:10:02 peter Exp $
 *
 *	TODO:
 */

#ifndef _AUTH_H_
#define	_AUTH_H_

typedef enum { VALID, INVALID, NOT_FOUND } LOCAL_AUTH_VALID;
LOCAL_AUTH_VALID	LocalAuthValidate( char *, char *, char *);

struct authinfo {
  void (*ChallengeFunc)();
  struct pppTimer authtimer;
  int retry;
  int id;
};
extern struct authinfo AuthPapInfo;
extern struct authinfo AuthChapInfo;

extern void SendPapChallenge(int);
extern void SendChapChallenge(int);
extern void StopAuthTimer(struct authinfo *);
extern void StartAuthChallenge(struct authinfo *);
extern LOCAL_AUTH_VALID LocalAuthInit(void);
extern int AuthValidate(char *, char *, char *);
#endif

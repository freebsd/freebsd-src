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
 * $Id: auth.h,v 1.2 1995/05/30 03:50:26 rgrimes Exp $
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

extern void SendPapChallenge __P((int));
extern void SendChapChallenge __P((int));
extern void StopAuthTimer __P((struct authinfo *));
extern void StartAuthChallenge __P((struct authinfo *));
extern LOCAL_AUTH_VALID LocalAuthInit __P((void));
extern int AuthValidate __P((char *, char *, char *));
#endif

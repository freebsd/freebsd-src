/*
 *			PPP Secret Key Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1994, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: auth.c,v 1.11 1997/05/07 23:01:21 brian Exp $
 *
 *	TODO:
 *		o Implement check against with registered IP addresses.
 */
#include "fsm.h"
#include "lcpproto.h"
#include "ipcp.h"
#include "vars.h"
#include "filter.h"
#include "auth.h"
#include "chat.h"

extern FILE *OpenSecret();
extern void CloseSecret();

LOCAL_AUTH_VALID
LocalAuthInit(void)
{

  char *p;

  if ( gethostname( VarShortHost, sizeof(VarShortHost))) {
  	return(NOT_FOUND);
  }
  p = strchr( VarShortHost, '.' );
  if (p) 
	*p = '\0';

  VarLocalAuth = LOCAL_NO_AUTH;
  return LocalAuthValidate( SECRETFILE, VarShortHost, "" );

}

LOCAL_AUTH_VALID
LocalAuthValidate( char *fname, char *system, char *key) {
  FILE *fp;
  int n;
  char *vector[20];	/* XXX */
  char buff[200];	/* XXX */
  LOCAL_AUTH_VALID rc;

  rc = NOT_FOUND;		/* No system entry */
  fp = OpenSecret(fname);
  if (fp == NULL)
    return( rc );
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff)-1] = 0;
    bzero(vector, sizeof(vector));
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 1)
      continue;
    if (strcmp(vector[0], system) == 0) {
      if ( vector[1] != (char *) NULL && strcmp(vector[1], key) == 0) {
	rc = VALID;		/* Valid   */
      } else {
	rc = INVALID;		/* Invalid */
      }
      break;
    }
  }
  CloseSecret(fp);
  return( rc );
}

int
AuthValidate(fname, system, key)
char *fname, *system, *key;
{
  FILE *fp;
  int n;
  char *vector[20];
  char buff[200];
  char passwd[100];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return(0);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff)-1] = 0;
    bzero(vector, sizeof(vector));
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 2)
      continue;
    if (strcmp(vector[0], system) == 0) {
      ExpandString(vector[1], passwd, sizeof(passwd), 0);
      if (strcmp(passwd, key) == 0) {
	CloseSecret(fp);
        bzero(&DefHisAddress, sizeof(DefHisAddress));
        n -= 2;
        if (n > 0) {
	  ParseAddr(n--, &vector[2],
	    &DefHisAddress.ipaddr, &DefHisAddress.mask, &DefHisAddress.width);
	}
	IpcpInit();
	return(1);	/* Valid */
      }
    }
  }
  CloseSecret(fp);
  return(0);		/* Invalid */
}

char *
AuthGetSecret(fname, system, len, setaddr)
char *fname, *system;
int len, setaddr;
{
  FILE *fp;
  int n;
  char *vector[20];
  char buff[200];
  static char passwd[100];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return(NULL);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff)-1] = 0;
    bzero(vector, sizeof(vector));
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 2)
      continue;
    if (strlen(vector[0]) == len && strncmp(vector[0], system, len) == 0) {
      ExpandString(vector[1], passwd, sizeof(passwd), 0);
      if (setaddr) {
        bzero(&DefHisAddress, sizeof(DefHisAddress));
      }
      n -= 2;
      if (n > 0 && setaddr) {
#ifdef DEBUG
	LogPrintf(LOG_LCP_BIT, "*** n = %d, %s\n", n, vector[2]);
#endif
	ParseAddr(n--, &vector[2],
	  &DefHisAddress.ipaddr, &DefHisAddress.mask, &DefHisAddress.width);
	IpcpInit();
      }
      return(passwd);
    }
  }
  CloseSecret(fp);
  return(NULL);		/* Invalid */
}

static void
AuthTimeout(authp)
struct authinfo *authp;
{
  struct pppTimer *tp;

  tp = &authp->authtimer;
  StopTimer(tp);
  if (--authp->retry > 0) {
    StartTimer(tp);
    (authp->ChallengeFunc)(++authp->id);
  }
}

void
StartAuthChallenge(authp)
struct authinfo *authp;
{
  struct pppTimer *tp;

  tp = &authp->authtimer;
  StopTimer(tp);
  tp->func = AuthTimeout;
  tp->load = VarRetryTimeout * SECTICKS;
  tp->state = TIMER_STOPPED;
  tp->arg = (void *)authp;
  StartTimer(tp);
  authp->retry = 3;
  authp->id = 1;
  (authp->ChallengeFunc)(authp->id);
}

void
StopAuthTimer(authp)
struct authinfo *authp;
{
  StopTimer(&authp->authtimer);
}

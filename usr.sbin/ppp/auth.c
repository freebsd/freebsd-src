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
 * $Id: auth.c,v 1.23 1997/11/17 00:42:37 brian Exp $
 *
 *	TODO:
 *		o Implement check against with registered IP addresses.
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "ipcp.h"
#include "loadalias.h"
#include "vars.h"
#include "filter.h"
#include "auth.h"
#include "chat.h"
#include "systems.h"

void
LocalAuthInit()
{
  if (!(mode&MODE_DAEMON))
    /* We're allowed in interactive mode */
    VarLocalAuth = LOCAL_AUTH;
  else if (VarHaveLocalAuthKey)
    VarLocalAuth = *VarLocalAuthKey == '\0' ? LOCAL_AUTH : LOCAL_NO_AUTH;
  else
    switch (LocalAuthValidate(SECRETFILE, VarShortHost, "")) {
    case NOT_FOUND:
      VarLocalAuth = LOCAL_DENY;
      break;
    case VALID:
      VarLocalAuth = LOCAL_AUTH;
      break;
    case INVALID:
      VarLocalAuth = LOCAL_NO_AUTH;
      break;
    }
}

LOCAL_AUTH_VALID
LocalAuthValidate(const char *fname, const char *system, const char *key)
{
  FILE *fp;
  int n;
  char *vector[3];
  char buff[LINE_LEN];
  LOCAL_AUTH_VALID rc;

  rc = NOT_FOUND;		/* No system entry */
  fp = OpenSecret(fname);
  if (fp == NULL)
    return (rc);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff) - 1] = 0;
    memset(vector, '\0', sizeof(vector));
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 1)
      continue;
    if (strcmp(vector[0], system) == 0) {
      if ((vector[1] == (char *) NULL && (key == NULL || *key == '\0')) ||
          (vector[1] != (char *) NULL && strcmp(vector[1], key) == 0)) {
	rc = VALID;		/* Valid   */
      } else {
	rc = INVALID;		/* Invalid */
      }
      break;
    }
  }
  CloseSecret(fp);
  return (rc);
}

int
AuthValidate(const char *fname, const char *system, const char *key)
{
  FILE *fp;
  int n;
  char *vector[4];
  char buff[LINE_LEN];
  char passwd[100];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return (0);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff) - 1] = 0;
    memset(vector, '\0', sizeof(vector));
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 2)
      continue;
    if (strcmp(vector[0], system) == 0) {
      ExpandString(vector[1], passwd, sizeof(passwd), 0);
      if (strcmp(passwd, key) == 0) {
	CloseSecret(fp);
	memset(&DefHisAddress, '\0', sizeof(DefHisAddress));
	n -= 2;
	if (n > 0) {
	  if (ParseAddr(n--, (char const *const *)(vector+2),
			&DefHisAddress.ipaddr,
			&DefHisAddress.mask,
			&DefHisAddress.width) == 0) {
	    return (0);		/* Invalid */
	  }
	}
	IpcpInit();
	return (1);		/* Valid */
      }
    }
  }
  CloseSecret(fp);
  return (0);			/* Invalid */
}

char *
AuthGetSecret(const char *fname, const char *system, int len, int setaddr)
{
  FILE *fp;
  int n;
  char *vector[4];
  char buff[LINE_LEN];
  static char passwd[100];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return (NULL);
  while (fgets(buff, sizeof(buff), fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff) - 1] = 0;
    memset(vector, '\0', sizeof(vector));
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 2)
      continue;
    if (strlen(vector[0]) == len && strncmp(vector[0], system, len) == 0) {
      ExpandString(vector[1], passwd, sizeof(passwd), 0);
      if (setaddr) {
	memset(&DefHisAddress, '\0', sizeof(DefHisAddress));
      }
      n -= 2;
      if (n > 0 && setaddr) {
	LogPrintf(LogDEBUG, "AuthGetSecret: n = %d, %s\n", n, vector[2]);
	if (ParseAddr(n--, (char const *const *)(vector+2),
		      &DefHisAddress.ipaddr,
		      &DefHisAddress.mask,
		      &DefHisAddress.width) != 0)
	  IpcpInit();
      }
      return (passwd);
    }
  }
  CloseSecret(fp);
  return (NULL);		/* Invalid */
}

static void
AuthTimeout(void *vauthp)
{
  struct pppTimer *tp;
  struct authinfo *authp = (struct authinfo *)vauthp;

  tp = &authp->authtimer;
  StopTimer(tp);
  if (--authp->retry > 0) {
    StartTimer(tp);
    (authp->ChallengeFunc) (++authp->id);
  }
}

void
StartAuthChallenge(struct authinfo *authp)
{
  struct pppTimer *tp;

  tp = &authp->authtimer;
  StopTimer(tp);
  tp->func = AuthTimeout;
  tp->load = VarRetryTimeout * SECTICKS;
  tp->state = TIMER_STOPPED;
  tp->arg = (void *) authp;
  StartTimer(tp);
  authp->retry = 3;
  authp->id = 1;
  (authp->ChallengeFunc) (authp->id);
}

void
StopAuthTimer(struct authinfo *authp)
{
  StopTimer(&authp->authtimer);
}

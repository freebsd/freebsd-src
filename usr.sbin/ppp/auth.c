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
 * $Id: auth.c,v 1.27.2.22 1998/04/19 15:24:34 brian Exp $
 *
 *	TODO:
 *		o Implement check against with registered IP addresses.
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mbuf.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "auth.h"
#include "systems.h"
#include "lcp.h"
#include "lqr.h"
#include "hdlc.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "chat.h"
#include "lcpproto.h"
#include "filter.h"
#include "mp.h"
#include "bundle.h"

const char *
Auth2Nam(u_short auth)
{
  switch (auth) {
  case PROTO_PAP:
    return "PAP";
  case PROTO_CHAP:
    return "CHAP";
  case 0:
    return "none";
  }
  return "unknown";
}

static int
auth_CheckPasswd(const char *name, const char *data, const char *key)
{
  if (!strcmp(data, "*")) {
    /* Then look up the real password database */
    struct passwd *pw;
    int result;

    result = (pw = getpwnam(name)) &&
             !strcmp(crypt(key, pw->pw_passwd), pw->pw_passwd);
    endpwent();
    return result;
  }

  return !strcmp(data, key);
}

int
AuthValidate(struct bundle *bundle, const char *fname, const char *system,
             const char *key, struct physical *physical)
{
  /* Used by PAP routines */

  FILE *fp;
  int n;
  char *vector[5];
  char buff[LINE_LEN];

  fp = OpenSecret(fname);
  if (fp != NULL) {
    while (fgets(buff, sizeof buff, fp)) {
      if (buff[0] == '#')
        continue;
      buff[strlen(buff) - 1] = 0;
      memset(vector, '\0', sizeof vector);
      n = MakeArgs(buff, vector, VECSIZE(vector));
      if (n < 2)
        continue;
      if (strcmp(vector[0], system) == 0)
        if (auth_CheckPasswd(vector[0], vector[1], key)) {
	  CloseSecret(fp);
	  if (n > 2 && !UseHisaddr(bundle, vector[2], 1))
	      return (0);
          /* XXX This should be deferred - we may join an existing bundle ! */
	  ipcp_Setup(&bundle->ncp.ipcp);
	  if (n > 3)
	    bundle_SetLabel(bundle, vector[3]);
	  return 1;		/* Valid */
        } else {
          CloseSecret(fp);
          return 0;		/* Invalid */
        }
    }
    CloseSecret(fp);
  }

#ifndef NOPASSWDAUTH
  if (Enabled(bundle, OPT_PASSWDAUTH))
    return auth_CheckPasswd(system, "*", key);
#endif

  return 0;			/* Invalid */
}

char *
AuthGetSecret(struct bundle *bundle, const char *fname, const char *system,
              int len, int setaddr, struct physical *physical)
{
  /* Used by CHAP routines */

  FILE *fp;
  int n;
  char *vector[5];
  static char buff[LINE_LEN];

  fp = OpenSecret(fname);
  if (fp == NULL)
    return (NULL);

  while (fgets(buff, sizeof buff, fp)) {
    if (buff[0] == '#')
      continue;
    buff[strlen(buff) - 1] = 0;
    memset(vector, '\0', sizeof vector);
    n = MakeArgs(buff, vector, VECSIZE(vector));
    if (n < 2)
      continue;
    if (strlen(vector[0]) == len && strncmp(vector[0], system, len) == 0) {
      if (setaddr)
	memset(&bundle->ncp.ipcp.cfg.peer_range, '\0',
               sizeof bundle->ncp.ipcp.cfg.peer_range);
      if (n > 2 && setaddr)
	if (UseHisaddr(bundle, vector[2], 1))
          /* XXX This should be deferred - we may join an existing bundle ! */
	  ipcp_Setup(&bundle->ncp.ipcp);
        else
          return NULL;
      if (n > 3)
        bundle_SetLabel(bundle, vector[3]);
      return vector[1];
    }
  }
  CloseSecret(fp);
  return (NULL);		/* Invalid */
}

static void
AuthTimeout(void *vauthp)
{
  struct authinfo *authp = (struct authinfo *)vauthp;

  StopTimer(&authp->authtimer);
  if (--authp->retry > 0) {
    StartTimer(&authp->authtimer);
    (*authp->ChallengeFunc)(authp, ++authp->id, authp->physical);
  }
}

void
authinfo_Init(struct authinfo *authinfo)
{
  memset(authinfo, '\0', sizeof(struct authinfo));
  authinfo->cfg.fsmretry = DEF_FSMRETRY;
}

void
StartAuthChallenge(struct authinfo *authp, struct physical *physical,
                   void (*fn)(struct authinfo *, int, struct physical *))
{
  authp->ChallengeFunc = fn;
  authp->physical = physical;
  StopTimer(&authp->authtimer);
  authp->authtimer.func = AuthTimeout;
  authp->authtimer.name = "auth";
  authp->authtimer.load = authp->cfg.fsmretry * SECTICKS;
  authp->authtimer.arg = (void *) authp;
  authp->retry = 3;
  authp->id = 1;
  (*authp->ChallengeFunc)(authp, authp->id, physical);
  StartTimer(&authp->authtimer);
}

void
StopAuthTimer(struct authinfo *authp)
{
  StopTimer(&authp->authtimer);
  authp->physical = NULL;
}

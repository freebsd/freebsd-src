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
 * $Id: auth.c,v 1.29 1998/05/21 21:44:00 brian Exp $
 *
 *	TODO:
 *		o Implement check against with registered IP addresses.
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

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
auth_Select(struct bundle *bundle, const char *name, struct physical *physical)
{
  FILE *fp;
  int n;
  char *vector[5];
  char buff[LINE_LEN];

  if (*name == '\0') {
    ipcp_Setup(&bundle->ncp.ipcp);
    return 1;
  }

  fp = OpenSecret(SECRETFILE);
  if (fp != NULL) {
    while (fgets(buff, sizeof buff, fp)) {
      if (buff[0] == '#')
        continue;
      buff[strlen(buff) - 1] = 0;
      memset(vector, '\0', sizeof vector);
      n = MakeArgs(buff, vector, VECSIZE(vector));
      if (n < 2)
        continue;
      if (strcmp(vector[0], name) == 0)
	CloseSecret(fp);
/*
	memset(&bundle->ncp.ipcp.cfg.peer_range, '\0',
               sizeof bundle->ncp.ipcp.cfg.peer_range);
*/
	if (n > 2 && !ipcp_UseHisaddr(bundle, vector[2], 1))
	  return 0;
	ipcp_Setup(&bundle->ncp.ipcp);
	if (n > 3)
	  bundle_SetLabel(bundle, vector[3]);
	return 1;		/* Valid */
    }
    CloseSecret(fp);
  }

#ifndef NOPASSWDAUTH
  /* Let 'em in anyway - they must have been in the passwd file */
  ipcp_Setup(&bundle->ncp.ipcp);
  return 1;
#else
  /* Disappeared from ppp.secret ? */
  return 0;
#endif
}

int
auth_Validate(struct bundle *bundle, const char *system,
             const char *key, struct physical *physical)
{
  /* Used by PAP routines */

  FILE *fp;
  int n;
  char *vector[5];
  char buff[LINE_LEN];

  fp = OpenSecret(SECRETFILE);
  if (fp != NULL) {
    while (fgets(buff, sizeof buff, fp)) {
      if (buff[0] == '#')
        continue;
      buff[strlen(buff) - 1] = 0;
      memset(vector, '\0', sizeof vector);
      n = MakeArgs(buff, vector, VECSIZE(vector));
      if (n < 2)
        continue;
      if (strcmp(vector[0], system) == 0) {
	CloseSecret(fp);
        return auth_CheckPasswd(vector[0], vector[1], key);
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
auth_GetSecret(struct bundle *bundle, const char *system, int len,
              struct physical *physical)
{
  /* Used by CHAP routines */

  FILE *fp;
  int n;
  char *vector[5];
  char buff[LINE_LEN];

  fp = OpenSecret(SECRETFILE);
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
      CloseSecret(fp);
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

  timer_Stop(&authp->authtimer);
  if (--authp->retry > 0) {
    timer_Start(&authp->authtimer);
    (*authp->ChallengeFunc)(authp, ++authp->id, authp->physical);
  }
}

void
auth_Init(struct authinfo *authinfo)
{
  memset(authinfo, '\0', sizeof(struct authinfo));
  authinfo->cfg.fsmretry = DEF_FSMRETRY;
}

void
auth_StartChallenge(struct authinfo *authp, struct physical *physical,
                   void (*fn)(struct authinfo *, int, struct physical *))
{
  authp->ChallengeFunc = fn;
  authp->physical = physical;
  timer_Stop(&authp->authtimer);
  authp->authtimer.func = AuthTimeout;
  authp->authtimer.name = "auth";
  authp->authtimer.load = authp->cfg.fsmretry * SECTICKS;
  authp->authtimer.arg = (void *) authp;
  authp->retry = 3;
  authp->id = 1;
  (*authp->ChallengeFunc)(authp, authp->id, physical);
  timer_Start(&authp->authtimer);
}

void
auth_StopTimer(struct authinfo *authp)
{
  timer_Stop(&authp->authtimer);
  authp->physical = NULL;
}

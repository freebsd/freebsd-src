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
 * $FreeBSD: src/usr.sbin/ppp/auth.c,v 1.50 1999/12/27 11:43:30 brian Exp $
 *
 *	TODO:
 *		o Implement check against with registered IP addresses.
 */
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "mbuf.h"
#include "defs.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "lqr.h"
#include "hdlc.h"
#include "ipcp.h"
#include "auth.h"
#include "systems.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "chat.h"
#include "proto.h"
#include "filter.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "cbcp.h"
#include "chap.h"
#include "async.h"
#include "physical.h"
#include "datalink.h"
#include "bundle.h"

const char *
Auth2Nam(u_short auth, u_char type)
{
  static char chap[10];

  switch (auth) {
  case PROTO_PAP:
    return "PAP";
  case PROTO_CHAP:
    snprintf(chap, sizeof chap, "CHAP 0x%02x", type);
    return chap;
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
auth_SetPhoneList(const char *name, char *phone, int phonelen)
{
  FILE *fp;
  int n, lineno;
  char *vector[6];
  char buff[LINE_LEN];

  fp = OpenSecret(SECRETFILE);
  lineno = 0;
  if (fp != NULL) {
    while (fgets(buff, sizeof buff, fp)) {
      lineno++;
      if (buff[0] == '#')
        continue;
      buff[strlen(buff) - 1] = '\0';
      memset(vector, '\0', sizeof vector);
      if ((n = MakeArgs(buff, vector, VECSIZE(vector), PARSE_REDUCE)) < 0)
        log_Printf(LogWARN, "%s: %d: Invalid line\n", SECRETFILE, lineno);
      if (n < 5)
        continue;
      if (strcmp(vector[0], name) == 0) {
        CloseSecret(fp);
        if (*vector[4] == '\0')
          return 0;
        strncpy(phone, vector[4], phonelen - 1);
        phone[phonelen - 1] = '\0';
        return 1;		/* Valid */
      }
    }
    CloseSecret(fp);
  }
  *phone = '\0';
  return 0;
}

int
auth_Select(struct bundle *bundle, const char *name)
{
  FILE *fp;
  int n, lineno;
  char *vector[5];
  char buff[LINE_LEN];

  if (*name == '\0') {
    ipcp_Setup(&bundle->ncp.ipcp, INADDR_NONE);
    return 1;
  }

#ifndef NORADIUS
  if (bundle->radius.valid && bundle->radius.ip.s_addr != INADDR_NONE) {
    /* We've got a radius IP - it overrides everything */
    if (!ipcp_UseHisIPaddr(bundle, bundle->radius.ip))
      return 0;
    ipcp_Setup(&bundle->ncp.ipcp, bundle->radius.mask.s_addr);
    /* Continue with ppp.secret in case we've got a new label */
  }
#endif

  fp = OpenSecret(SECRETFILE);
  lineno = 0;
  if (fp != NULL) {
    while (fgets(buff, sizeof buff, fp)) {
      lineno++;
      if (buff[0] == '#')
        continue;
      buff[strlen(buff) - 1] = '\0';
      memset(vector, '\0', sizeof vector);
      if ((n = MakeArgs(buff, vector, VECSIZE(vector), PARSE_REDUCE)) < 0)
        log_Printf(LogWARN, "%s: %d: Invalid line\n", SECRETFILE, lineno);
      if (n < 2)
        continue;
      if (strcmp(vector[0], name) == 0) {
        CloseSecret(fp);
#ifndef NORADIUS
        if (!bundle->radius.valid || bundle->radius.ip.s_addr == INADDR_NONE) {
#endif
          if (n > 2 && *vector[2] && strcmp(vector[2], "*") &&
              !ipcp_UseHisaddr(bundle, vector[2], 1))
            return 0;
          ipcp_Setup(&bundle->ncp.ipcp, INADDR_NONE);
#ifndef NORADIUS
        }
#endif
        if (n > 3 && *vector[3] && strcmp(vector[3], "*"))
          bundle_SetLabel(bundle, vector[3]);
        return 1;		/* Valid */
      }
    }
    CloseSecret(fp);
  }

#ifndef NOPASSWDAUTH
  /* Let 'em in anyway - they must have been in the passwd file */
  ipcp_Setup(&bundle->ncp.ipcp, INADDR_NONE);
  return 1;
#else
#ifndef NORADIUS
  if (bundle->radius.valid)
    return 1;
#endif

  /* Disappeared from ppp.secret ??? */
  return 0;
#endif
}

int
auth_Validate(struct bundle *bundle, const char *name,
             const char *key, struct physical *physical)
{
  /* Used by PAP routines */

  FILE *fp;
  int n, lineno;
  char *vector[5];
  char buff[LINE_LEN];

  fp = OpenSecret(SECRETFILE);
  lineno = 0;
  if (fp != NULL) {
    while (fgets(buff, sizeof buff, fp)) {
      lineno++;
      if (buff[0] == '#')
        continue;
      buff[strlen(buff) - 1] = 0;
      memset(vector, '\0', sizeof vector);
      if ((n = MakeArgs(buff, vector, VECSIZE(vector), PARSE_REDUCE)) < 0)
        log_Printf(LogWARN, "%s: %d: Invalid line\n", SECRETFILE, lineno);
      if (n < 2)
        continue;
      if (strcmp(vector[0], name) == 0) {
        CloseSecret(fp);
        return auth_CheckPasswd(name, vector[1], key);
      }
    }
    CloseSecret(fp);
  }

#ifndef NOPASSWDAUTH
  if (Enabled(bundle, OPT_PASSWDAUTH))
    return auth_CheckPasswd(name, "*", key);
#endif

  return 0;			/* Invalid */
}

char *
auth_GetSecret(struct bundle *bundle, const char *name, int len,
              struct physical *physical)
{
  /* Used by CHAP routines */

  FILE *fp;
  int n, lineno;
  char *vector[5];
  static char buff[LINE_LEN];	/* vector[] will point here when returned */

  fp = OpenSecret(SECRETFILE);
  if (fp == NULL)
    return (NULL);

  lineno = 0;
  while (fgets(buff, sizeof buff, fp)) {
    lineno++;
    if (buff[0] == '#')
      continue;
    n = strlen(buff) - 1;
    if (buff[n] == '\n')
      buff[n] = '\0';	/* Trim the '\n' */
    memset(vector, '\0', sizeof vector);
    if ((n = MakeArgs(buff, vector, VECSIZE(vector), PARSE_REDUCE)) < 0)
      log_Printf(LogWARN, "%s: %d: Invalid line\n", SECRETFILE, lineno);
    if (n < 2)
      continue;
    if (strlen(vector[0]) == len && strncmp(vector[0], name, len) == 0) {
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
    authp->id++;
    (*authp->fn.req)(authp);
    timer_Start(&authp->authtimer);
  } else {
    log_Printf(LogPHASE, "Auth: No response from server\n");
    datalink_AuthNotOk(authp->physical->dl);
  }
}

void
auth_Init(struct authinfo *authp, struct physical *p, auth_func req,
          auth_func success, auth_func failure)
{
  memset(authp, '\0', sizeof(struct authinfo));
  authp->cfg.fsm.timeout = DEF_FSMRETRY;
  authp->cfg.fsm.maxreq = DEF_FSMAUTHTRIES;
  authp->cfg.fsm.maxtrm = 0;	/* not used */
  authp->fn.req = req;
  authp->fn.success = success;
  authp->fn.failure = failure;
  authp->physical = p;
}

void
auth_StartReq(struct authinfo *authp)
{
  timer_Stop(&authp->authtimer);
  authp->authtimer.func = AuthTimeout;
  authp->authtimer.name = "auth";
  authp->authtimer.load = authp->cfg.fsm.timeout * SECTICKS;
  authp->authtimer.arg = (void *)authp;
  authp->retry = authp->cfg.fsm.maxreq;
  authp->id = 1;
  (*authp->fn.req)(authp);
  timer_Start(&authp->authtimer);
}

void
auth_StopTimer(struct authinfo *authp)
{
  timer_Stop(&authp->authtimer);
}

struct mbuf *
auth_ReadHeader(struct authinfo *authp, struct mbuf *bp)
{
  int len;

  len = m_length(bp);
  if (len >= sizeof authp->in.hdr) {
    bp = mbuf_Read(bp, (u_char *)&authp->in.hdr, sizeof authp->in.hdr);
    if (len >= ntohs(authp->in.hdr.length))
      return bp;
    authp->in.hdr.length = htons(0);
    log_Printf(LogWARN, "auth_ReadHeader: Short packet (%d > %d) !\n",
               ntohs(authp->in.hdr.length), len);
  } else {
    authp->in.hdr.length = htons(0);
    log_Printf(LogWARN, "auth_ReadHeader: Short packet header (%d > %d) !\n",
               (int)(sizeof authp->in.hdr), len);
  }

  m_freem(bp);
  return NULL;
}

struct mbuf *
auth_ReadName(struct authinfo *authp, struct mbuf *bp, int len)
{
  if (len > sizeof authp->in.name - 1)
    log_Printf(LogWARN, "auth_ReadName: Name too long (%d) !\n", len);
  else {
    int mlen = m_length(bp);

    if (len > mlen)
      log_Printf(LogWARN, "auth_ReadName: Short packet (%d > %d) !\n",
                 len, mlen);
    else {
      bp = mbuf_Read(bp, (u_char *)authp->in.name, len);
      authp->in.name[len] = '\0';
      return bp;
    }
  }

  *authp->in.name = '\0';
  m_freem(bp);
  return NULL;
}

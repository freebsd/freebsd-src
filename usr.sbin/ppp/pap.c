/*
 *			PPP PAP Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993-94, Internet Initiative Japan, Inc.
 *		     All rights reserverd.
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
 * $Id: pap.c,v 1.16 1997/09/27 19:11:41 brian Exp $
 *
 *	TODO:
 */
#include <time.h>
#include <utmp.h>
#include <pwd.h>
#include "fsm.h"
#include "lcp.h"
#include "pap.h"
#include "loadalias.h"
#include "vars.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "phase.h"
#include "auth.h"
#ifdef __OpenBSD__
#include "util.h"
#else
#include "libutil.h"
#endif

static char *papcodes[] = {
  "???", "REQUEST", "ACK", "NAK"
};

struct authinfo AuthPapInfo = {
  SendPapChallenge,
};

void
SendPapChallenge(int papid)
{
  struct fsmheader lh;
  struct mbuf *bp;
  u_char *cp;
  int namelen, keylen, plen;

  namelen = strlen(VarAuthName);
  keylen = strlen(VarAuthKey);
  plen = namelen + keylen + 2;
  LogPrintf(LogDEBUG, "SendPapChallenge: namelen = %d, keylen = %d\n",
	    namelen, keylen);
  if (LogIsKept(LogDEBUG))
    LogPrintf(LogPHASE, "PAP: %s (%s)\n", VarAuthName, VarAuthKey);
  else
    LogPrintf(LogPHASE, "PAP: %s\n", VarAuthName);
  lh.code = PAP_REQUEST;
  lh.id = papid;
  lh.length = htons(plen + sizeof(struct fsmheader));
  bp = mballoc(plen + sizeof(struct fsmheader), MB_FSM);
  bcopy(&lh, MBUF_CTOP(bp), sizeof(struct fsmheader));
  cp = MBUF_CTOP(bp) + sizeof(struct fsmheader);
  *cp++ = namelen;
  bcopy(VarAuthName, cp, namelen);
  cp += namelen;
  *cp++ = keylen;
  bcopy(VarAuthKey, cp, keylen);

  HdlcOutput(PRI_LINK, PROTO_PAP, bp);
}

static void
SendPapCode(int id, int code, char *message)
{
  struct fsmheader lh;
  struct mbuf *bp;
  u_char *cp;
  int plen, mlen;

  lh.code = code;
  lh.id = id;
  mlen = strlen(message);
  plen = mlen + 1;
  lh.length = htons(plen + sizeof(struct fsmheader));
  bp = mballoc(plen + sizeof(struct fsmheader), MB_FSM);
  bcopy(&lh, MBUF_CTOP(bp), sizeof(struct fsmheader));
  cp = MBUF_CTOP(bp) + sizeof(struct fsmheader);
  *cp++ = mlen;
  bcopy(message, cp, mlen);
  LogPrintf(LogPHASE, "PapOutput: %s\n", papcodes[code]);
  HdlcOutput(PRI_LINK, PROTO_PAP, bp);
}

/*
 * Validate given username and passwrd against with secret table
 */
static int
PapValidate(u_char * name, u_char * key)
{
  int nlen, klen;

  nlen = *name++;
  klen = *key;
  *key++ = 0;
  key[klen] = 0;
  LogPrintf(LogDEBUG, "PapValidate: name %s (%d), key %s (%d)\n",
	    name, nlen, key, klen);

#ifndef NOPASSWDAUTH
  if (Enabled(ConfPasswdAuth)) {
    struct passwd *pwd;
    int result;

    LogPrintf(LogLCP, "Using PasswdAuth\n");
    result = (pwd = getpwnam(name)) &&
             !strcmp(crypt(key, pwd->pw_passwd), pwd->pw_passwd);
    endpwent();
    return result;
  }
#endif

  return (AuthValidate(SECRETFILE, name, key));
}

void
PapInput(struct mbuf * bp)
{
  int len = plength(bp);
  struct fsmheader *php;
  struct lcpstate *lcp = &LcpInfo;
  u_char *cp;

  if (len >= sizeof(struct fsmheader)) {
    php = (struct fsmheader *) MBUF_CTOP(bp);
    if (len >= ntohs(php->length)) {
      if (php->code < PAP_REQUEST || php->code > PAP_NAK)
	php->code = 0;
      LogPrintf(LogPHASE, "PapInput: %s\n", papcodes[php->code]);

      switch (php->code) {
      case PAP_REQUEST:
	cp = (u_char *) (php + 1);
	if (PapValidate(cp, cp + *cp + 1)) {
	  SendPapCode(php->id, PAP_ACK, "Greetings!!");
	  lcp->auth_ineed = 0;
	  if (lcp->auth_iwait == 0) {
	    if ((mode & MODE_DIRECT) && isatty(modem) && Enabled(ConfUtmp))
	      if (Utmp)
		LogPrintf(LogERROR, "Oops, already logged in on %s\n",
			  VarBaseDevice);
	      else {
	        struct utmp ut;
	        memset(&ut, 0, sizeof(ut));
	        time(&ut.ut_time);
	        strncpy(ut.ut_name, cp+1, sizeof(ut.ut_name)-1);
	        strncpy(ut.ut_line, VarBaseDevice, sizeof(ut.ut_line)-1);
	        if (logout(ut.ut_line))
		  logwtmp(ut.ut_line, "", "");
	        login(&ut);
	        Utmp = 1;
	      }
	    NewPhase(PHASE_NETWORK);
	  }
	} else {
	  SendPapCode(php->id, PAP_NAK, "Login incorrect");
	  reconnect(RECON_FALSE);
	  LcpClose();
	}
	break;
      case PAP_ACK:
	StopAuthTimer(&AuthPapInfo);
	cp = (u_char *) (php + 1);
	len = *cp++;
	cp[len] = 0;
	LogPrintf(LogPHASE, "Received PAP_ACK (%s)\n", cp);
	if (lcp->auth_iwait == PROTO_PAP) {
	  lcp->auth_iwait = 0;
	  if (lcp->auth_ineed == 0)
	    NewPhase(PHASE_NETWORK);
	}
	break;
      case PAP_NAK:
	StopAuthTimer(&AuthPapInfo);
	cp = (u_char *) (php + 1);
	len = *cp++;
	cp[len] = 0;
	LogPrintf(LogPHASE, "Received PAP_NAK (%s)\n", cp);
	reconnect(RECON_FALSE);
	LcpClose();
	break;
      }
    }
  }
  pfree(bp);
}

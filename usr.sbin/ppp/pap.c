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
 * $Id: pap.c,v 1.20.2.16 1998/03/13 21:07:14 brian Exp $
 *
 *	TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#ifdef __OpenBSD__
#include <util.h>
#else
#include <libutil.h>
#endif

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "auth.h"
#include "pap.h"
#include "loadalias.h"
#include "vars.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "async.h"
#include "throughput.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "iplist.h"
#include "ipcp.h"
#include "bundle.h"
#include "chat.h"
#include "ccp.h"
#include "chap.h"
#include "datalink.h"

static const char *papcodes[] = { "???", "REQUEST", "ACK", "NAK" };

void
SendPapChallenge(struct authinfo *auth, int papid, struct physical *physical)
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
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  cp = MBUF_CTOP(bp) + sizeof(struct fsmheader);
  *cp++ = namelen;
  memcpy(cp, VarAuthName, namelen);
  cp += namelen;
  *cp++ = keylen;
  memcpy(cp, VarAuthKey, keylen);

  HdlcOutput(physical2link(physical), PRI_LINK, PROTO_PAP, bp);
}

static void
SendPapCode(int id, int code, const char *message, struct physical *physical)
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
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  cp = MBUF_CTOP(bp) + sizeof(struct fsmheader);
  *cp++ = mlen;
  memcpy(cp, message, mlen);
  LogPrintf(LogPHASE, "PapOutput: %s\n", papcodes[code]);
  HdlcOutput(physical2link(physical), PRI_LINK, PROTO_PAP, bp);
}

/*
 * Validate given username and passwrd against with secret table
 */
static int
PapValidate(struct bundle *bundle, u_char *name, u_char *key,
            struct physical *physical)
{
  int nlen, klen;

  nlen = *name++;
  klen = *key;
  *key++ = 0;
  key[klen] = 0;
  LogPrintf(LogDEBUG, "PapValidate: name %s (%d), key %s (%d)\n",
	    name, nlen, key, klen);

  return AuthValidate(bundle, SECRETFILE, name, key, physical);
}

void
PapInput(struct bundle *bundle, struct mbuf *bp, struct physical *physical)
{
  struct datalink *dl = bundle2datalink(bundle, physical->link.name);
  int len = plength(bp);
  struct fsmheader *php;
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
	if (PapValidate(bundle, cp, cp + *cp + 1, physical)) {
	  SendPapCode(php->id, PAP_ACK, "Greetings!!", physical);
	  dl->lcp.auth_ineed = 0;
          Physical_Login(physical, cp + 1);

          if (dl->lcp.auth_iwait == 0)
            /*
             * Either I didn't need to authenticate, or I've already been
             * told that I got the answer right.
             */
            datalink_AuthOk(dl);

	} else {
	  SendPapCode(php->id, PAP_NAK, "Login incorrect", physical);
          datalink_AuthNotOk(dl);
	}
	break;
      case PAP_ACK:
	StopAuthTimer(&dl->pap);
	cp = (u_char *) (php + 1);
	len = *cp++;
	cp[len] = 0;
	LogPrintf(LogPHASE, "Received PAP_ACK (%s)\n", cp);
	if (dl->lcp.auth_iwait == PROTO_PAP) {
	  dl->lcp.auth_iwait = 0;
	  if (dl->lcp.auth_ineed == 0)
            /*
             * We've succeeded in our ``login''
             * If we're not expecting  the peer to authenticate (or he already
             * has), proceed to network phase.
             */
            datalink_AuthOk(dl);
	}
	break;
      case PAP_NAK:
	StopAuthTimer(&dl->pap);
	cp = (u_char *) (php + 1);
	len = *cp++;
	cp[len] = 0;
	LogPrintf(LogPHASE, "Received PAP_NAK (%s)\n", cp);
        datalink_AuthNotOk(dl);
	break;
      }
    }
  }
  pfree(bp);
}

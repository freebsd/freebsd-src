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
 * $Id: pap.c,v 1.4 1996/01/30 11:08:45 dfr Exp $
 *
 *	TODO:
 */
#include "fsm.h"
#include "lcp.h"
#include "pap.h"
#include "vars.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "phase.h"
#include "auth.h"

static char *papcodes[] = {
  "???", "REQUEST", "ACK", "NAK"
};

struct authinfo AuthPapInfo  = {
  SendPapChallenge,
};

void
SendPapChallenge(papid)
int papid;
{
  struct fsmheader lh;
  struct mbuf *bp;
  u_char *cp;
  int namelen, keylen, plen;

  namelen = strlen(VarAuthName);
  keylen = strlen(VarAuthKey);
  plen = namelen + keylen + 2;
#ifdef DEBUG
  logprintf("namelen = %d, keylen = %d\n", namelen, keylen);
#endif
  LogPrintf(LOG_PHASE_BIT, "PAP: %s (%s)\n", VarAuthName, VarAuthKey);
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
SendPapCode(id, code, message)
int id;
char *message;
int code;
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
  LogPrintf(LOG_PHASE_BIT, "PapOutput: %s\n", papcodes[code]);
  HdlcOutput(PRI_LINK, PROTO_PAP, bp);
}

/*
 * Validate given username and passwrd against with secret table
 */
static int
PapValidate(name, key)
u_char *name, *key;
{
  int nlen, klen;

  nlen = *name++;
  klen = *key;
  *key++ = 0;
  key[klen] = 0;
#ifdef DEBUG
  logprintf("name: %s (%d), key: %s (%d)\n", name, nlen, key, klen);
#endif
  return(AuthValidate(SECRETFILE, name, key));
}

void
PapInput(bp)
struct mbuf *bp;
{
  int len = plength(bp);
  struct fsmheader *php;
  struct lcpstate *lcp = &LcpInfo;
  u_char *cp;

  if (len >= sizeof(struct fsmheader)) {
    php = (struct fsmheader *)MBUF_CTOP(bp);
    if (len >= ntohs(php->length)) {
      if (php->code < PAP_REQUEST || php->code > PAP_NAK)
	php->code = 0;
      LogPrintf(LOG_PHASE_BIT, "PapInput: %s\n", papcodes[php->code]);

      switch (php->code) {
      case PAP_REQUEST:
	cp = (u_char *) (php + 1);
	if (PapValidate(cp, cp + *cp + 1)) {
	  SendPapCode(php->id, PAP_ACK, "Greetings!!");
	  lcp->auth_ineed = 0;
	  if (lcp->auth_iwait == 0)
	    NewPhase(PHASE_NETWORK);
	} else {
	  SendPapCode(php->id, PAP_NAK, "Login incorrect");
	  LcpClose();
	}
	break;
      case PAP_ACK:
	StopAuthTimer(&AuthPapInfo);
	cp = (u_char *)(php + 1);
	len = *cp++;
	cp[len] = 0;
	LogPrintf(LOG_PHASE_BIT, "Received PAP_ACK (%s)\n", cp);
	if (lcp->auth_iwait == PROTO_PAP) {
	  lcp->auth_iwait = 0;
	  if (lcp->auth_ineed == 0)
	    NewPhase(PHASE_NETWORK);
	}
	break;
      case PAP_NAK:
	StopAuthTimer(&AuthPapInfo);
	cp = (u_char *)(php + 1);
	len = *cp++;
	cp[len] = 0;
	LogPrintf(LOG_PHASE_BIT, "Received PAP_NAK (%s)\n", cp);
	LcpClose();
	break;
      }
    }
  }
  pfree(bp);
}

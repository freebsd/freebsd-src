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
 * $Id: pap.c,v 1.23 1998/05/21 21:47:18 brian Exp $
 *
 *	TODO:
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <string.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "auth.h"
#include "pap.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "async.h"
#include "throughput.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "mp.h"
#include "bundle.h"
#include "chat.h"
#include "chap.h"
#include "datalink.h"

static const char *papcodes[] = { "???", "REQUEST", "ACK", "NAK" };

void
pap_SendChallenge(struct authinfo *auth, int papid, struct physical *physical)
{
  struct fsmheader lh;
  struct mbuf *bp;
  u_char *cp;
  int namelen, keylen, plen;

  namelen = strlen(physical->dl->bundle->cfg.auth.name);
  keylen = strlen(physical->dl->bundle->cfg.auth.key);
  plen = namelen + keylen + 2;
  log_Printf(LogDEBUG, "pap_SendChallenge: namelen = %d, keylen = %d\n",
	    namelen, keylen);
  log_Printf(LogPHASE, "PAP: %s\n", physical->dl->bundle->cfg.auth.name);
  lh.code = PAP_REQUEST;
  lh.id = papid;
  lh.length = htons(plen + sizeof(struct fsmheader));
  bp = mbuf_Alloc(plen + sizeof(struct fsmheader), MB_FSM);
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  cp = MBUF_CTOP(bp) + sizeof(struct fsmheader);
  *cp++ = namelen;
  memcpy(cp, physical->dl->bundle->cfg.auth.name, namelen);
  cp += namelen;
  *cp++ = keylen;
  memcpy(cp, physical->dl->bundle->cfg.auth.key, keylen);

  hdlc_Output(&physical->link, PRI_LINK, PROTO_PAP, bp);
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
  bp = mbuf_Alloc(plen + sizeof(struct fsmheader), MB_FSM);
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  cp = MBUF_CTOP(bp) + sizeof(struct fsmheader);
  *cp++ = mlen;
  memcpy(cp, message, mlen);
  log_Printf(LogPHASE, "PapOutput: %s\n", papcodes[code]);
  hdlc_Output(&physical->link, PRI_LINK, PROTO_PAP, bp);
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
  log_Printf(LogDEBUG, "PapValidate: name %s (%d), key %s (%d)\n",
	    name, nlen, key, klen);

  return auth_Validate(bundle, name, key, physical);
}

void
pap_Input(struct bundle *bundle, struct mbuf *bp, struct physical *physical)
{
  int len = mbuf_Length(bp);
  struct fsmheader *php;
  u_char *cp;

  if (len >= sizeof(struct fsmheader)) {
    php = (struct fsmheader *) MBUF_CTOP(bp);
    if (len >= ntohs(php->length)) {
      if (php->code < PAP_REQUEST || php->code > PAP_NAK)
	php->code = 0;
      log_Printf(LogPHASE, "pap_Input: %s\n", papcodes[php->code]);

      switch (php->code) {
      case PAP_REQUEST:
	cp = (u_char *) (php + 1);
	if (PapValidate(bundle, cp, cp + *cp + 1, physical)) {
          datalink_GotAuthname(physical->dl, cp+1, *cp);
	  SendPapCode(php->id, PAP_ACK, "Greetings!!", physical);
	  physical->link.lcp.auth_ineed = 0;
          if (Enabled(bundle, OPT_UTMP))
            physical_Login(physical, cp + 1);

          if (physical->link.lcp.auth_iwait == 0)
            /*
             * Either I didn't need to authenticate, or I've already been
             * told that I got the answer right.
             */
            datalink_AuthOk(physical->dl);

	} else {
	  SendPapCode(php->id, PAP_NAK, "Login incorrect", physical);
          datalink_AuthNotOk(physical->dl);
	}
	break;
      case PAP_ACK:
	auth_StopTimer(&physical->dl->pap);
	cp = (u_char *) (php + 1);
	len = *cp++;
	cp[len] = 0;
	log_Printf(LogPHASE, "Received PAP_ACK (%s)\n", cp);
	if (physical->link.lcp.auth_iwait == PROTO_PAP) {
	  physical->link.lcp.auth_iwait = 0;
	  if (physical->link.lcp.auth_ineed == 0)
            /*
             * We've succeeded in our ``login''
             * If we're not expecting  the peer to authenticate (or he already
             * has), proceed to network phase.
             */
            datalink_AuthOk(physical->dl);
	}
	break;
      case PAP_NAK:
	auth_StopTimer(&physical->dl->pap);
	cp = (u_char *) (php + 1);
	len = *cp++;
	cp[len] = 0;
	log_Printf(LogPHASE, "Received PAP_NAK (%s)\n", cp);
        datalink_AuthNotOk(physical->dl);
	break;
      }
    }
  }
  mbuf_Free(bp);
}

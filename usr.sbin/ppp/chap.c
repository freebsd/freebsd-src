/*
 *			PPP CHAP Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
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
 * $Id: chap.c,v 1.7.2.4 1997/05/19 02:02:15 brian Exp $
 *
 *	TODO:
 */
#include <sys/types.h>
#include <time.h>
#include "fsm.h"
#include "chap.h"
#include "lcpproto.h"
#include "lcp.h"
#include "hdlc.h"
#include "phase.h"
#include "vars.h"
#include "auth.h"

static char *chapcodes[] = {
  "???", "CHALLENGE", "RESPONSE", "SUCCESS", "FAILURE"
};

struct authinfo AuthChapInfo  = {
  SendChapChallenge,
};

extern char *AuthGetSecret();

void
ChapOutput(code, id, ptr, count)
u_int code, id;
u_char *ptr;
int count;
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  plen =  sizeof(struct fsmheader) + count;
  lh.code = code;
  lh.id = id;
  lh.length = htons(plen);
  bp = mballoc(plen, MB_FSM);
  bcopy(&lh, MBUF_CTOP(bp), sizeof(struct fsmheader));
  if (count)
    bcopy(ptr, MBUF_CTOP(bp) + sizeof(struct fsmheader), count);
#ifdef DEBUG
  DumpBp(bp);
#endif
  LogPrintf(LOG_LCP_BIT, "ChapOutput: %s\n", chapcodes[code]);
  HdlcOutput(PRI_LINK, PROTO_CHAP, bp);
}


static char challenge_data[80];
static int  challenge_len;

void
SendChapChallenge(chapid)
int chapid;
{
  int len, i;
  char *cp;

  srandom(time(NULL));

  cp = challenge_data;
  *cp++ = challenge_len = random() % 32 + 16;
  for (i = 0; i < challenge_len; i++)
    *cp++ = random() & 0xff;
  len = strlen(VarAuthName);
  bcopy(VarAuthName, cp, len);
  cp += len;
  ChapOutput(CHAP_CHALLENGE, chapid, challenge_data, cp - challenge_data);
}

#ifdef DEBUG
void
DumpDigest(mes, cp, len)
char *mes;
char *cp;
int len;
{
  int i;

  logprintf("%s: ", mes);
  for (i = 0; i < len; i++) {
    logprintf(" %02x", *cp++ & 0xff);
  }
  logprintf("\n");
}
#endif

void
RecvChapTalk(chp, bp)
struct fsmheader *chp;
struct mbuf *bp;
{
  int valsize, len;
  int arglen, keylen, namelen;
  char *cp, *argp, *ap, *name, *digest;
  char *keyp;
  MD5_CTX context;                            /* context */
  char answer[100];
  char cdigest[16];

  len = ntohs(chp->length);
#ifdef DEBUG
  logprintf("length: %d\n", len);
#endif
  arglen = len - sizeof(struct fsmheader);
  cp = (char *)MBUF_CTOP(bp);
  valsize = *cp++ & 255;
  name = cp + valsize;
  namelen = arglen - valsize - 1;
  name[namelen] = 0;
  LogPrintf(LOG_PHASE_BIT, " Valsize = %d, Name = %s\n", valsize, name);

  /*
   * Get a secret key corresponds to the peer
   */
  keyp = AuthGetSecret(SECRETFILE, name, namelen, chp->code == CHAP_RESPONSE);

  switch (chp->code) {
  case CHAP_CHALLENGE:
    if (keyp) {
      keylen = strlen(keyp);
    } else {
      keylen = strlen(VarAuthKey);
      keyp = VarAuthKey;
    }
    name = VarAuthName;
    namelen = strlen(VarAuthName);
    argp = malloc(1 + valsize + namelen + 16);
    if (argp == NULL) {
      ChapOutput(CHAP_FAILURE, chp->id, "Out of memory!", 14);
      return;
    }
    digest = argp;
    *digest++ = 16;		/* value size */
    ap = answer;
    *ap++ = chp->id;
    bcopy(keyp, ap, keylen);
    ap += keylen;
    bcopy(cp, ap, valsize);
#ifdef DEBUG
    DumpDigest("recv", ap, valsize);
#endif
    ap += valsize;
    MD5Init(&context);
    MD5Update(&context, answer, ap - answer);
    MD5Final(digest, &context);
#ifdef DEBUG
    DumpDigest("answer", digest, 16);
#endif
    bcopy(name, digest + 16, namelen);
    ap += namelen;
    /* Send answer to the peer */
    ChapOutput(CHAP_RESPONSE, chp->id, argp, namelen + 17);
    free(argp);
    break;
  case CHAP_RESPONSE:
    if (keyp) {
      /*
       * Compute correct digest value
       */
      keylen = strlen(keyp);
      ap = answer;
      *ap++ = chp->id;
      bcopy(keyp, ap, keylen);
      ap += keylen;
      MD5Init(&context);
      MD5Update(&context, answer, ap - answer);
      MD5Update(&context, challenge_data+1, challenge_len);
      MD5Final(cdigest, &context);
#ifdef DEBUG
      DumpDigest("got", cp, 16);
      DumpDigest("expect", cdigest, 16);
#endif
      /*
       * Compare with the response
       */
      if (bcmp(cp, cdigest, 16) == 0) {
	ChapOutput(CHAP_SUCCESS, chp->id, "Wellcome!!", 10);
	NewPhase(PHASE_NETWORK);
	break;
      }
    }
    /*
     * Peer is not registerd, or response digest is wrong.
     */
    ChapOutput(CHAP_FAILURE, chp->id, "Invalid!!", 9);
    reconnect(RECON_FALSE);
    LcpClose();
    break;
  }
}

void
RecvChapResult(chp, bp)
struct fsmheader *chp;
struct mbuf *bp;
{
  int len;
  struct lcpstate *lcp = &LcpInfo;

  len = ntohs(chp->length);
#ifdef DEBUG
  logprintf("length: %d\n", len);
#endif
  if (chp->code == CHAP_SUCCESS) {
    if (lcp->auth_iwait == PROTO_CHAP) {
      lcp->auth_iwait = 0;
      if (lcp->auth_ineed == 0)
	NewPhase(PHASE_NETWORK);
    }
  } else {
    /*
     * Maybe, we shoud close LCP. Of cause, peer may take close action, too.
     */
    ;
  }
}

void
ChapInput(struct mbuf *bp)
{
  int len = plength(bp);
  struct fsmheader *chp;

  if (len >= sizeof(struct fsmheader)) {
    chp = (struct fsmheader *)MBUF_CTOP(bp);
    if (len >= ntohs(chp->length)) {
      if (chp->code < 1 || chp->code > 4)
	chp->code = 0;
      LogPrintf(LOG_LCP_BIT, "ChapInput: %s\n", chapcodes[chp->code]);

      bp->offset += sizeof(struct fsmheader);
      bp->cnt -= sizeof(struct fsmheader);

      switch (chp->code) {
      case CHAP_RESPONSE:
	StopAuthTimer(&AuthChapInfo);
	/* Fall into.. */
      case CHAP_CHALLENGE:
	RecvChapTalk(chp, bp);
	break;
      case CHAP_SUCCESS:
      case CHAP_FAILURE:
	RecvChapResult(chp, bp);
	break;
      }
    }
  }
  pfree(bp);
}

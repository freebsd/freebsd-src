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
 * $Id: chap.c,v 1.23 1997/09/25 00:52:32 brian Exp $
 *
 *	TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <ctype.h>
#ifdef HAVE_DES
#include <md4.h>
#endif
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef __OpenBSD__
#include <util.h>
#else
#include <libutil.h>
#endif
#include <utmp.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "chap.h"
#include "chap_ms.h"
#include "lcpproto.h"
#include "lcp.h"
#include "hdlc.h"
#include "phase.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "auth.h"

static char *chapcodes[] = {
  "???", "CHALLENGE", "RESPONSE", "SUCCESS", "FAILURE"
};

static void
ChapOutput(u_int code, u_int id, u_char * ptr, int count)
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  plen = sizeof(struct fsmheader) + count;
  lh.code = code;
  lh.id = id;
  lh.length = htons(plen);
  bp = mballoc(plen, MB_FSM);
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  if (count)
    memcpy(MBUF_CTOP(bp) + sizeof(struct fsmheader), ptr, count);
  LogDumpBp(LogDEBUG, "ChapOutput", bp);
  LogPrintf(LogLCP, "ChapOutput: %s\n", chapcodes[code]);
  HdlcOutput(PRI_LINK, PROTO_CHAP, bp);
}


static char challenge_data[80];
static int challenge_len;

static void
SendChapChallenge(int chapid)
{
  int len, i;
  char *cp;

  randinit();
  cp = challenge_data;
  *cp++ = challenge_len = random() % 32 + 16;
  for (i = 0; i < challenge_len; i++)
    *cp++ = random() & 0xff;
  len = strlen(VarAuthName);
  memcpy(cp, VarAuthName, len);
  cp += len;
  ChapOutput(CHAP_CHALLENGE, chapid, challenge_data, cp - challenge_data);
}

struct authinfo AuthChapInfo = {
  SendChapChallenge,
};

static void
RecvChapTalk(struct fsmheader *chp, struct mbuf *bp)
{
  int valsize, len;
  int arglen, keylen, namelen;
  char *cp, *argp, *ap, *name, *digest;
  char *keyp;
  MD5_CTX MD5context;		/* context for MD5 */
  char answer[100];
  char cdigest[16];
#ifdef HAVE_DES
  int ix;
  MD4_CTX MD4context;		/* context for MD4 */
#endif

  len = ntohs(chp->length);
  LogPrintf(LogDEBUG, "RecvChapTalk: length: %d\n", len);
  arglen = len - sizeof(struct fsmheader);
  cp = (char *) MBUF_CTOP(bp);
  valsize = *cp++ & 255;
  name = cp + valsize;
  namelen = arglen - valsize - 1;
  name[namelen] = 0;
  LogPrintf(LogPHASE, " Valsize = %d, Name = %s\n", valsize, name);

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

#ifdef HAVE_DES
    if (VarMSChap)
      argp = malloc(1 + namelen + MS_CHAP_RESPONSE_LEN);
    else
#endif
      argp = malloc(1 + valsize + namelen + 16);

    if (argp == NULL) {
      ChapOutput(CHAP_FAILURE, chp->id, "Out of memory!", 14);
      return;
    }
#ifdef HAVE_DES
    if (VarMSChap) {
      digest = argp;     /* this is the response */
      *digest++ = MS_CHAP_RESPONSE_LEN;   /* 49 */
      memset(digest, '\0', 24);
      digest += 24;

      ap = answer;       /* this is the challenge */
      memcpy(ap, keyp, keylen);
      ap += 2 * keylen;
      memcpy(ap, cp, valsize);
      LogDumpBuff(LogDEBUG, "recv", ap, valsize);
      ap += valsize;
      for (ix = keylen; ix > 0 ; ix--) {
          answer[2*ix-2] = answer[ix-1];
          answer[2*ix-1] = 0;
      }
      MD4Init(&MD4context);
      MD4Update(&MD4context, answer, 2 * keylen);
      MD4Final(digest, &MD4context);
      memcpy(digest + 25, name, namelen);
      ap += 2 * keylen;
      ChapMS(digest, answer + 2 * keylen, valsize);
      LogDumpBuff(LogDEBUG, "answer", digest, 24);
      ChapOutput(CHAP_RESPONSE, chp->id, argp, namelen + MS_CHAP_RESPONSE_LEN + 1);
    } else {
#endif
      digest = argp;
      *digest++ = 16;		/* value size */
      ap = answer;
      *ap++ = chp->id;
      memcpy(ap, keyp, keylen);
      ap += keylen;
      memcpy(ap, cp, valsize);
      LogDumpBuff(LogDEBUG, "recv", ap, valsize);
      ap += valsize;
      MD5Init(&MD5context);
      MD5Update(&MD5context, answer, ap - answer);
      MD5Final(digest, &MD5context);
      LogDumpBuff(LogDEBUG, "answer", digest, 16);
      memcpy(digest + 16, name, namelen);
      ap += namelen;
      /* Send answer to the peer */
      ChapOutput(CHAP_RESPONSE, chp->id, argp, namelen + 17);
#ifdef HAVE_DES
    }
#endif
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
      memcpy(ap, keyp, keylen);
      ap += keylen;
      MD5Init(&MD5context);
      MD5Update(&MD5context, answer, ap - answer);
      MD5Update(&MD5context, challenge_data + 1, challenge_len);
      MD5Final(cdigest, &MD5context);
      LogDumpBuff(LogDEBUG, "got", cp, 16);
      LogDumpBuff(LogDEBUG, "expect", cdigest, 16);

      /*
       * Compare with the response
       */
      if (memcmp(cp, cdigest, 16) == 0) {
	ChapOutput(CHAP_SUCCESS, chp->id, "Welcome!!", 10);
        if ((mode & MODE_DIRECT) && isatty(modem) && Enabled(ConfUtmp))
	  if (Utmp)
	    LogPrintf(LogERROR, "Oops, already logged in on %s\n",
		      VarBaseDevice);
	  else {
	    struct utmp ut;
	    memset(&ut, 0, sizeof(ut));
	    time(&ut.ut_time);
	    strncpy(ut.ut_name, name, sizeof(ut.ut_name)-1);
	    strncpy(ut.ut_line, VarBaseDevice, sizeof(ut.ut_line)-1);
	    if (logout(ut.ut_line))
	      logwtmp(ut.ut_line, "", "");
	    login(&ut);
	    Utmp = 1;
	  }
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

static void
RecvChapResult(struct fsmheader *chp, struct mbuf *bp)
{
  int len;
  struct lcpstate *lcp = &LcpInfo;

  len = ntohs(chp->length);
  LogPrintf(LogDEBUG, "RecvChapResult: length: %d\n", len);
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
    chp = (struct fsmheader *) MBUF_CTOP(bp);
    if (len >= ntohs(chp->length)) {
      if (chp->code < 1 || chp->code > 4)
	chp->code = 0;
      LogPrintf(LogLCP, "ChapInput: %s\n", chapcodes[chp->code]);

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

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
 * $Id: chap.c,v 1.41 1999/02/07 13:48:38 brian Exp $
 *
 *	TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#ifdef HAVE_DES
#include <md4.h>
#include <string.h>
#endif
#include <md5.h>
#include <stdlib.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "lcp.h"
#include "lqr.h"
#include "hdlc.h"
#include "auth.h"
#include "chap.h"
#include "async.h"
#include "throughput.h"
#include "descriptor.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "chat.h"
#include "cbcp.h"
#include "datalink.h"
#ifdef HAVE_DES
#include "chap_ms.h"
#endif

static const char *chapcodes[] = {
  "???", "CHALLENGE", "RESPONSE", "SUCCESS", "FAILURE"
};
#define MAXCHAPCODE (sizeof chapcodes / sizeof chapcodes[0] - 1)

static void
ChapOutput(struct physical *physical, u_int code, u_int id,
	   const u_char *ptr, int count, const char *text)
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  plen = sizeof(struct fsmheader) + count;
  lh.code = code;
  lh.id = id;
  lh.length = htons(plen);
  bp = mbuf_Alloc(plen, MB_FSM);
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  if (count)
    memcpy(MBUF_CTOP(bp) + sizeof(struct fsmheader), ptr, count);
  log_DumpBp(LogDEBUG, "ChapOutput", bp);
  if (text == NULL)
    log_Printf(LogPHASE, "Chap Output: %s\n", chapcodes[code]);
  else
    log_Printf(LogPHASE, "Chap Output: %s (%s)\n", chapcodes[code], text);
  hdlc_Output(&physical->link, PRI_LINK, PROTO_CHAP, bp);
}

static char *
chap_BuildAnswer(char *name, char *key, u_char id, char *challenge, int MSChap)
{
  char *result, *digest;
  size_t nlen, klen;

  nlen = strlen(name);
  klen = strlen(key);

#ifdef HAVE_DES
  if (MSChap) {
    char expkey[AUTHLEN << 2];
    MD4_CTX MD4context;
    int f;

    if ((result = malloc(1 + nlen + MS_CHAP_RESPONSE_LEN)) == NULL)
      return result;

    digest = result;				/* this is the response */
    *digest++ = MS_CHAP_RESPONSE_LEN;		/* 49 */
    memset(digest, '\0', 24);
    digest += 24;

    for (f = klen; f; f--) {
      expkey[2*f-2] = key[f-1];
      expkey[2*f-1] = 0;
    }

    /*
     *           -----------
     * answer = | k\0e\0y\0 |
     *           -----------
     */
    MD4Init(&MD4context);
    MD4Update(&MD4context, expkey, klen << 1);
    MD4Final(digest, &MD4context);
    memcpy(digest + 25, name, nlen);

    /*
     * ``result'' is:
     *           ---- --------- -------------------- ------
     * result = | 49 | 24 * \0 | digest (pad to 25) | name |
     *           ---- --------- -------------------- ------
     */
    chap_MS(digest, challenge + 1, *challenge);

    /*
     *           ---- --------- ---------------- --- ----------
     * result = | 49 | 24 * \0 | 24 byte digest | 1 | authname |
     *           ---- --------- ---------------- --- ----------
     */
  } else
#endif
  if ((result = malloc(nlen + 17)) != NULL) {
    /* Normal MD5 stuff */
    MD5_CTX MD5context;

    digest = result;
    *digest++ = 16;				/* value size */

    MD5Init(&MD5context);
    MD5Update(&MD5context, &id, 1);
    MD5Update(&MD5context, key, klen);
    MD5Update(&MD5context, challenge + 1, *challenge);
    MD5Final(digest, &MD5context);

    memcpy(digest + 16, name, nlen);
    /*
     *           ---- -------- ------
     * result = | 16 | digest | name |
     *           ---- -------- ------
     */
  }

  return result;
}

static void
chap_Challenge(struct authinfo *authp)
{
  struct chap *chap = auth2chap(authp);
  int len, i;
  char *cp;

  randinit();
  cp = chap->challenge;

#ifndef NORADIUS
  if (*authp->physical->dl->bundle->radius.cfg.file) {
    /* For radius, our challenge is 16 readable NUL terminated bytes :*/
    *cp++ = 16;
    for (i = 0; i < 16; i++)
      *cp++ = (random() % 10) + '0';
  } else
#endif
  {
    *cp++ = random() % (CHAPCHALLENGELEN-16) + 16;
    for (i = 0; i < *chap->challenge; i++)
      *cp++ = random() & 0xff;
  }

  len = strlen(authp->physical->dl->bundle->cfg.auth.name);
  memcpy(cp, authp->physical->dl->bundle->cfg.auth.name, len);
  cp += len;
  ChapOutput(authp->physical, CHAP_CHALLENGE, authp->id, chap->challenge,
	     cp - chap->challenge, NULL);
}

static void
chap_Success(struct authinfo *authp)
{
  datalink_GotAuthname(authp->physical->dl, authp->in.name);
  ChapOutput(authp->physical, CHAP_SUCCESS, authp->id, "Welcome!!", 10, NULL);
  authp->physical->link.lcp.auth_ineed = 0;
  if (Enabled(authp->physical->dl->bundle, OPT_UTMP))
    physical_Login(authp->physical, authp->in.name);

  if (authp->physical->link.lcp.auth_iwait == 0)
    /*
     * Either I didn't need to authenticate, or I've already been
     * told that I got the answer right.
     */
    datalink_AuthOk(authp->physical->dl);
}

static void
chap_Failure(struct authinfo *authp)
{
  ChapOutput(authp->physical, CHAP_FAILURE, authp->id, "Invalid!!", 9, NULL);
  datalink_AuthNotOk(authp->physical->dl);
}

void
chap_Init(struct chap *chap, struct physical *p)
{
  auth_Init(&chap->auth, p, chap_Challenge, chap_Success, chap_Failure);
  *chap->challenge = 0;
  chap->using_MSChap = 0;
}

void
chap_Input(struct physical *p, struct mbuf *bp)
{
  struct chap *chap = &p->dl->chap;
  char *name, *key, *ans, *myans;
  int len, nlen;
  u_char alen;

  if ((bp = auth_ReadHeader(&chap->auth, bp)) == NULL)
    log_Printf(LogERROR, "Chap Input: Truncated header !\n");
  else if (chap->auth.in.hdr.code == 0 || chap->auth.in.hdr.code > MAXCHAPCODE)
    log_Printf(LogPHASE, "Chap Input: %d: Bad CHAP code !\n",
               chap->auth.in.hdr.code);
  else {
    len = mbuf_Length(bp);
    ans = NULL;

    if (chap->auth.in.hdr.code != CHAP_CHALLENGE &&
        chap->auth.id != chap->auth.in.hdr.id &&
        Enabled(p->dl->bundle, OPT_IDCHECK)) {
      /* Wrong conversation dude ! */
      log_Printf(LogPHASE, "Chap Input: %s dropped (got id %d, not %d)\n",
                 chapcodes[chap->auth.in.hdr.code], chap->auth.in.hdr.id,
                 chap->auth.id);
      mbuf_Free(bp);
      return;
    }
    chap->auth.id = chap->auth.in.hdr.id;	/* We respond with this id */

    switch (chap->auth.in.hdr.code) {
      case CHAP_CHALLENGE:
        bp = mbuf_Read(bp, chap->challenge, 1);
        len -= *chap->challenge + 1;
        if (len < 0) {
          log_Printf(LogERROR, "Chap Input: Truncated challenge !\n");
          mbuf_Free(bp);
          return;
        }
        bp = mbuf_Read(bp, chap->challenge + 1, *chap->challenge);
        bp = auth_ReadName(&chap->auth, bp, len);
        break;

      case CHAP_RESPONSE:
        auth_StopTimer(&chap->auth);
        bp = mbuf_Read(bp, &alen, 1);
        len -= alen + 1;
        if (len < 0) {
          log_Printf(LogERROR, "Chap Input: Truncated response !\n");
          mbuf_Free(bp);
          return;
        }
        if ((ans = malloc(alen + 2)) == NULL) {
          log_Printf(LogERROR, "Chap Input: Out of memory !\n");
          mbuf_Free(bp);
          return;
        }
        *ans = chap->auth.id;
        bp = mbuf_Read(bp, ans + 1, alen);
        ans[alen+1] = '\0';
        bp = auth_ReadName(&chap->auth, bp, len);
        break;

      case CHAP_SUCCESS:
      case CHAP_FAILURE:
        /* chap->auth.in.name is already set up at CHALLENGE time */
        if ((ans = malloc(len + 1)) == NULL) {
          log_Printf(LogERROR, "Chap Input: Out of memory !\n");
          mbuf_Free(bp);
          return;
        }
        bp = mbuf_Read(bp, ans, len);
        ans[len] = '\0';
        break;
    }

    switch (chap->auth.in.hdr.code) {
      case CHAP_CHALLENGE:
      case CHAP_RESPONSE:
        if (*chap->auth.in.name)
          log_Printf(LogPHASE, "Chap Input: %s (from %s)\n",
                     chapcodes[chap->auth.in.hdr.code], chap->auth.in.name);
        else
          log_Printf(LogPHASE, "Chap Input: %s\n",
                     chapcodes[chap->auth.in.hdr.code]);
        break;

      case CHAP_SUCCESS:
      case CHAP_FAILURE:
        if (*ans)
          log_Printf(LogPHASE, "Chap Input: %s (%s)\n",
                     chapcodes[chap->auth.in.hdr.code], ans);
        else
          log_Printf(LogPHASE, "Chap Input: %s\n",
                     chapcodes[chap->auth.in.hdr.code]);
        break;
    }

    switch (chap->auth.in.hdr.code) {
      case CHAP_CHALLENGE:
        name = p->dl->bundle->cfg.auth.name;
        nlen = strlen(name);
        key = p->dl->bundle->cfg.auth.key;
        myans = chap_BuildAnswer(name, key, chap->auth.id, chap->challenge, 0);
        if (myans) {
          ChapOutput(p, CHAP_RESPONSE, chap->auth.id, myans,
                     *myans + 1 + nlen, name);
          free(myans);
        } else
          ChapOutput(p, CHAP_FAILURE, chap->auth.id, "Out of memory!",
                     14, NULL);
        break;

      case CHAP_RESPONSE:
        name = chap->auth.in.name;
        nlen = strlen(name);
#ifndef NORADIUS
        if (*p->dl->bundle->radius.cfg.file) {
          chap->challenge[*chap->challenge+1] = '\0';
          radius_Authenticate(&p->dl->bundle->radius, &chap->auth,
                              chap->auth.in.name, ans, chap->challenge + 1);
        } else
#endif
        {
          key = auth_GetSecret(p->dl->bundle, name, nlen, p);
          if (key) {
            myans = chap_BuildAnswer(name, key, chap->auth.id, chap->challenge,
                                     chap->using_MSChap);
            if (myans == NULL)
              key = NULL;
            else {
              if (*myans != alen || memcmp(myans + 1, ans + 1, *myans))
                key = NULL;
              free(myans);
            }
          }

          if (key)
            chap_Success(&chap->auth);
          else
            chap_Failure(&chap->auth);
        }

        break;

      case CHAP_SUCCESS:
        if (p->link.lcp.auth_iwait == PROTO_CHAP) {
          p->link.lcp.auth_iwait = 0;
          if (p->link.lcp.auth_ineed == 0)
            /*
             * We've succeeded in our ``login''
             * If we're not expecting  the peer to authenticate (or he already
             * has), proceed to network phase.
             */
            datalink_AuthOk(p->dl);
        }
        break;

      case CHAP_FAILURE:
        datalink_AuthNotOk(p->dl);
        break;
    }
    free(ans);
  }

  mbuf_Free(bp);
}

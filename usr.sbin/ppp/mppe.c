/*-
 * Copyright (c) 2000 Semen Ustimenko <semenu@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sha.h>
#include <openssl/rc4.h>

#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "chap_ms.h"
#include "mppe.h"

/*
 * Documentation:
 *
 * draft-ietf-pppext-mppe-04.txt
 * draft-ietf-pppext-mppe-keys-02.txt
 */

struct mppe_state {
	int	cohnum;
	int	keylen;			/* 8 or 16 bytes */
	int 	keybits;		/* 40, 56 or 128 bits */
	char	sesskey[MPPE_KEY_LEN];
	char	mastkey[MPPE_KEY_LEN];
	RC4_KEY	rc4key;
};

int MPPE_MasterKeyValid = 0;
char MPPE_MasterKey[MPPE_KEY_LEN];

static void
MPPEResetOutput(void *v)
{
  log_Printf(LogCCP, "MPPE: Output channel reset\n");
}

void
MPPEReduceSessionKey(struct mppe_state *mp) {
  switch(mp->keybits) {
  case 40:
    mp->sesskey[2] = 0x9e;
    mp->sesskey[1] = 0x26;
  case 56:
    mp->sesskey[0] = 0xd1;
  case 128:
  }
}

void
MPPEKeyChange(struct mppe_state *mp) {
  char InterimKey[MPPE_KEY_LEN];
  RC4_KEY RC4Key;

  GetNewKeyFromSHA(mp->mastkey, mp->sesskey, mp->keylen, InterimKey);
  RC4_set_key(&RC4Key, mp->keylen, InterimKey);
  RC4(&RC4Key, mp->keylen, InterimKey, mp->sesskey);

  MPPEReduceSessionKey(mp);
}

static struct mbuf *
MPPEOutput(void *v, struct ccp *ccp, struct link *l, int pri, u_short *proto,
           struct mbuf *mp)
{
  struct mppe_state *mop = (struct mppe_state *)v;
  struct mbuf *mo;
  u_short nproto;
  int ilen;
  char *rp;

  log_Printf(LogCCP, "MPPE: Output\n");

  ilen = m_length(mp);

  log_Printf(LogDEBUG, "MPPE: Output: Proto %02x (%d bytes)\n", *proto, ilen);
  if (*proto < 0x21 && *proto > 0xFA) {
    log_Printf(LogDEBUG, "MPPE: Output: Not encrypting\n");
    return mp;
  }

  log_DumpBp(LogDEBUG, "MPPE: Output: Encrypt packet:", mp);

  /* Get mbuf for prefixes */
  mo = m_get(4, MB_CCPOUT);
  mo->m_next = mp;

  /* Init RC4 keys */
  RC4_set_key(&mop->rc4key, mop->keylen, mop->sesskey);

  /* Set MPPE packet prefix */
  rp = MBUF_CTOP(mo);
  *(u_short *)rp = htons(0x9000 | mop->cohnum);

  /* Save encrypted protocol number */
  nproto = htons(*proto);
  RC4(&mop->rc4key, 2, (char *)&nproto, rp + 2);

  /* Encrypt main packet */
  rp = MBUF_CTOP(mp);
  RC4(&mop->rc4key, ilen, rp, rp);

  /* Rotate keys */
  MPPEKeyChange(mop);
  mop->cohnum ++; mop->cohnum &= 0xFFF;

  /* Chage protocol number */
  *proto = ccp_Proto(ccp);

  log_Printf(LogDEBUG, "MPPE: Output: Encrypted: Proto %02x (%d bytes)\n", *proto, m_length(mo));

  return mo;
}

static void
MPPEResetInput(void *v)
{
  log_Printf(LogCCP, "MPPE: Input channel reset\n");
}

static struct mbuf *
MPPEInput(void *v, struct ccp *ccp, u_short *proto, struct mbuf *mp)
{
  struct mppe_state *mip = (struct mppe_state *)v;
  u_short prefix;
  char *rp;
  int ilen;

  log_Printf(LogCCP, "MPPE: Input\n");

  ilen = m_length(mp);

  log_Printf(LogDEBUG, "MPPE: Input: Proto %02x (%d bytes)\n", *proto, ilen);

  log_DumpBp(LogDEBUG, "MPPE: Input: Packet:", mp);

  mp = mbuf_Read(mp, &prefix, 2);
  prefix = ntohs(prefix);
  if ((prefix & 0xF000) != 0x9000) {
    log_Printf(LogERROR, "MPPE: Input: Invalid packet\n");
    m_freem(mp);
    return NULL;
  }

  prefix &= 0xFFF;
  while (prefix != mip->cohnum) {
    MPPEKeyChange(mip);
    mip->cohnum ++; mip->cohnum &= 0xFFF;
  }

  RC4_set_key(&mip->rc4key, mip->keylen, mip->sesskey);

  mp = mbuf_Read(mp, proto, 2);
  RC4(&mip->rc4key, 2, (char *)proto, (char *)proto);
  *proto = ntohs(*proto);

  rp = MBUF_CTOP(mp);
  RC4(&mip->rc4key, m_length(mp), rp, rp);

  log_Printf(LogDEBUG, "MPPE: Input: Decrypted: Proto %02x (%d bytes)\n", *proto, m_length(mp));

  log_DumpBp(LogDEBUG, "MPPE: Input: Decrypted: Packet:", mp);

  return mp;
}

static void
MPPEDictSetup(void *v, struct ccp *ccp, u_short proto, struct mbuf *mi)
{
  log_Printf(LogCCP, "MPPE: DictSetup\n");
}

static const char *
MPPEDispOpts(struct lcp_opt *o)
{
  static char buf[32];
  sprintf(buf, "value 0x%08x", (int)ntohl(*(u_int32_t *)(o->data)));
  return buf;
}

static void
MPPEInitOptsOutput(struct lcp_opt *o, const struct ccp_config *cfg)
{
  u_long val;

  o->len = 6;

  log_Printf(LogCCP, "MPPE: InitOptsOutput\n");

  if (!MPPE_MasterKeyValid) {
    log_Printf(LogWARN, "MPPE: MasterKey is invalid, MPPE is capable only with CHAP81 authentication\n");
    *(u_int32_t *)o->data = htonl(0x0);
    return;
  }

  val = 0x1000000;
  switch(cfg->mppe.keybits) {
  case 128:
    val |= 0x40; break;
  case 56:
    val |= 0x80; break;
  case 40:
    val |= 0x20; break;
  }
  *(u_int32_t *)o->data = htonl(val);
}

static int
MPPESetOptsOutput(struct lcp_opt *o)
{
  u_long *p = (u_long *)(o->data);
  u_long val = ntohl(*p);

  log_Printf(LogCCP, "MPPE: SetOptsOutput\n");

  if (!MPPE_MasterKeyValid) {
    if (*p != 0x0) {
      *p = 0x0;
      return MODE_NAK;
    } else {
      return MODE_ACK;
    }
  }

  if (val == 0x01000020 ||
      val == 0x01000040 ||
      val == 0x01000080)
    return MODE_ACK;

  return MODE_NAK;
}

static int
MPPESetOptsInput(struct lcp_opt *o, const struct ccp_config *cfg)
{
  u_long *p = (u_long *)(o->data);
  u_long val = ntohl(*p);
  u_long mval;

  log_Printf(LogCCP, "MPPE: SetOptsInput\n");

  if (!MPPE_MasterKeyValid) {
    if (*p != 0x0) {
      *p = 0x0;
      return MODE_NAK;
    } else {
      return MODE_ACK;
    }
  }

  mval = 0x01000000;
  switch(cfg->mppe.keybits) {
  case 128:
    mval |= 0x40; break;
  case 56:
    mval |= 0x80; break;
  case 40:
    mval |= 0x20; break;
  }

  if (val == mval)
    return MODE_ACK;

  *p = htonl(mval);

  return MODE_NAK;
}

static void *
MPPEInitInput(struct lcp_opt *o)
{
  struct mppe_state *mip;
  u_int32_t val = ntohl(*(unsigned long *)o->data);

  log_Printf(LogCCP, "MPPE: InitInput\n");

  if (!MPPE_MasterKeyValid) {
    log_Printf(LogERROR, "MPPE: InitInput: MasterKey is invalid!!!!\n");
    return NULL;
  }

  mip = malloc(sizeof(*mip));
  memset(mip, 0, sizeof(*mip));

  if (val & 0x20) {		/* 40-bits */
    mip->keylen = 8;
    mip->keybits = 40;
  } else if (val & 0x80) {	/* 56-bits */
    mip->keylen = 8;
    mip->keybits = 56;
  } else {			/* 128-bits */
    mip->keylen = 16;
    mip->keybits = 128;
  }

  log_Printf(LogDEBUG, "MPPE: InitInput: %d-bits\n", mip->keybits);

  GetAsymetricStartKey(MPPE_MasterKey, mip->mastkey, mip->keylen, 0, 0);
  GetNewKeyFromSHA(mip->mastkey, mip->mastkey, mip->keylen, mip->sesskey);

  MPPEReduceSessionKey(mip);

  MPPEKeyChange(mip);

  mip->cohnum = 0;

  return mip;
}

static void *
MPPEInitOutput(struct lcp_opt *o)
{
  struct mppe_state *mop;
  u_int32_t val = ntohl(*(unsigned long *)o->data);

  log_Printf(LogCCP, "MPPE: InitOutput\n");

  if (!MPPE_MasterKeyValid) {
    log_Printf(LogERROR, "MPPE: InitOutput: MasterKey is invalid!!!!\n");
    return NULL;
  }

  mop = malloc(sizeof(*mop));
  memset(mop, 0, sizeof(*mop));

  if (val & 0x20) {		/* 40-bits */
    mop->keylen = 8;
    mop->keybits = 40;
  } else if (val & 0x80) {	/* 56-bits */
    mop->keylen = 8;
    mop->keybits = 56;
  } else {			/* 128-bits */
    mop->keylen = 16;
    mop->keybits = 128;
  }

  log_Printf(LogDEBUG, "MPPE: InitOutput: %d-bits\n", mop->keybits);

  GetAsymetricStartKey(MPPE_MasterKey, mop->mastkey, mop->keylen, 1, 0);
  GetNewKeyFromSHA(mop->mastkey, mop->mastkey, mop->keylen, mop->sesskey);

  MPPEReduceSessionKey(mop);

  MPPEKeyChange(mop);

  mop->cohnum = 0;

  return mop;
}

static void
MPPETermInput(void *v)
{
  log_Printf(LogCCP, "MPPE: TermInput\n");
  free(v);
}

static void
MPPETermOutput(void *v)
{
  log_Printf(LogCCP, "MPPE: TermOutput\n");
  free(v);
}

const struct ccp_algorithm MPPEAlgorithm = {
  TY_MPPE,
  CCP_NEG_MPPE,
  MPPEDispOpts,
  {
    MPPESetOptsInput,
    MPPEInitInput,
    MPPETermInput,
    MPPEResetInput,
    MPPEInput,
    MPPEDictSetup
  },
  {
    MPPEInitOptsOutput,
    MPPESetOptsOutput,
    MPPEInitOutput,
    MPPETermOutput,
    MPPEResetOutput,
    MPPEOutput
  },
};


/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
 *                    Ian Donaldson <iand@labtam.labtam.oz.au>
 *                    Carsten Bormann <cabo@cs.tu-berlin.de>
 *                    Dave Rand <dlr@bungi.com>/<dave_rand@novell.com>
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
 *	$Id: pred.c,v 1.19 1997/12/21 12:11:08 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "loadalias.h"
#include "vars.h"
#include "timer.h"
#include "fsm.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "lcp.h"
#include "ccp.h"
#include "pred.h"

/* The following hash code is the heart of the algorithm:
 * It builds a sliding hash sum of the previous 3-and-a-bit characters
 * which will be used to index the guess table.
 * A better hash function would result in additional compression,
 * at the expense of time.
 */
#define IHASH(x) do {iHash = (iHash << 4) ^ (x);} while(0)
#define OHASH(x) do {oHash = (oHash << 4) ^ (x);} while(0)
#define GUESS_TABLE_SIZE 65536

static unsigned short int iHash, oHash;
static unsigned char *InputGuessTable;
static unsigned char *OutputGuessTable;

static int
compress(u_char * source, u_char * dest, int len)
{
  int i, bitmask;
  unsigned char *flagdest, flags, *orgdest;

  orgdest = dest;
  while (len) {
    flagdest = dest++;
    flags = 0;			/* All guess wrong initially */
    for (bitmask = 1, i = 0; i < 8 && len; i++, bitmask <<= 1) {
      if (OutputGuessTable[oHash] == *source) {
	flags |= bitmask;	/* Guess was right - don't output */
      } else {
	OutputGuessTable[oHash] = *source;
	*dest++ = *source;	/* Guess wrong, output char */
      }
      OHASH(*source++);
      len--;
    }
    *flagdest = flags;
  }
  return (dest - orgdest);
}

static void
SyncTable(u_char * source, u_char * dest, int len)
{

  while (len--) {
    if (InputGuessTable[iHash] != *source) {
      InputGuessTable[iHash] = *source;
    }
    IHASH(*dest++ = *source++);
  }
}

static int
decompress(u_char * source, u_char * dest, int len)
{
  int i, bitmask;
  unsigned char flags, *orgdest;

  orgdest = dest;
  while (len) {
    flags = *source++;
    len--;
    for (i = 0, bitmask = 1; i < 8; i++, bitmask <<= 1) {
      if (flags & bitmask) {
	*dest = InputGuessTable[iHash];	/* Guess correct */
      } else {
	if (!len)
	  break;		/* we seem to be really done -- cabo */
	InputGuessTable[iHash] = *source;	/* Guess wrong */
	*dest = *source++;	/* Read from source */
	len--;
      }
      IHASH(*dest++);
    }
  }
  return (dest - orgdest);
}

static void
Pred1TermInput(void)
{
  if (InputGuessTable != NULL) {
    free(InputGuessTable);
    InputGuessTable = NULL;
  }
}

static void
Pred1TermOutput(void)
{
  if (OutputGuessTable != NULL) {
    free(OutputGuessTable);
    OutputGuessTable = NULL;
  }
}

static void
Pred1ResetInput(void)
{
  iHash = 0;
  memset(InputGuessTable, '\0', GUESS_TABLE_SIZE);
  LogPrintf(LogCCP, "Predictor1: Input channel reset\n");
}

static void
Pred1ResetOutput(void)
{
  oHash = 0;
  memset(OutputGuessTable, '\0', GUESS_TABLE_SIZE);
  LogPrintf(LogCCP, "Predictor1: Output channel reset\n");
}

static int
Pred1InitInput(void)
{
  if (InputGuessTable == NULL)
    if ((InputGuessTable = malloc(GUESS_TABLE_SIZE)) == NULL)
      return 0;
  Pred1ResetInput();
  return 1;
}

static int
Pred1InitOutput(void)
{
  if (OutputGuessTable == NULL)
    if ((OutputGuessTable = malloc(GUESS_TABLE_SIZE)) == NULL)
      return 0;
  Pred1ResetOutput();
  return 1;
}

static int
Pred1Output(int pri, u_short proto, struct mbuf * bp)
{
  struct mbuf *mwp;
  u_char *cp, *wp, *hp;
  int orglen, len;
  u_char bufp[MAX_MTU + 2];
  u_short fcs;

  orglen = plength(bp) + 2;	/* add count of proto */
  mwp = mballoc((orglen + 2) / 8 * 9 + 12, MB_HDLCOUT);
  hp = wp = MBUF_CTOP(mwp);
  cp = bufp;
  *wp++ = *cp++ = orglen >> 8;
  *wp++ = *cp++ = orglen & 0377;
  *cp++ = proto >> 8;
  *cp++ = proto & 0377;
  mbread(bp, cp, orglen - 2);
  fcs = HdlcFcs(INITFCS, bufp, 2 + orglen);
  fcs = ~fcs;

  len = compress(bufp + 2, wp, orglen);
  LogPrintf(LogDEBUG, "Pred1Output: orglen (%d) --> len (%d)\n", orglen, len);
  CcpInfo.uncompout += orglen;
  if (len < orglen) {
    *hp |= 0x80;
    wp += len;
    CcpInfo.compout += len;
  } else {
    memcpy(wp, bufp + 2, orglen);
    wp += orglen;
    CcpInfo.compout += orglen;
  }

  *wp++ = fcs & 0377;
  *wp++ = fcs >> 8;
  mwp->cnt = wp - MBUF_CTOP(mwp);
  HdlcOutput(PRI_NORMAL, PROTO_COMPD, mwp);
  return 1;
}

static struct mbuf *
Pred1Input(u_short *proto, struct mbuf *bp)
{
  u_char *cp, *pp;
  int len, olen, len1;
  struct mbuf *wp;
  u_char *bufp;
  u_short fcs;

  wp = mballoc(MAX_MTU + 2, MB_IPIN);
  cp = MBUF_CTOP(bp);
  olen = plength(bp);
  pp = bufp = MBUF_CTOP(wp);
  *pp++ = *cp & 0177;
  len = *cp++ << 8;
  *pp++ = *cp;
  len += *cp++;
  CcpInfo.uncompin += len & 0x7fff;
  if (len & 0x8000) {
    len1 = decompress(cp, pp, olen - 4);
    CcpInfo.compin += olen;
    len &= 0x7fff;
    if (len != len1) {		/* Error is detected. Send reset request */
      LogPrintf(LogCCP, "Pred1: Length error\n");
      CcpSendResetReq(&CcpFsm);
      pfree(bp);
      pfree(wp);
      return NULL;
    }
    cp += olen - 4;
    pp += len1;
  } else {
    CcpInfo.compin += len;
    SyncTable(cp, pp, len);
    cp += len;
    pp += len;
  }
  *pp++ = *cp++;		/* CRC */
  *pp++ = *cp++;
  fcs = HdlcFcs(INITFCS, bufp, wp->cnt = pp - bufp);
  if (fcs != GOODFCS)
    LogPrintf(LogDEBUG, "Pred1Input: fcs = 0x%04x (%s), len = 0x%x,"
	      " olen = 0x%x\n", fcs, (fcs == GOODFCS) ? "good" : "bad",
	      len, olen);
  if (fcs == GOODFCS) {
    wp->offset += 2;		/* skip length */
    wp->cnt -= 4;		/* skip length & CRC */
    pp = MBUF_CTOP(wp);
    *proto = *pp++;
    if (*proto & 1) {
      wp->offset++;
      wp->cnt--;
    } else {
      wp->offset += 2;
      wp->cnt -= 2;
      *proto = (*proto << 8) | *pp++;
    }
    pfree(bp);
    return wp;
  } else {
    LogDumpBp(LogHDLC, "Bad FCS", wp);
    CcpSendResetReq(&CcpFsm);
    pfree(wp);
  }
  pfree(bp);
  return NULL;
}

static void
Pred1DictSetup(u_short proto, struct mbuf * bp)
{
}

static const char *
Pred1DispOpts(struct lcp_opt *o)
{
  return NULL;
}

static void
Pred1GetOpts(struct lcp_opt *o)
{
  o->id = TY_PRED1;
  o->len = 2;
}

static int
Pred1SetOpts(struct lcp_opt *o)
{
  if (o->id != TY_PRED1 || o->len != 2) {
    Pred1GetOpts(o);
    return MODE_NAK;
  }
  return MODE_ACK;
}

const struct ccp_algorithm Pred1Algorithm = {
  TY_PRED1,
  ConfPred1,
  Pred1DispOpts,
  {
    Pred1GetOpts,
    Pred1SetOpts,
    Pred1InitInput,
    Pred1TermInput,
    Pred1ResetInput,
    Pred1Input,
    Pred1DictSetup
  },
  {
    Pred1GetOpts,
    Pred1SetOpts,
    Pred1InitOutput,
    Pred1TermOutput,
    Pred1ResetOutput,
    Pred1Output
  },
};

/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: deflate.c,v 1.4 1997/12/21 12:11:04 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "loadalias.h"
#include "vars.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "lcpproto.h"
#include "timer.h"
#include "fsm.h"
#include "deflate.h"

/* Our state */
struct deflate_state {
    u_short seqno;
    int dodgy_seqno;
    z_stream cx;
};

static int iWindowSize = 15;
static int oWindowSize = 15;
static struct deflate_state InputState, OutputState;
static char garbage[10];
static u_char EMPTY_BLOCK[4] = { 0x00, 0x00, 0xff, 0xff };

#define DEFLATE_CHUNK_LEN 1024		/* Allocate mbufs this size */

static void
DeflateResetOutput(void)
{
  OutputState.seqno = 0;
  OutputState.dodgy_seqno = 0;
  deflateReset(&OutputState.cx);
  LogPrintf(LogCCP, "Deflate: Output channel reset\n");
}

static int
DeflateOutput(int pri, u_short proto, struct mbuf *mp)
{
  u_char *wp, *rp;
  int olen, ilen, len, res, flush;
  struct mbuf *mo_head, *mo, *mi_head, *mi;

  ilen = plength(mp);
  LogPrintf(LogDEBUG, "DeflateOutput: Proto %02x (%d bytes)\n", proto, ilen);
  LogDumpBp(LogDEBUG, "DeflateOutput: Compress packet:", mp);

  /* Stuff the protocol in front of the input */
  mi_head = mi = mballoc(2, MB_HDLCOUT);
  mi->next = mp;
  rp = MBUF_CTOP(mi);
  if (proto < 0x100) {			/* Compress the protocol */
    rp[0] = proto & 0377;
    mi->cnt = 1;
  } else {				/* Don't compress the protocol */
    rp[0] = proto >> 8;
    rp[1] = proto & 0377;
    mi->cnt = 2;
  }

  /* Allocate the initial output mbuf */
  mo_head = mo = mballoc(DEFLATE_CHUNK_LEN, MB_HDLCOUT);
  mo->cnt = 2;
  wp = MBUF_CTOP(mo);
  *wp++ = OutputState.seqno >> 8;
  *wp++ = OutputState.seqno & 0377;
  LogPrintf(LogDEBUG, "DeflateOutput: Seq %d\n", OutputState.seqno);
  OutputState.seqno++;

  /* Set up the deflation context */
  OutputState.cx.next_out = wp;
  OutputState.cx.avail_out = DEFLATE_CHUNK_LEN - 2;
  OutputState.cx.next_in = MBUF_CTOP(mi);
  OutputState.cx.avail_in = mi->cnt;
  flush = Z_NO_FLUSH;

  olen = 0;
  while (1) {
    if ((res = deflate(&OutputState.cx, flush)) != Z_OK) {
      if (res == Z_STREAM_END)
        break;			/* Done */
      LogPrintf(LogERROR, "DeflateOutput: deflate returned %d (%s)\n",
                res, OutputState.cx.msg ? OutputState.cx.msg : "");
      pfree(mo_head);
      mbfree(mi_head);
      OutputState.seqno--;
      return 1;			/* packet dropped */
    }

    if (flush == Z_SYNC_FLUSH && OutputState.cx.avail_out != 0)
      break;

    if (OutputState.cx.avail_in == 0 && mi->next != NULL) {
      mi = mi->next;
      OutputState.cx.next_in = MBUF_CTOP(mi);
      OutputState.cx.avail_in = mi->cnt;
      if (mi->next == NULL)
        flush = Z_SYNC_FLUSH;
    }

    if (OutputState.cx.avail_out == 0) {
      mo->next = mballoc(DEFLATE_CHUNK_LEN, MB_HDLCOUT);
      olen += (mo->cnt = DEFLATE_CHUNK_LEN);
      mo = mo->next;
      mo->cnt = 0;
      OutputState.cx.next_out = MBUF_CTOP(mo);
      OutputState.cx.avail_out = DEFLATE_CHUNK_LEN;
    }
  }

  olen += (mo->cnt = DEFLATE_CHUNK_LEN - OutputState.cx.avail_out);
  olen -= 4;		/* exclude the trailing EMPTY_BLOCK */

  /*
   * If the output packet (including seqno and excluding the EMPTY_BLOCK)
   * got bigger, send the original - returning 0 to HdlcOutput() will
   * continue to send ``mp''.
   */
  if (olen >= ilen) {
    pfree(mo_head);
    mbfree(mi_head);
    LogPrintf(LogDEBUG, "DeflateOutput: %d => %d: Uncompressible (0x%04x)\n",
              ilen, olen, proto);
    CcpInfo.uncompout += ilen;
    CcpInfo.compout += ilen;	/* We measure this stuff too */
    return 0;
  }

  pfree(mi_head);

  /*
   * Lose the last four bytes of our output.
   * XXX: We should probably assert that these are the same as the
   *      contents of EMPTY_BLOCK.
   */
  for (mo = mo_head, len = mo->cnt; len < olen; mo = mo->next, len += mo->cnt)
    ;
  mo->cnt -= len - olen;
  if (mo->next != NULL) {
    pfree(mo->next);
    mo->next = NULL;
  }

  CcpInfo.uncompout += ilen;
  CcpInfo.compout += olen;

  LogPrintf(LogDEBUG, "DeflateOutput: %d => %d bytes, proto 0x%04x\n",
            ilen, olen, proto);

  HdlcOutput(PRI_NORMAL, PROTO_COMPD, mo_head);
  return 1;
}

static void
DeflateResetInput(void)
{
  InputState.seqno = 0;
  InputState.dodgy_seqno = 0;
  inflateReset(&InputState.cx);
  LogPrintf(LogCCP, "Deflate: Input channel reset\n");
}

static struct mbuf *
DeflateInput(u_short *proto, struct mbuf *mi)
{
  struct mbuf *mo, *mo_head, *mi_head;
  u_char *wp;
  int ilen, olen;
  int seq, flush, res, first;
  u_char hdr[2];

  LogDumpBp(LogDEBUG, "DeflateInput: Decompress packet:", mi);
  mi_head = mi = mbread(mi, hdr, 2);
  ilen = 2;

  /* Check the sequence number. */
  seq = (hdr[0] << 8) + hdr[1];
  LogPrintf(LogDEBUG, "DeflateInput: Seq %d\n", seq);
  if (seq != InputState.seqno) {
    if (InputState.dodgy_seqno && seq < InputState.seqno)
      InputState.seqno = seq;
    else {
      LogPrintf(LogERROR, "DeflateInput: Seq error: Got %d, expected %d\n",
                seq, InputState.seqno);
      pfree(mi_head);
      CcpSendResetReq(&CcpFsm);
      return NULL;
    }
  }
  InputState.seqno++;
  InputState.dodgy_seqno = 0;

  /* Allocate an output mbuf */
  mo_head = mo = mballoc(DEFLATE_CHUNK_LEN, MB_IPIN);

  /* Our proto starts with 0 if it's compressed */
  wp = MBUF_CTOP(mo);
  wp[0] = '\0';

  /*
   * We set avail_out to 1 initially so we can look at the first
   * byte of the output and decide whether we have a compressed
   * proto field.
   */
  InputState.cx.next_in = MBUF_CTOP(mi);
  InputState.cx.avail_in = mi->cnt;
  InputState.cx.next_out = wp + 1;
  InputState.cx.avail_out = 1;
  ilen += mi->cnt;

  flush = mi->next ? Z_NO_FLUSH : Z_SYNC_FLUSH;
  first = 1;
  olen = 0;

  while (1) {
    if ((res = inflate(&InputState.cx, flush)) != Z_OK) {
      if (res == Z_STREAM_END)
        break;			/* Done */
      LogPrintf(LogERROR, "DeflateInput: inflate returned %d (%s)\n",
                res, InputState.cx.msg ? InputState.cx.msg : "");
      pfree(mo_head);
      pfree(mi);
      CcpSendResetReq(&CcpFsm);
      return NULL;
    }

    if (flush == Z_SYNC_FLUSH && InputState.cx.avail_out != 0)
      break;

    if (InputState.cx.avail_in == 0 && mi && (mi = mbfree(mi)) != NULL) {
      /* underflow */
      InputState.cx.next_in = MBUF_CTOP(mi);
      ilen += (InputState.cx.avail_in = mi->cnt);
      if (mi->next == NULL)
        flush = Z_SYNC_FLUSH;
    }

    if (InputState.cx.avail_out == 0)
      /* overflow */
      if (first) {
        if (!(wp[1] & 1)) {
          /* 2 byte proto, shuffle it back in output */
          wp[0] = wp[1];
          InputState.cx.next_out--;
          InputState.cx.avail_out = DEFLATE_CHUNK_LEN-1;
        } else
          InputState.cx.avail_out = DEFLATE_CHUNK_LEN-2;
        first = 0;
      } else {
        olen += (mo->cnt = DEFLATE_CHUNK_LEN);
        mo->next = mballoc(DEFLATE_CHUNK_LEN, MB_IPIN);
        mo = mo->next;
        InputState.cx.next_out = MBUF_CTOP(mo);
        InputState.cx.avail_out = DEFLATE_CHUNK_LEN;
      }
  }

  if (mi != NULL)
    pfree(mi);

  if (first) {
    LogPrintf(LogERROR, "DeflateInput: Length error\n");
    pfree(mo_head);
    CcpSendResetReq(&CcpFsm);
    return NULL;
  }

  olen += (mo->cnt = DEFLATE_CHUNK_LEN - InputState.cx.avail_out);

  *proto = ((u_short)wp[0] << 8) | wp[1];
  mo_head->offset += 2;
  mo_head->cnt -= 2;
  olen -= 2;

  CcpInfo.compin += ilen;
  CcpInfo.uncompin += olen;

  LogPrintf(LogDEBUG, "DeflateInput: %d => %d bytes, proto 0x%04x\n",
            ilen, olen, *proto);

  /*
   * Simulate an EMPTY_BLOCK so that our dictionary stays in sync.
   * The peer will have silently removed this!
   */
  InputState.cx.next_out = garbage;
  InputState.cx.avail_out = sizeof garbage;
  InputState.cx.next_in = EMPTY_BLOCK;
  InputState.cx.avail_in = sizeof EMPTY_BLOCK;
  inflate(&InputState.cx, Z_SYNC_FLUSH);

  return mo_head;
}

static void
DeflateDictSetup(u_short proto, struct mbuf *mi)
{
  int res, flush, expect_error;
  u_char *rp;
  struct mbuf *mi_head;
  short len;

  LogPrintf(LogDEBUG, "DeflateDictSetup: Got seq %d\n", InputState.seqno);

  /*
   * Stuff an ``uncompressed data'' block header followed by the
   * protocol in front of the input
   */
  mi_head = mballoc(7, MB_HDLCOUT);
  mi_head->next = mi;
  len = plength(mi);
  mi = mi_head;
  rp = MBUF_CTOP(mi);
  if (proto < 0x100) {			/* Compress the protocol */
    rp[5] = proto & 0377;
    mi->cnt = 6;
    len++;
  } else {				/* Don't compress the protocol */
    rp[5] = proto >> 8;
    rp[6] = proto & 0377;
    mi->cnt = 7;
    len += 2;
  }
  rp[0] = 0x80;				/* BITS: 100xxxxx */
  rp[1] = len & 0377;			/* The length */
  rp[2] = len >> 8;
  rp[3] = (~len) & 0377;		/* One's compliment of the length */
  rp[4] = (~len) >> 8;

  InputState.cx.next_in = rp;
  InputState.cx.avail_in = mi->cnt;
  InputState.cx.next_out = garbage;
  InputState.cx.avail_out = sizeof garbage;
  flush = Z_NO_FLUSH;
  expect_error = 0;

  while (1) {
    if ((res = inflate(&InputState.cx, flush)) != Z_OK) {
      if (res == Z_STREAM_END)
        break;			/* Done */
      if (expect_error && res == Z_BUF_ERROR)
        break;
      LogPrintf(LogERROR, "DeflateDictSetup: inflate returned %d (%s)\n",
                res, InputState.cx.msg ? InputState.cx.msg : "");
      LogPrintf(LogERROR, "DeflateDictSetup: avail_in %d, avail_out %d\n",
                InputState.cx.avail_in, InputState.cx.avail_out);
      CcpSendResetReq(&CcpFsm);
      mbfree(mi_head);		/* lose our allocated ``head'' buf */
      return;
    }

    if (flush == Z_SYNC_FLUSH && InputState.cx.avail_out != 0)
      break;

    if (InputState.cx.avail_in == 0 && mi && (mi = mi->next) != NULL) {
      /* underflow */
      InputState.cx.next_in = MBUF_CTOP(mi);
      InputState.cx.avail_in = mi->cnt;
      if (mi->next == NULL)
        flush = Z_SYNC_FLUSH;
    }

    if (InputState.cx.avail_out == 0) {
      if (InputState.cx.avail_in == 0)
        /*
         * This seems to be a bug in libz !  If inflate() finished
         * with 0 avail_in and 0 avail_out *and* this is the end of
         * our input *and* inflate() *has* actually written all the
         * output it's going to, it *doesn't* return Z_STREAM_END !
         * When we subsequently call it with no more input, it gives
         * us Z_BUF_ERROR :-(  It seems pretty safe to ignore this
         * error (the dictionary seems to stay in sync).  In the worst
         * case, we'll drop the next compressed packet and do a
         * CcpReset() then.
         */
        expect_error = 1;
      /* overflow */
      InputState.cx.next_out = garbage;
      InputState.cx.avail_out = sizeof garbage;
    }
  }

  CcpInfo.compin += len;
  CcpInfo.uncompin += len;

  InputState.seqno++;
  mbfree(mi_head);		/* lose our allocated ``head'' buf */
}

static const char *
DeflateDispOpts(struct lcp_opt *o)
{
  static char disp[7];

  sprintf(disp, "win %d", (o->data[0]>>4) + 8);
  return disp;
}

static void
DeflateGetInputOpts(struct lcp_opt *o)
{
  o->id = TY_DEFLATE;
  o->len = 4;
  o->data[0] = ((iWindowSize-8)<<4)+8;
  o->data[1] = '\0';
}

static void
DeflateGetOutputOpts(struct lcp_opt *o)
{
  o->id = TY_DEFLATE;
  o->len = 4;
  o->data[0] = ((oWindowSize-8)<<4)+8;
  o->data[1] = '\0';
}

static void
PppdDeflateGetInputOpts(struct lcp_opt *o)
{
  o->id = TY_PPPD_DEFLATE;
  o->len = 4;
  o->data[0] = ((iWindowSize-8)<<4)+8;
  o->data[1] = '\0';
}

static void
PppdDeflateGetOutputOpts(struct lcp_opt *o)
{
  o->id = TY_PPPD_DEFLATE;
  o->len = 4;
  o->data[0] = ((oWindowSize-8)<<4)+8;
  o->data[1] = '\0';
}

static int
DeflateSetOpts(struct lcp_opt *o, int *sz)
{
  if (o->len != 4 || (o->data[0]&15) != 8 || o->data[1] != '\0') {
    return MODE_REJ;
  }
  *sz = (o->data[0] >> 4) + 8;
  if (*sz > 15) {
    *sz = 15;
    return MODE_NAK;
  }

  return MODE_ACK;
}

static int
DeflateSetInputOpts(struct lcp_opt *o)
{
  int res;
  res = DeflateSetOpts(o, &iWindowSize);
  if (res != MODE_ACK)
    DeflateGetInputOpts(o);
  return res;
}

static int
DeflateSetOutputOpts(struct lcp_opt *o)
{
  int res;
  res = DeflateSetOpts(o, &oWindowSize);
  if (res != MODE_ACK)
    DeflateGetOutputOpts(o);
  return res;
}

static int
PppdDeflateSetInputOpts(struct lcp_opt *o)
{
  int res;
  res = DeflateSetOpts(o, &iWindowSize);
  if (res != MODE_ACK)
    PppdDeflateGetInputOpts(o);
  return res;
}

static int
PppdDeflateSetOutputOpts(struct lcp_opt *o)
{
  int res;
  res = DeflateSetOpts(o, &oWindowSize);
  if (res != MODE_ACK)
    PppdDeflateGetOutputOpts(o);
  return res;
}

static int
DeflateInitInput(void)
{
  InputState.cx.zalloc = NULL;
  InputState.cx.opaque = NULL;
  InputState.cx.zfree = NULL;
  InputState.cx.next_out = NULL;
  if (inflateInit2(&InputState.cx, -iWindowSize) != Z_OK)
    return 0;
  DeflateResetInput();
  /*
   * When we begin, we may start adding to our dictionary before the
   * peer does.  If `dodgy_seqno' is set, we'll allow the peer to send
   * us a seqno that's too small and just adjust seqno accordingly -
   * deflate is a sliding window compressor !
   */
  InputState.dodgy_seqno = 1;
  return 1;
}

static int
DeflateInitOutput(void)
{
  OutputState.cx.zalloc = NULL;
  OutputState.cx.opaque = NULL;
  OutputState.cx.zfree = NULL;
  OutputState.cx.next_in = NULL;
  if (deflateInit2(&OutputState.cx, Z_DEFAULT_COMPRESSION, 8,
                   -oWindowSize, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return 0;
  DeflateResetOutput();
  return 1;
}

static void
DeflateTermInput(void)
{
  iWindowSize = 15;
  inflateEnd(&InputState.cx);
}

static void
DeflateTermOutput(void)
{
  oWindowSize = 15;
  deflateEnd(&OutputState.cx);
}

const struct ccp_algorithm PppdDeflateAlgorithm = {
  TY_PPPD_DEFLATE,	/* pppd (wrongly) expects this ``type'' field */
  ConfPppdDeflate,
  DeflateDispOpts,
  {
    PppdDeflateGetInputOpts,
    PppdDeflateSetInputOpts,
    DeflateInitInput,
    DeflateTermInput,
    DeflateResetInput,
    DeflateInput,
    DeflateDictSetup
  },
  {
    PppdDeflateGetOutputOpts,
    PppdDeflateSetOutputOpts,
    DeflateInitOutput,
    DeflateTermOutput,
    DeflateResetOutput,
    DeflateOutput
  },
};

const struct ccp_algorithm DeflateAlgorithm = {
  TY_DEFLATE,		/* rfc 1979 */
  ConfDeflate,
  DeflateDispOpts,
  {
    DeflateGetInputOpts,
    DeflateSetInputOpts,
    DeflateInitInput,
    DeflateTermInput,
    DeflateResetInput,
    DeflateInput,
    DeflateDictSetup
  },
  {
    DeflateGetOutputOpts,
    DeflateSetOutputOpts,
    DeflateInitOutput,
    DeflateTermOutput,
    DeflateResetOutput,
    DeflateOutput
  },
};

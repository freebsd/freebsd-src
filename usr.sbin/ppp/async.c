/*
 *	             PPP Async HDLC Module
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
 * $FreeBSD: src/usr.sbin/ppp/async.c,v 1.23 1999/12/20 20:29:40 brian Exp $
 *
 */
#include <sys/types.h>

#include <string.h>
#include <termios.h>

#include "layer.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "proto.h"
#include "async.h"
#include "throughput.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"

#define MODE_HUNT 0x01
#define MODE_ESC  0x02

void
async_Init(struct async *async)
{
  async->mode = MODE_HUNT;
  async->length = 0;
  async->my_accmap = async->his_accmap = 0xffffffff;
  memset(async->cfg.EscMap, '\0', sizeof async->cfg.EscMap);
}

void
async_SetLinkParams(struct async *async, struct lcp *lcp)
{
  async->my_accmap = lcp->want_accmap;
  async->his_accmap = lcp->his_accmap | lcp->want_accmap;
}

/*
 * Encode into async HDLC byte code
 */
static void
async_Encode(struct async *async, u_char **cp, u_char c, int proto)
{
  u_char *wp;

  wp = *cp;
  if ((c < 0x20 && (proto == PROTO_LCP || (async->his_accmap & (1 << c))))
      || (c == HDLC_ESC) || (c == HDLC_SYN)) {
    *wp++ = HDLC_ESC;
    c ^= HDLC_XOR;
  }
  if (async->cfg.EscMap[32] && async->cfg.EscMap[c >> 3] & (1 << (c & 7))) {
    *wp++ = HDLC_ESC;
    c ^= HDLC_XOR;
  }
  *wp++ = c;
  *cp = wp;
}

static struct mbuf *
async_LayerPush(struct bundle *bundle, struct link *l, struct mbuf *bp,
                int pri, u_short *proto)
{
  struct physical *p = link2physical(l);
  u_char *cp, *sp, *ep;
  struct mbuf *wp;
  int cnt;

  if (!p || m_length(bp) > HDLCSIZE) {
    m_freem(bp);
    return NULL;
  }

  cp = p->async.xbuff;
  ep = cp + HDLCSIZE - 10;
  wp = bp;
  *cp++ = HDLC_SYN;
  while (wp) {
    sp = MBUF_CTOP(wp);
    for (cnt = wp->m_len; cnt > 0; cnt--) {
      async_Encode(&p->async, &cp, *sp++, *proto);
      if (cp >= ep) {
	m_freem(bp);
	return NULL;
      }
    }
    wp = wp->m_next;
  }
  *cp++ = HDLC_SYN;

  cnt = cp - p->async.xbuff;
  m_freem(bp);
  bp = m_get(cnt, MB_ASYNCOUT);
  memcpy(MBUF_CTOP(bp), p->async.xbuff, cnt);
  log_DumpBp(LogASYNC, "Write", bp);

  return bp;
}

static struct mbuf *
async_Decode(struct async *async, u_char c)
{
  struct mbuf *bp;

  if ((async->mode & MODE_HUNT) && c != HDLC_SYN)
    return NULL;

  switch (c) {
  case HDLC_SYN:
    async->mode &= ~MODE_HUNT;
    if (async->length) {		/* packet is ready. */
      bp = m_get(async->length, MB_ASYNCIN);
      mbuf_Write(bp, async->hbuff, async->length);
      async->length = 0;
      return bp;
    }
    break;
  case HDLC_ESC:
    if (!(async->mode & MODE_ESC)) {
      async->mode |= MODE_ESC;
      break;
    }
    /* Fall into ... */
  default:
    if (async->length >= HDLCSIZE) {
      /* packet is too large, discard it */
      log_Printf(LogWARN, "Packet too large (%d), discarding.\n",
                 async->length);
      async->length = 0;
      async->mode = MODE_HUNT;
      break;
    }
    if (async->mode & MODE_ESC) {
      c ^= HDLC_XOR;
      async->mode &= ~MODE_ESC;
    }
    async->hbuff[async->length++] = c;
    break;
  }
  return NULL;
}

static struct mbuf *
async_LayerPull(struct bundle *b, struct link *l, struct mbuf *bp,
                u_short *proto)
{
  struct mbuf *nbp, **last;
  struct physical *p = link2physical(l);
  u_char *ch;
  size_t cnt;

  if (!p) {
    log_Printf(LogERROR, "Can't Pull an async packet from a logical link\n");
    return bp;
  }

  last = &nbp;

  log_DumpBp(LogASYNC, "Read", bp);
  while (bp) {
    ch = MBUF_CTOP(bp);
    for (cnt = bp->m_len; cnt; cnt--) {
      *last = async_Decode(&p->async, *ch++);
      if (*last != NULL)
        last = &(*last)->m_nextpkt;
    }
    bp = m_free(bp);
  }

  return nbp;
}

struct layer asynclayer =
  { LAYER_ASYNC, "async", async_LayerPush, async_LayerPull };

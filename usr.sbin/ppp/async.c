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
 * $Id: async.c,v 1.5 1996/01/11 17:48:35 phk Exp $
 *
 */
#include "fsm.h"
#include "hdlc.h"
#include "lcp.h"
#include "lcpproto.h"
#include "modem.h"
#include "loadalias.h"
#include "vars.h"
#include "os.h"

#define HDLCSIZE	(MAX_MRU*2+6)

struct async_state {
  int mode;
  int length;
  u_char hbuff[HDLCSIZE];	/* recv buffer */
  u_char xbuff[HDLCSIZE];	/* xmit buffer */
  u_long my_accmap;
  u_long his_accmap;
} AsyncState;

#define MODE_HUNT 0x01
#define MODE_ESC  0x02

void
AsyncInit()
{
  struct async_state *stp = &AsyncState;

  stp->mode = MODE_HUNT;
  stp->length = 0;
  stp->my_accmap = stp->his_accmap = 0xffffffff;
}

void
SetLinkParams(lcp)
struct lcpstate *lcp;
{
  struct async_state *stp = &AsyncState;

  stp->my_accmap = lcp->want_accmap;
  stp->his_accmap = lcp->his_accmap;
}

/*
 * Encode into async HDLC byte code if necessary
 */
static void
HdlcPutByte(cp, c, proto)
u_char **cp;
u_char c;
int proto;
{
  u_char *wp;

  wp = *cp;
  if ((c < 0x20 && (proto == PROTO_LCP || (AsyncState.his_accmap & (1<<c))))
	|| (c == HDLC_ESC) || (c == HDLC_SYN)) {
    *wp++ = HDLC_ESC;
    c ^= HDLC_XOR;
  }
  if (EscMap[32] && EscMap[c >> 3] &  (1 << (c&7))) {
    *wp++ = HDLC_ESC;
    c ^= HDLC_XOR;
  }
  *wp++ = c;
  *cp = wp;
}

void
AsyncOutput(pri, bp, proto)
int pri;
struct mbuf *bp;
int proto;
{
  struct async_state *hs = &AsyncState;
  u_char *cp, *sp, *ep;
  struct mbuf *wp;
  int cnt;

  if (plength(bp) > HDLCSIZE) {
    pfree(bp);
    return;
  }
  cp = hs->xbuff;
  ep = cp + HDLCSIZE - 10;
  wp = bp;
  *cp ++ = HDLC_SYN;
  while (wp) {
    sp = MBUF_CTOP(wp);
    for (cnt = wp->cnt; cnt > 0; cnt--) {
      HdlcPutByte(&cp, *sp++, proto);
      if (cp >= ep) {
	pfree(bp);
	return;
      }
    }
    wp = wp->next;
  }
  *cp ++ = HDLC_SYN;

  cnt = cp - hs->xbuff;
  LogDumpBuff(LOG_ASYNC, "WriteModem", hs->xbuff, cnt);
  WriteModem(pri, (char *)hs->xbuff, cnt);
  OsAddOutOctets(cnt);
  pfree(bp);
}

struct mbuf *
AsyncDecode(c)
u_char c;
{
  struct async_state *hs = &AsyncState;
  struct mbuf *bp;

  if ((hs->mode & MODE_HUNT) && c != HDLC_SYN)
    return(NULLBUFF);

  switch (c) {
  case HDLC_SYN:
    hs->mode &= ~MODE_HUNT;
    if (hs->length) {	/* packet is ready. */
      bp = mballoc(hs->length, MB_ASYNC);
      mbwrite(bp, hs->hbuff, hs->length);
      hs->length = 0;
      return(bp);
    }
    break;
  case HDLC_ESC:
    if (!(hs->mode & MODE_ESC)) {
      hs->mode |= MODE_ESC;
      break;
    }
    /* Fall into ... */
  default:
    if (hs->length >= HDLCSIZE) {
      /* packet is too large, discard it */
      logprintf("too large, diacarding.\n");
      hs->length = 0;
      hs->mode = MODE_HUNT;
      break;
    }
    if (hs->mode & MODE_ESC) {
      c ^= HDLC_XOR;
      hs->mode &= ~MODE_ESC;
    }
    hs->hbuff[hs->length++] = c;
    break;
  }
  return NULLBUFF;
}

void
AsyncInput(buff, cnt)
u_char *buff;
int cnt;
{
  struct mbuf *bp;

  OsAddInOctets(cnt);
  if (DEV_IS_SYNC) {
    bp = mballoc(cnt, MB_ASYNC);
    bcopy(buff, MBUF_CTOP(bp), cnt);
    bp->cnt = cnt;
    HdlcInput(bp);
  } else {
    while (cnt > 0) {
      bp = AsyncDecode(*buff++);
      if (bp)
        HdlcInput(bp);
      cnt--;
    }
  }
}

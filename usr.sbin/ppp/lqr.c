/*
 *	      PPP Line Quality Monitoring (LQM) Module
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
 * $Id: lqr.c,v 1.25 1998/05/21 21:46:33 brian Exp $
 *
 *	o LQR based on RFC1333
 *
 * TODO:
 *	o LQM policy
 *	o Allow user to configure LQM method and interval.
 */

#include <sys/types.h>
#include <sys/un.h>

#include <string.h>
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
#include "async.h"
#include "throughput.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "mp.h"
#include "chat.h"
#include "auth.h"
#include "chap.h"
#include "command.h"
#include "datalink.h"

struct echolqr {
  u_int32_t magic;
  u_int32_t signature;
  u_int32_t sequence;
};

#define	SIGNATURE  0x594e4f54

static void
SendEchoReq(struct lcp *lcp)
{
  struct hdlc *hdlc = &link2physical(lcp->fsm.link)->hdlc;
  struct echolqr echo;

  echo.magic = htonl(lcp->want_magic);
  echo.signature = htonl(SIGNATURE);
  echo.sequence = htonl(hdlc->lqm.echo.seq_sent);
  fsm_Output(&lcp->fsm, CODE_ECHOREQ, hdlc->lqm.echo.seq_sent++,
            (u_char *)&echo, sizeof echo);
}

void
lqr_RecvEcho(struct fsm *fp, struct mbuf * bp)
{
  struct hdlc *hdlc = &link2physical(fp->link)->hdlc;
  struct echolqr *lqr;
  u_int32_t seq;

  if (mbuf_Length(bp) == sizeof(struct echolqr)) {
    lqr = (struct echolqr *) MBUF_CTOP(bp);
    if (ntohl(lqr->signature) == SIGNATURE) {
      seq = ntohl(lqr->sequence);
      /* careful not to update lqm.echo.seq_recv with older values */
      if ((hdlc->lqm.echo.seq_recv > (u_int32_t)0 - 5 && seq < 5) ||
          (hdlc->lqm.echo.seq_recv <= (u_int32_t)0 - 5 &&
           seq > hdlc->lqm.echo.seq_recv))
        hdlc->lqm.echo.seq_recv = seq;
    } else
      log_Printf(LogERROR, "lqr_RecvEcho: Got sig 0x%08x, expecting 0x%08x !\n",
                (unsigned)ntohl(lqr->signature), (unsigned)SIGNATURE);
  } else
    log_Printf(LogERROR, "lqr_RecvEcho: Got packet size %d, expecting %d !\n",
              mbuf_Length(bp), sizeof(struct echolqr));
}

void
lqr_ChangeOrder(struct lqrdata * src, struct lqrdata * dst)
{
  u_long *sp, *dp;
  int n;

  sp = (u_long *) src;
  dp = (u_long *) dst;
  for (n = 0; n < sizeof(struct lqrdata) / sizeof(u_int32_t); n++)
    *dp++ = ntohl(*sp++);
}

static void
SendLqrData(struct lcp *lcp)
{
  struct mbuf *bp;

  bp = mbuf_Alloc(sizeof(struct lqrdata), MB_LQR);
  hdlc_Output(lcp->fsm.link, PRI_LINK, PROTO_LQR, bp);
}

static void
SendLqrReport(void *v)
{
  struct lcp *lcp = (struct lcp *)v;
  struct physical *p = link2physical(lcp->fsm.link);

  timer_Stop(&p->hdlc.lqm.timer);

  if (p->hdlc.lqm.method & LQM_LQR) {
    if (p->hdlc.lqm.lqr.resent > 5) {
      /* XXX: Should implement LQM strategy */
      log_Printf(LogPHASE, "%s: ** Too many LQR packets lost **\n",
                lcp->fsm.link->name);
      log_Printf(LogLQM, "%s: Too many LQR packets lost\n",
                lcp->fsm.link->name);
      p->hdlc.lqm.method = 0;
      datalink_Down(p->dl, CLOSE_NORMAL);
    } else {
      SendLqrData(lcp);
      p->hdlc.lqm.lqr.resent++;
    }
  } else if (p->hdlc.lqm.method & LQM_ECHO) {
    if ((p->hdlc.lqm.echo.seq_sent > 5 &&
         p->hdlc.lqm.echo.seq_sent - 5 > p->hdlc.lqm.echo.seq_recv) ||
        (p->hdlc.lqm.echo.seq_sent <= 5 &&
         p->hdlc.lqm.echo.seq_sent > p->hdlc.lqm.echo.seq_recv + 5)) {
      log_Printf(LogPHASE, "%s: ** Too many ECHO LQR packets lost **\n",
                lcp->fsm.link->name);
      log_Printf(LogLQM, "%s: Too many ECHO LQR packets lost\n",
                lcp->fsm.link->name);
      p->hdlc.lqm.method = 0;
      datalink_Down(p->dl, CLOSE_NORMAL);
    } else
      SendEchoReq(lcp);
  }
  if (p->hdlc.lqm.method && p->hdlc.lqm.timer.load)
    timer_Start(&p->hdlc.lqm.timer);
}

void
lqr_Input(struct physical *physical, struct mbuf *bp)
{
  int len;

  len = mbuf_Length(bp);
  if (len != sizeof(struct lqrdata))
    log_Printf(LogERROR, "lqr_Input: Got packet size %d, expecting %d !\n",
              len, sizeof(struct lqrdata));
  else if (!IsAccepted(physical->link.lcp.cfg.lqr) &&
           !(physical->hdlc.lqm.method & LQM_LQR)) {
    bp->offset -= 2;
    bp->cnt += 2;
    lcp_SendProtoRej(physical->hdlc.lqm.owner, MBUF_CTOP(bp), bp->cnt);
  } else {
    struct lqrdata *lqr;
    struct lcp *lcp;
    u_int32_t lastLQR;

    lqr = (struct lqrdata *)MBUF_CTOP(bp);
    lcp = physical->hdlc.lqm.owner;
    if (ntohl(lqr->MagicNumber) != physical->hdlc.lqm.owner->his_magic)
      log_Printf(LogERROR, "lqr_Input: magic %x != expecting %x\n",
		(unsigned)ntohl(lqr->MagicNumber),
                physical->hdlc.lqm.owner->his_magic);
    else {
      /*
       * Remember our PeerInLQRs, then convert byte order and save
       */
      lastLQR = physical->hdlc.lqm.lqr.peer.PeerInLQRs;

      lqr_ChangeOrder(lqr, &physical->hdlc.lqm.lqr.peer);
      lqr_Dump(physical->link.name, "Input", &physical->hdlc.lqm.lqr.peer);
      /* we have received an LQR from peer */
      physical->hdlc.lqm.lqr.resent = 0;

      /*
       * Generate an LQR response if we're not running an LQR timer OR
       * two successive LQR's PeerInLQRs are the same OR we're not going to
       * send our next one before the peers max timeout.
       */
      if (physical->hdlc.lqm.timer.load == 0 ||
          !(physical->hdlc.lqm.method & LQM_LQR) ||
          (lastLQR && lastLQR == physical->hdlc.lqm.lqr.peer.PeerInLQRs) ||
          (physical->hdlc.lqm.lqr.peer_timeout && 
           physical->hdlc.lqm.timer.rest * 100 / SECTICKS >
           physical->hdlc.lqm.lqr.peer_timeout))
        SendLqrData(physical->hdlc.lqm.owner);
    }
  }
  mbuf_Free(bp);
}

/*
 *  When LCP is reached to opened state, We'll start LQM activity.
 */

static void
lqr_Setup(struct lcp *lcp)
{
  struct physical *physical = link2physical(lcp->fsm.link);

  physical->hdlc.lqm.lqr.resent = 0;
  physical->hdlc.lqm.echo.seq_sent = 0;
  physical->hdlc.lqm.echo.seq_recv = 0;
  memset(&physical->hdlc.lqm.lqr.peer, '\0',
         sizeof physical->hdlc.lqm.lqr.peer);

  physical->hdlc.lqm.method = LQM_ECHO;
  if (IsEnabled(lcp->cfg.lqr) && !REJECTED(lcp, TY_QUALPROTO))
    physical->hdlc.lqm.method |= LQM_LQR;
  timer_Stop(&physical->hdlc.lqm.timer);

  physical->hdlc.lqm.lqr.peer_timeout = lcp->his_lqrperiod;
  if (lcp->his_lqrperiod)
    log_Printf(LogLQM, "%s: Expecting LQR every %d.%02d secs\n",
              physical->link.name, lcp->his_lqrperiod / 100,
              lcp->his_lqrperiod % 100);

  if (lcp->want_lqrperiod) {
    log_Printf(LogLQM, "%s: Will send %s every %d.%02d secs\n",
              physical->link.name,
              physical->hdlc.lqm.method & LQM_LQR ? "LQR" : "ECHO LQR",
              lcp->want_lqrperiod / 100, lcp->want_lqrperiod % 100);
    physical->hdlc.lqm.timer.load = lcp->want_lqrperiod * SECTICKS / 100;
    physical->hdlc.lqm.timer.func = SendLqrReport;
    physical->hdlc.lqm.timer.name = "lqm";
    physical->hdlc.lqm.timer.arg = lcp;
  } else {
    physical->hdlc.lqm.timer.load = 0;
    if (!lcp->his_lqrperiod)
      log_Printf(LogLQM, "%s: LQR/ECHO LQR not negotiated\n",
                 physical->link.name);
  }
}

void
lqr_Start(struct lcp *lcp)
{
  struct physical *p = link2physical(lcp->fsm.link);

  lqr_Setup(lcp);
  if (p->hdlc.lqm.timer.load)
    SendLqrReport(lcp);
}

void
lqr_reStart(struct lcp *lcp)
{
  struct physical *p = link2physical(lcp->fsm.link);

  lqr_Setup(lcp);
  if (p->hdlc.lqm.timer.load)
    timer_Start(&p->hdlc.lqm.timer);
}

void
lqr_StopTimer(struct physical *physical)
{
  timer_Stop(&physical->hdlc.lqm.timer);
}

void
lqr_Stop(struct physical *physical, int method)
{
  if (method == LQM_LQR)
    log_Printf(LogLQM, "%s: Stop sending LQR, Use LCP ECHO instead.\n",
               physical->link.name);
  if (method == LQM_ECHO)
    log_Printf(LogLQM, "%s: Stop sending LCP ECHO.\n",
               physical->link.name);
  physical->hdlc.lqm.method &= ~method;
  if (physical->hdlc.lqm.method)
    SendLqrReport(physical->hdlc.lqm.owner);
  else
    timer_Stop(&physical->hdlc.lqm.timer);
}

void
lqr_Dump(const char *link, const char *message, const struct lqrdata *lqr)
{
  if (log_IsKept(LogLQM)) {
    log_Printf(LogLQM, "%s: %s:\n", link, message);
    log_Printf(LogLQM, "  Magic:          %08x   LastOutLQRs:    %08x\n",
	      lqr->MagicNumber, lqr->LastOutLQRs);
    log_Printf(LogLQM, "  LastOutPackets: %08x   LastOutOctets:  %08x\n",
	      lqr->LastOutPackets, lqr->LastOutOctets);
    log_Printf(LogLQM, "  PeerInLQRs:     %08x   PeerInPackets:  %08x\n",
	      lqr->PeerInLQRs, lqr->PeerInPackets);
    log_Printf(LogLQM, "  PeerInDiscards: %08x   PeerInErrors:   %08x\n",
	      lqr->PeerInDiscards, lqr->PeerInErrors);
    log_Printf(LogLQM, "  PeerInOctets:   %08x   PeerOutLQRs:    %08x\n",
	      lqr->PeerInOctets, lqr->PeerOutLQRs);
    log_Printf(LogLQM, "  PeerOutPackets: %08x   PeerOutOctets:  %08x\n",
	      lqr->PeerOutPackets, lqr->PeerOutOctets);
  }
}

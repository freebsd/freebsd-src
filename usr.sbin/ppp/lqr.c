/*-
 * Copyright (c) 1996 - 2001 Brian Somers <brian@Awfulhak.org>
 *          based on work by Toshiharu OHNO <tony-o@iij.ad.jp>
 *                           Internet Initiative Japan, Inc (IIJ)
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

#include <sys/param.h>
#include <sys/un.h>

#include <string.h>
#include <termios.h>

#include "layer.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "acf.h"
#include "proto.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
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
#include "cbcp.h"
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
            (u_char *)&echo, sizeof echo, MB_ECHOOUT);
}

struct mbuf *
lqr_RecvEcho(struct fsm *fp, struct mbuf *bp)
{
  struct hdlc *hdlc = &link2physical(fp->link)->hdlc;
  struct lcp *lcp = fsm2lcp(fp);
  struct echolqr lqr;

  if (m_length(bp) >= sizeof lqr) {
    m_freem(mbuf_Read(bp, &lqr, sizeof lqr));
    bp = NULL;
    lqr.magic = ntohl(lqr.magic);
    lqr.signature = ntohl(lqr.signature);
    lqr.sequence = ntohl(lqr.sequence);

    /* Tolerate echo replies with either magic number */
    if (lqr.magic != 0 && lqr.magic != lcp->his_magic &&
        lqr.magic != lcp->want_magic) {
      log_Printf(LogWARN, "%s: lqr_RecvEcho: Bad magic: expected 0x%08x,"
                 " got 0x%08x\n", fp->link->name, lcp->his_magic, lqr.magic);
      /*
       * XXX: We should send a terminate request. But poor implementations may
       *      die as a result.
       */
    }
    if (lqr.signature == SIGNATURE) {
      /* careful not to update lqm.echo.seq_recv with older values */
      if ((hdlc->lqm.echo.seq_recv > (u_int32_t)0 - 5 && lqr.sequence < 5) ||
          (hdlc->lqm.echo.seq_recv <= (u_int32_t)0 - 5 &&
           lqr.sequence > hdlc->lqm.echo.seq_recv))
        hdlc->lqm.echo.seq_recv = lqr.sequence;
    } else
      log_Printf(LogWARN, "lqr_RecvEcho: Got sig 0x%08lx, not 0x%08lx !\n",
                (u_long)lqr.signature, (u_long)SIGNATURE);
  } else
    log_Printf(LogWARN, "lqr_RecvEcho: Got packet size %d, expecting %ld !\n",
              m_length(bp), (long)sizeof(struct echolqr));
  return bp;
}

void
lqr_ChangeOrder(struct lqrdata *src, struct lqrdata *dst)
{
  u_int32_t *sp, *dp;
  int n;

  sp = (u_int32_t *) src;
  dp = (u_int32_t *) dst;
  for (n = 0; n < sizeof(struct lqrdata) / sizeof(u_int32_t); n++, sp++, dp++)
    *dp = ntohl(*sp);
}

static void
SendLqrData(struct lcp *lcp)
{
  struct mbuf *bp;
  int extra;

  extra = proto_WrapperOctets(lcp, PROTO_LQR) +
          acf_WrapperOctets(lcp, PROTO_LQR);
  bp = m_get(sizeof(struct lqrdata) + extra, MB_LQROUT);
  bp->m_len -= extra;
  bp->m_offset += extra;
  link_PushPacket(lcp->fsm.link, bp, lcp->fsm.bundle,
                  LINK_QUEUES(lcp->fsm.link) - 1, PROTO_LQR);
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

struct mbuf *
lqr_Input(struct bundle *bundle, struct link *l, struct mbuf *bp)
{
  struct physical *p = link2physical(l);
  struct lcp *lcp = p->hdlc.lqm.owner;
  int len;

  if (p == NULL) {
    log_Printf(LogERROR, "lqr_Input: Not a physical link - dropped\n");
    m_freem(bp);
    return NULL;
  }

  p->hdlc.lqm.lqr.SaveInLQRs++;

  len = m_length(bp);
  if (len != sizeof(struct lqrdata))
    log_Printf(LogWARN, "lqr_Input: Got packet size %d, expecting %ld !\n",
              len, (long)sizeof(struct lqrdata));
  else if (!IsAccepted(l->lcp.cfg.lqr) && !(p->hdlc.lqm.method & LQM_LQR)) {
    bp = m_pullup(proto_Prepend(bp, PROTO_LQR, 0, 0));
    lcp_SendProtoRej(lcp, MBUF_CTOP(bp), bp->m_len);
  } else {
    struct lqrdata *lqr;
    u_int32_t lastLQR;

    bp = m_pullup(bp);
    lqr = (struct lqrdata *)MBUF_CTOP(bp);
    if (ntohl(lqr->MagicNumber) != lcp->his_magic)
      log_Printf(LogWARN, "lqr_Input: magic 0x%08lx is wrong,"
                 " expecting 0x%08lx\n",
		 (u_long)ntohl(lqr->MagicNumber), (u_long)lcp->his_magic);
    else {
      /*
       * Remember our PeerInLQRs, then convert byte order and save
       */
      lastLQR = p->hdlc.lqm.lqr.peer.PeerInLQRs;

      lqr_ChangeOrder(lqr, &p->hdlc.lqm.lqr.peer);
      lqr_Dump(l->name, "Input", &p->hdlc.lqm.lqr.peer);
      /* we have received an LQR from peer */
      p->hdlc.lqm.lqr.resent = 0;

      /*
       * Generate an LQR response if we're not running an LQR timer OR
       * two successive LQR's PeerInLQRs are the same OR we're not going to
       * send our next one before the peers max timeout.
       */
      if (p->hdlc.lqm.timer.load == 0 ||
          !(p->hdlc.lqm.method & LQM_LQR) ||
          (lastLQR && lastLQR == p->hdlc.lqm.lqr.peer.PeerInLQRs) ||
          (p->hdlc.lqm.lqr.peer_timeout && 
           p->hdlc.lqm.timer.rest * 100 / SECTICKS >
           p->hdlc.lqm.lqr.peer_timeout))
        SendLqrData(lcp);
    }
  }
  m_freem(bp);
  return NULL;
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

static struct mbuf *
lqr_LayerPush(struct bundle *b, struct link *l, struct mbuf *bp,
              int pri, u_short *proto)
{
  struct physical *p = link2physical(l);
  int len;

  if (!p) {
    /* Oops - can't happen :-] */
    m_freem(bp);
    return NULL;
  }

  /*
   * From rfc1989:
   *
   *  All octets which are included in the FCS calculation MUST be counted,
   *  including the packet header, the information field, and any padding.
   *  The FCS octets MUST also be counted, and one flag octet per frame
   *  MUST be counted.  All other octets (such as additional flag
   *  sequences, and escape bits or octets) MUST NOT be counted.
   *
   * As we're stacked before the HDLC layer (otherwise HDLC wouldn't be
   * able to calculate the FCS), we must not forget about these additional
   * bytes when we're asynchronous.
   *
   * We're also expecting to be stacked *before* the proto and acf layers.
   * If we were after these, it makes alignment more of a pain, and we
   * don't do LQR without these layers.
   */

  bp = m_pullup(bp);
  len = m_length(bp);

  if (!physical_IsSync(p))
    p->hdlc.lqm.OutOctets += hdlc_WrapperOctets(&l->lcp, *proto);
  p->hdlc.lqm.OutOctets += acf_WrapperOctets(&l->lcp, *proto) +
                           proto_WrapperOctets(&l->lcp, *proto) + len + 1;
  p->hdlc.lqm.OutPackets++;

  if (*proto == PROTO_LQR) {
    /* Overwrite the entire packet (created in SendLqrData()) */
    struct lqrdata lqr;

    lqr.MagicNumber = p->link.lcp.want_magic;
    lqr.LastOutLQRs = p->hdlc.lqm.lqr.peer.PeerOutLQRs;
    lqr.LastOutPackets = p->hdlc.lqm.lqr.peer.PeerOutPackets;
    lqr.LastOutOctets = p->hdlc.lqm.lqr.peer.PeerOutOctets;
    lqr.PeerInLQRs = p->hdlc.lqm.lqr.SaveInLQRs;
    lqr.PeerInPackets = p->hdlc.lqm.SaveInPackets;
    lqr.PeerInDiscards = p->hdlc.lqm.SaveInDiscards;
    lqr.PeerInErrors = p->hdlc.lqm.SaveInErrors;
    lqr.PeerInOctets = p->hdlc.lqm.SaveInOctets;
    lqr.PeerOutPackets = p->hdlc.lqm.OutPackets;
    lqr.PeerOutOctets = p->hdlc.lqm.OutOctets;
    if (p->hdlc.lqm.lqr.peer.LastOutLQRs == p->hdlc.lqm.lqr.OutLQRs) {
      /*
       * only increment if it's the first time or we've got a reply
       * from the last one
       */
      lqr.PeerOutLQRs = ++p->hdlc.lqm.lqr.OutLQRs;
      lqr_Dump(l->name, "Output", &lqr);
    } else {
      lqr.PeerOutLQRs = p->hdlc.lqm.lqr.OutLQRs;
      lqr_Dump(l->name, "Output (again)", &lqr);
    }
    lqr_ChangeOrder(&lqr, (struct lqrdata *)MBUF_CTOP(bp));
  }

  return bp;
}

static struct mbuf *
lqr_LayerPull(struct bundle *b, struct link *l, struct mbuf *bp, u_short *proto)
{
  /*
   * We mark the packet as ours but don't do anything 'till it's dispatched
   * to lqr_Input()
   */
  if (*proto == PROTO_LQR)
    m_settype(bp, MB_LQRIN);
  return bp;
}

/*
 * Statistics for pulled packets are recorded either in hdlc_PullPacket()
 * or sync_PullPacket()
 */

struct layer lqrlayer = { LAYER_LQR, "lqr", lqr_LayerPush, lqr_LayerPull };

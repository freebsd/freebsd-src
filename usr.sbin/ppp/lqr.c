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
 * $Id: lqr.c,v 1.21 1998/01/11 17:50:40 brian Exp $
 *
 *	o LQR based on RFC1333
 *
 * TODO:
 *	o LQM policy
 *	o Allow user to configure LQM method and interval.
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "loadalias.h"
#include "vars.h"

struct lqrdata MyLqrData, HisLqrData;
struct lqrsave HisLqrSave;

static struct pppTimer LqrTimer;

static u_long lastpeerin = (u_long) - 1;

static int lqmmethod;
static u_int32_t echoseq;
static u_int32_t gotseq;
static int lqrsendcnt;

struct echolqr {
  u_int32_t magic;
  u_int32_t signature;
  u_int32_t sequence;
};

#define	SIGNATURE  0x594e4f54

static void
SendEchoReq(void)
{
  struct fsm *fp = &LcpFsm;
  struct echolqr *lqr, lqrdata;

  if (fp->state == ST_OPENED) {
    lqr = &lqrdata;
    lqr->magic = htonl(LcpInfo.want_magic);
    lqr->signature = htonl(SIGNATURE);
    LogPrintf(LogLQM, "Send echo LQR [%d]\n", echoseq);
    lqr->sequence = htonl(echoseq++);
    FsmOutput(fp, CODE_ECHOREQ, fp->reqid++,
	      (u_char *) lqr, sizeof(struct echolqr));
  }
}

void
RecvEchoLqr(struct mbuf * bp)
{
  struct echolqr *lqr;
  u_int32_t seq;

  if (plength(bp) == sizeof(struct echolqr)) {
    lqr = (struct echolqr *) MBUF_CTOP(bp);
    if (htonl(lqr->signature) == SIGNATURE) {
      seq = ntohl(lqr->sequence);
      LogPrintf(LogLQM, "Got echo LQR [%d]\n", ntohl(lqr->sequence));
      gotseq = seq;
    }
  }
}

void
LqrChangeOrder(struct lqrdata * src, struct lqrdata * dst)
{
  u_long *sp, *dp;
  int n;

  sp = (u_long *) src;
  dp = (u_long *) dst;
  for (n = 0; n < sizeof(struct lqrdata) / sizeof(u_long); n++)
    *dp++ = ntohl(*sp++);
}

static void
SendLqrReport(void *v)
{
  struct mbuf *bp;

  StopTimer(&LqrTimer);

  if (lqmmethod & LQM_LQR) {
    if (lqrsendcnt > 5) {

      /*
       * XXX: Should implement LQM strategy
       */
      LogPrintf(LogPHASE, "** 1 Too many ECHO packets are lost. **\n");
      lqmmethod = 0;		/* Prevent rcursion via LcpClose() */
      reconnect(RECON_TRUE);
      LcpClose();
    } else {
      bp = mballoc(sizeof(struct lqrdata), MB_LQR);
      HdlcOutput(PRI_LINK, PROTO_LQR, bp);
      lqrsendcnt++;
    }
  } else if (lqmmethod & LQM_ECHO) {
    if (echoseq - gotseq > 5) {
      LogPrintf(LogPHASE, "** 2 Too many ECHO packets are lost. **\n");
      lqmmethod = 0;		/* Prevent rcursion via LcpClose() */
      reconnect(RECON_TRUE);
      LcpClose();
    } else
      SendEchoReq();
  }
  if (lqmmethod && Enabled(ConfLqr))
    StartTimer(&LqrTimer);
}

void
LqrInput(struct mbuf * bp)
{
  int len;
  u_char *cp;
  struct lqrdata *lqr;

  len = plength(bp);
  if (len != sizeof(struct lqrdata)) {
    pfree(bp);
    return;
  }
  if (!Acceptable(ConfLqr)) {
    bp->offset -= 2;
    bp->cnt += 2;

    cp = MBUF_CTOP(bp);
    LcpSendProtoRej(cp, bp->cnt);
  } else {
    cp = MBUF_CTOP(bp);
    lqr = (struct lqrdata *) cp;
    if (ntohl(lqr->MagicNumber) != LcpInfo.his_magic) {
      LogPrintf(LogERROR, "LqrInput: magic %x != expecting %x\n",
		ntohl(lqr->MagicNumber), LcpInfo.his_magic);
      pfree(bp);
      return;
    }

    /*
     * Convert byte order and save into our strage
     */
    LqrChangeOrder(lqr, &HisLqrData);
    LqrDump("LqrInput", &HisLqrData);
    lqrsendcnt = 0;		/* we have received LQR from peer */

    /*
     * Generate LQR responce to peer, if i) We are not running LQR timer. ii)
     * Two successive LQR's PeerInLQRs are same.
     */
    if (LqrTimer.load == 0 || lastpeerin == HisLqrData.PeerInLQRs) {
      lqmmethod |= LQM_LQR;
      SendLqrReport(0);
    }
    lastpeerin = HisLqrData.PeerInLQRs;
  }
  pfree(bp);
}

/*
 *  When LCP is reached to opened state, We'll start LQM activity.
 */
void
StartLqm()
{
  struct lcpstate *lcp = &LcpInfo;
  int period;

  lqrsendcnt = 0;		/* start waiting all over for ECHOs */
  echoseq = 0;
  gotseq = 0;

  lqmmethod = LQM_ECHO;
  if (Enabled(ConfLqr))
    lqmmethod |= LQM_LQR;
  StopTimer(&LqrTimer);
  LogPrintf(LogLQM, "LQM method = %d\n", lqmmethod);

  if (lcp->his_lqrperiod || lcp->want_lqrperiod) {

    /*
     * We need to run timer. Let's figure out period.
     */
    period = lcp->his_lqrperiod ? lcp->his_lqrperiod : lcp->want_lqrperiod;
    StopTimer(&LqrTimer);
    LqrTimer.state = TIMER_STOPPED;
    LqrTimer.load = period * SECTICKS / 100;
    LqrTimer.func = SendLqrReport;
    SendLqrReport(0);
    StartTimer(&LqrTimer);
    LogPrintf(LogLQM, "Will send LQR every %d.%d secs\n",
	      period / 100, period % 100);
  } else {
    LogPrintf(LogLQM, "LQR is not activated.\n");
  }
}

void
StopLqrTimer()
{
  StopTimer(&LqrTimer);
}

void
StopLqr(int method)
{
  LogPrintf(LogLQM, "StopLqr method = %x\n", method);

  if (method == LQM_LQR)
    LogPrintf(LogLQM, "Stop sending LQR, Use LCP ECHO instead.\n");
  if (method == LQM_ECHO)
    LogPrintf(LogLQM, "Stop sending LCP ECHO.\n");
  lqmmethod &= ~method;
  if (lqmmethod)
    SendLqrReport(0);
  else
    StopTimer(&LqrTimer);
}

void
LqrDump(const char *message, const struct lqrdata * lqr)
{
  if (LogIsKept(LogLQM)) {
    LogPrintf(LogLQM, "%s:\n", message);
    LogPrintf(LogLQM, "  Magic:          %08x   LastOutLQRs:    %08x\n",
	      lqr->MagicNumber, lqr->LastOutLQRs);
    LogPrintf(LogLQM, "  LastOutPackets: %08x   LastOutOctets:  %08x\n",
	      lqr->LastOutPackets, lqr->LastOutOctets);
    LogPrintf(LogLQM, "  PeerInLQRs:     %08x   PeerInPackets:  %08x\n",
	      lqr->PeerInLQRs, lqr->PeerInPackets);
    LogPrintf(LogLQM, "  PeerInDiscards: %08x   PeerInErrors:   %08x\n",
	      lqr->PeerInDiscards, lqr->PeerInErrors);
    LogPrintf(LogLQM, "  PeerInOctets:   %08x   PeerOutLQRs:    %08x\n",
	      lqr->PeerInOctets, lqr->PeerOutLQRs);
    LogPrintf(LogLQM, "  PeerOutPackets: %08x   PeerOutOctets:  %08x\n",
	      lqr->PeerOutPackets, lqr->PeerOutOctets);
  }
}

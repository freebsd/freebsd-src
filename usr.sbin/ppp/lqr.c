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
 * $Id: lqr.c,v 1.7.2.2 1997/05/19 02:02:22 brian Exp $
 *
 *	o LQR based on RFC1333
 *
 * TODO:
 *	o LQM policy
 *	o Allow user to configure LQM method and interval.
 */
#include "fsm.h"
#include "lcpproto.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "vars.h"
#include "main.h"

struct pppTimer LqrTimer;

static u_long lastpeerin = (u_long)-1;

static int lqmmethod;
static int echoseq;
static int gotseq;
static int lqrsendcnt;

struct echolqr {
  u_long magic;
  u_long signature;
  u_long sequence;
};

#define	SIGNATURE  0x594e4f54

static void
SendEchoReq()
{
  struct fsm *fp = &LcpFsm;
  struct echolqr *lqr, lqrdata;

  if (fp->state == ST_OPENED) {
    lqr = &lqrdata;
    lqr->magic = htonl(LcpInfo.want_magic);
    lqr->signature = htonl(SIGNATURE);
    LogPrintf(LOG_LQM_BIT, "Send echo LQR [%d]\n", echoseq);
    lqr->sequence = htonl(echoseq++);
    FsmOutput(fp, CODE_ECHOREQ, fp->reqid++,
	      (u_char *)lqr, sizeof(struct echolqr));
  }
}

void
RecvEchoLqr(bp)
struct mbuf *bp;
{
  struct echolqr *lqr;
  u_long seq;

  if (plength(bp) == sizeof(struct echolqr)) {
    lqr = (struct echolqr *)MBUF_CTOP(bp);
    if (htonl(lqr->signature) == SIGNATURE) {
      seq = ntohl(lqr->sequence);
      LogPrintf(LOG_LQM_BIT, "Got echo LQR [%d]\n", ntohl(lqr->sequence));
      gotseq = seq;
    }
  }
}

void
LqrChangeOrder(src, dst)
struct lqrdata *src, *dst;
{
  u_long *sp, *dp;
  int n;

  sp = (u_long *)src; dp = (u_long *)dst;
  for (n = 0; n < sizeof(struct lqrdata)/sizeof(u_long); n++)
    *dp++ = ntohl(*sp++);
}

static void
SendLqrReport()
{
  struct mbuf *bp;

  StopTimer(&LqrTimer);

  if (lqmmethod & LQM_LQR) {
    if (lqrsendcnt > 5) {
      /*
       * XXX: Should implement LQM strategy
       */
      LogPrintf(LOG_PHASE_BIT, "** 1 Too many ECHO packets are lost. **\n");
      lqmmethod = 0;   /* Prevent rcursion via LcpClose() */
      reconnect(RECON_TRUE);
      LcpClose();
    } else {
      bp = mballoc(sizeof(struct lqrdata), MB_LQR);
      HdlcOutput(PRI_LINK, PROTO_LQR, bp);
      lqrsendcnt++;
    }
  } else if (lqmmethod & LQM_ECHO) {
    if (echoseq - gotseq > 5) {
      LogPrintf(LOG_PHASE_BIT, "** 2 Too many ECHO packets are lost. **\n");
      lqmmethod = 0;   /* Prevent rcursion via LcpClose() */
      reconnect(RECON_TRUE);
      LcpClose();
    } else
      SendEchoReq();
  }

  if (lqmmethod && Enabled(ConfLqr))
    StartTimer(&LqrTimer);
}

void
LqrInput(struct mbuf *bp)
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
    lqr = (struct lqrdata *)cp;
    if (ntohl(lqr->MagicNumber) != LcpInfo.his_magic) {
#ifdef notdef
logprintf("*** magic %x != expecting %x\n", ntohl(lqr->MagicNumber), LcpInfo.his_magic);
#endif
      pfree(bp);
      return;
    }

    /*
     * Convert byte order and save into our strage
     */
    LqrChangeOrder(lqr, &HisLqrData);
    LqrDump("LqrInput", &HisLqrData);
    lqrsendcnt = 0;	/* we have received LQR from peer */

    /*
     *  Generate LQR responce to peer, if
     *    i) We are not running LQR timer.
     *   ii) Two successive LQR's PeerInLQRs are same.
     */
    if (LqrTimer.load == 0 || lastpeerin == HisLqrData.PeerInLQRs) {
      lqmmethod |= LQM_LQR;
      SendLqrReport();
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

  lqrsendcnt = 0;	/* start waiting all over for ECHOs */
  lqmmethod = LQM_ECHO;
  if (Enabled(ConfLqr))
    lqmmethod |= LQM_LQR;
  StopTimer(&LqrTimer);
  LogPrintf(LOG_LQM_BIT, "LQM method = %d\n", lqmmethod);

  if (lcp->his_lqrperiod || lcp->want_lqrperiod) {
    /*
     *  We need to run timer. Let's figure out period.
     */
    period = lcp->his_lqrperiod ? lcp->his_lqrperiod : lcp->want_lqrperiod;
    StopTimer(&LqrTimer);
    LqrTimer.state = TIMER_STOPPED;
    LqrTimer.load = period * SECTICKS / 100;
    LqrTimer.func = SendLqrReport;
    SendLqrReport();
    StartTimer(&LqrTimer);
    LogPrintf(LOG_LQM_BIT, "Will send LQR every %d.%d secs\n",
	      period/100, period % 100);
  } else {
    LogPrintf(LOG_LQM_BIT, "LQR is not activated.\n");
  }
}

void
StopLqrTimer(void)
{
    StopTimer(&LqrTimer);
}

void
StopLqr(method)
int method;
{
  LogPrintf(LOG_LQM_BIT, "StopLqr method = %x\n", method);

  if (method == LQM_LQR)
    LogPrintf(LOG_LQM_BIT, "Stop sending LQR, Use LCP ECHO instead.\n");
  if (method == LQM_ECHO)
    LogPrintf(LOG_LQM_BIT, "Stop sending LCP ECHO.\n");
  lqmmethod &= ~method;
  if (lqmmethod)
    SendLqrReport();
  else
    StopTimer(&LqrTimer);
}

void
LqrDump(message, lqr)
char *message;
struct lqrdata *lqr;
{
  if (loglevel & (1 << LOG_LQM)) {
    LogTimeStamp();
    logprintf("%s:\n", message);
    LogTimeStamp();
    logprintf("  Magic:          %08x   LastOutLQRs:    %08x\n",
	lqr->MagicNumber, lqr->LastOutLQRs);
    LogTimeStamp();
    logprintf("  LastOutPackets: %08x   LastOutOctets:  %08x\n",
	lqr->LastOutPackets, lqr->LastOutOctets);
    LogTimeStamp();
    logprintf("  PeerInLQRs:     %08x   PeerInPackets:  %08x\n",
	lqr->PeerInLQRs, lqr->PeerInPackets);
    LogTimeStamp();
    logprintf("  PeerInDiscards: %08x   PeerInErrors:   %08x\n",
	lqr->PeerInDiscards, lqr->PeerInErrors);
    LogTimeStamp();
    logprintf("  PeerInOctets:   %08x   PeerOutLQRs:    %08x\n",
	lqr->PeerInOctets, lqr->PeerOutLQRs);
    LogTimeStamp();
    logprintf("  PeerOutPackets: %08x   PeerOutOctets:  %08x\n",
	lqr->PeerOutPackets, lqr->PeerOutOctets);
  }
}

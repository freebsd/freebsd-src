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
 * $Id: lqr.c,v 1.7.2.9 1998/01/26 20:04:55 brian Exp $
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
#include <string.h>

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
      /* careful not to update gotseq with older values */
      if ((gotseq > (u_int32_t)0 - 5 && seq < 5) ||
          (gotseq <= (u_int32_t)0 - 5 && seq > gotseq))
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
  for (n = 0; n < sizeof(struct lqrdata) / sizeof(u_int32_t); n++)
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
      LogPrintf(LogPHASE, "** Too many LQR packets lost **\n");
      LogPrintf(LogLQM, "LqrOutput: Too many LQR packets lost\n");
      lqmmethod = 0;		/* Prevent recursion via LcpClose() */
      reconnect(RECON_TRUE);
      LcpClose();
    } else {
      bp = mballoc(sizeof(struct lqrdata), MB_LQR);
      HdlcOutput(PRI_LINK, PROTO_LQR, bp);
      lqrsendcnt++;
    }
  } else if (lqmmethod & LQM_ECHO) {
    if ((echoseq > 5 && echoseq - 5 > gotseq) ||
        (echoseq <= 5 && echoseq > gotseq + 5)) {
      LogPrintf(LogPHASE, "** Too many ECHO LQR packets lost **\n");
      LogPrintf(LogLQM, "LqrOutput: Too many ECHO LQR packets lost\n");
      lqmmethod = 0;		/* Prevent recursion via LcpClose() */
      reconnect(RECON_TRUE);
      LcpClose();
    } else
      SendEchoReq();
  }
  if (lqmmethod && LqrTimer.load)
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
     * Generate an LQR response to peer we're not running LQR timer OR
     * two successive LQR's PeerInLQRs are same OR we're not going to
     * send our next one before the peers max timeout.
     */
    if (LqrTimer.load == 0 || lastpeerin == HisLqrData.PeerInLQRs ||
        (LqrTimer.arg &&
         LqrTimer.rest * 100 / SECTICKS > (u_int32_t)LqrTimer.arg)) {
      lqmmethod |= LQM_LQR;
      SendLqrReport(LqrTimer.arg);
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

  lqrsendcnt = 0;		/* start waiting all over for ECHOs */
  echoseq = 0;
  gotseq = 0;
  memset(&HisLqrData, '\0', sizeof HisLqrData);

  lqmmethod = LQM_ECHO;
  if (Enabled(ConfLqr) && !REJECTED(lcp, TY_QUALPROTO))
    lqmmethod |= LQM_LQR;
  StopTimer(&LqrTimer);

  if (lcp->his_lqrperiod)
    LogPrintf(LogLQM, "Expecting LQR every %d.%02d secs\n",
	      lcp->his_lqrperiod / 100, lcp->his_lqrperiod % 100);

  if (lcp->want_lqrperiod) {
    LogPrintf(LogLQM, "Will send %s every %d.%02d secs\n",
              lqmmethod & LQM_LQR ? "LQR" : "ECHO LQR",
	      lcp->want_lqrperiod / 100, lcp->want_lqrperiod % 100);
    LqrTimer.state = TIMER_STOPPED;
    LqrTimer.load = lcp->want_lqrperiod * SECTICKS / 100;
    LqrTimer.func = SendLqrReport;
    LqrTimer.arg = (void *)lcp->his_lqrperiod;
    SendLqrReport(LqrTimer.arg);
  } else {
    LqrTimer.load = 0;
    if (!lcp->his_lqrperiod)
      LogPrintf(LogLQM, "LQR/ECHO LQR not negotiated\n");
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
    SendLqrReport(LqrTimer.arg);
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

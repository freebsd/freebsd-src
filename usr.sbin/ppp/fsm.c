/*
 *		PPP Finite State Machine for LCP/IPCP
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
 * $Id: fsm.c,v 1.27.2.13 1998/02/21 01:45:07 brian Exp $
 *
 *  TODO:
 *		o Refer loglevel for log output
 *		o Better option log display
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "hdlc.h"
#include "lqr.h"
#include "lcpproto.h"
#include "lcp.h"
#include "ccp.h"
#include "modem.h"
#include "loadalias.h"
#include "vars.h"
#include "throughput.h"
#include "async.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "bundle.h"

u_char AckBuff[200];
u_char NakBuff[200];
u_char RejBuff[100];
u_char ReqBuff[200];
u_char *ackp = NULL;
u_char *nakp = NULL;
u_char *rejp = NULL;

static void FsmSendConfigReq(struct fsm *);
static void FsmSendTerminateReq(struct fsm *);
static void FsmInitRestartCounter(struct fsm *);

char const *StateNames[] = {
  "Initial", "Starting", "Closed", "Stopped", "Closing", "Stopping",
  "Req-Sent", "Ack-Rcvd", "Ack-Sent", "Opened",
};

static void
StoppedTimeout(void *v)
{
  struct fsm *fp = (struct fsm *)v;

  LogPrintf(fp->LogLevel, "Stopped timer expired\n");
  if (fp->OpenTimer.state == TIMER_RUNNING) {
    LogPrintf(LogWARN, "%s: aborting open delay due to stopped timer\n",
              fp->name);
    StopTimer(&fp->OpenTimer);
  }
  if (link_IsActive(fp->link))
    link_Close(fp->link, fp->bundle, 0, 1);
}

void
fsm_Init(struct fsm *fp, const char *name, u_short proto, int maxcode,
         int maxcfg, int LogLevel, struct bundle *bundle, struct link *l,
         struct fsm_callbacks *fn)
{
  fp->name = name;
  fp->proto = proto;
  fp->max_code = maxcode;
  fp->state = ST_INITIAL;
  fp->reqid = 1;
  fp->restart = 1;
  fp->maxconfig = maxcfg;
  memset(&fp->FsmTimer, '\0', sizeof fp->FsmTimer);
  memset(&fp->OpenTimer, '\0', sizeof fp->OpenTimer);
  memset(&fp->StoppedTimer, '\0', sizeof fp->StoppedTimer);
  fp->LogLevel = LogLevel;
  fp->link = l;
  fp->bundle = bundle;
  fp->fn = fn;
}

static void
NewState(struct fsm * fp, int new)
{
  LogPrintf(fp->LogLevel, "State change %s --> %s\n",
	    StateNames[fp->state], StateNames[new]);
  if (fp->state == ST_STOPPED && fp->StoppedTimer.state == TIMER_RUNNING)
    StopTimer(&fp->StoppedTimer);
  fp->state = new;
  if ((new >= ST_INITIAL && new <= ST_STOPPED) || (new == ST_OPENED)) {
    StopTimer(&fp->FsmTimer);
    if (new == ST_STOPPED && fp->StoppedTimer.load) {
      fp->StoppedTimer.state = TIMER_STOPPED;
      fp->StoppedTimer.func = StoppedTimeout;
      fp->StoppedTimer.arg = (void *) fp;
      StartTimer(&fp->StoppedTimer);
    }
  }
}

void
FsmOutput(struct fsm * fp, u_int code, u_int id, u_char * ptr, int count)
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  plen = sizeof(struct fsmheader) + count;
  lh.code = code;
  lh.id = id;
  lh.length = htons(plen);
  bp = mballoc(plen, MB_FSM);
  memcpy(MBUF_CTOP(bp), &lh, sizeof(struct fsmheader));
  if (count)
    memcpy(MBUF_CTOP(bp) + sizeof(struct fsmheader), ptr, count);
  LogDumpBp(LogDEBUG, "FsmOutput", bp);
  HdlcOutput(fp->link, PRI_LINK, fp->proto, bp);
}

static void
FsmOpenNow(void *v)
{
  struct fsm *fp = (struct fsm *)v;

  StopTimer(&fp->OpenTimer);
  if (fp->state <= ST_STOPPED) {
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
  }
}

void
FsmOpen(struct fsm * fp)
{
  switch (fp->state) {
  case ST_INITIAL:
    NewState(fp, ST_STARTING);
    (*fp->fn->LayerStart)(fp);
    bundle_LayerStart(fp->bundle, fp);
    break;
  case ST_CLOSED:
    if (fp->open_mode == OPEN_PASSIVE) {
      NewState(fp, ST_STOPPED);
    } else if (fp->open_mode > 0) {
      if (fp->open_mode > 1)
        LogPrintf(LogPHASE, "Entering STOPPED state for %d seconds\n",
                  fp->open_mode);
      NewState(fp, ST_STOPPED);
      fp->OpenTimer.state = TIMER_STOPPED;
      fp->OpenTimer.load = fp->open_mode * SECTICKS;
      fp->OpenTimer.func = FsmOpenNow;
      fp->OpenTimer.arg = (void *)fp;
      StartTimer(&fp->OpenTimer);
    } else
      FsmOpenNow(fp);
    break;
  case ST_STOPPED:		/* XXX: restart option */
  case ST_REQSENT:
  case ST_ACKRCVD:
  case ST_ACKSENT:
  case ST_OPENED:		/* XXX: restart option */
    break;
  case ST_CLOSING:		/* XXX: restart option */
  case ST_STOPPING:		/* XXX: restart option */
    NewState(fp, ST_STOPPING);
    break;
  }
}

void
FsmUp(struct fsm * fp)
{
  switch (fp->state) {
    case ST_INITIAL:
    NewState(fp, ST_CLOSED);
    break;
  case ST_STARTING:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  default:
    LogPrintf(fp->LogLevel, "Oops, Up at %s\n", StateNames[fp->state]);
    break;
  }
}

void
FsmDown(struct fsm *fp)
{
  switch (fp->state) {
  case ST_CLOSED:
    NewState(fp, ST_INITIAL);
    break;
  case ST_CLOSING:
    (*fp->fn->LayerFinish)(fp);
    NewState(fp, ST_INITIAL);
    bundle_LayerFinish(fp->bundle, fp);
    break;
  case ST_STOPPED:
    NewState(fp, ST_STARTING);
    (*fp->fn->LayerStart)(fp);
    bundle_LayerStart(fp->bundle, fp);
    break;
  case ST_STOPPING:
  case ST_REQSENT:
  case ST_ACKRCVD:
  case ST_ACKSENT:
    NewState(fp, ST_STARTING);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    NewState(fp, ST_STARTING);
    bundle_LayerDown(fp->bundle, fp);
    break;
  }
}

void
FsmClose(struct fsm *fp)
{
  switch (fp->state) {
  case ST_STARTING:
    (*fp->fn->LayerFinish)(fp);
    NewState(fp, ST_INITIAL);
    bundle_LayerFinish(fp->bundle, fp);
    break;
  case ST_STOPPED:
    NewState(fp, ST_CLOSED);
    break;
  case ST_STOPPING:
    NewState(fp, ST_CLOSING);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmInitRestartCounter(fp);
    FsmSendTerminateReq(fp);
    NewState(fp, ST_CLOSING);
    bundle_LayerDown(fp->bundle, fp);
    break;
  case ST_REQSENT:
  case ST_ACKRCVD:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendTerminateReq(fp);
    NewState(fp, ST_CLOSING);
    break;
  }
}

/*
 *	Send functions
 */
static void
FsmSendConfigReq(struct fsm * fp)
{
  if (--fp->maxconfig > 0) {
    (*fp->fn->SendConfigReq)(fp);
    StartTimer(&fp->FsmTimer);	/* Start restart timer */
    fp->restart--;		/* Decrement restart counter */
  } else {
    FsmClose(fp);
  }
}

static void
FsmSendTerminateReq(struct fsm * fp)
{
  LogPrintf(fp->LogLevel, "SendTerminateReq.\n");
  FsmOutput(fp, CODE_TERMREQ, fp->reqid++, NULL, 0);
  (*fp->fn->SendTerminateReq)(fp);
  StartTimer(&fp->FsmTimer);	/* Start restart timer */
  fp->restart--;		/* Decrement restart counter */
}

static void
FsmSendConfigAck(struct fsm *fp, struct fsmheader *lhp,
		 u_char *option, int count)
{
  LogPrintf(fp->LogLevel, "SendConfigAck(%s)\n", StateNames[fp->state]);
  (*fp->fn->DecodeConfig)(fp, option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGACK, lhp->id, option, count);
}

static void
FsmSendConfigRej(struct fsm *fp, struct fsmheader *lhp,
		 u_char *option, int count)
{
  LogPrintf(fp->LogLevel, "SendConfigRej(%s)\n", StateNames[fp->state]);
  (*fp->fn->DecodeConfig)(fp, option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGREJ, lhp->id, option, count);
}

static void
FsmSendConfigNak(struct fsm *fp, struct fsmheader *lhp,
		 u_char *option, int count)
{
  LogPrintf(fp->LogLevel, "SendConfigNak(%s)\n", StateNames[fp->state]);
  (*fp->fn->DecodeConfig)(fp, option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGNAK, lhp->id, option, count);
}

/*
 *	Timeout actions
 */
static void
FsmTimeout(void *v)
{
  struct fsm *fp = (struct fsm *)v;

  if (fp->restart) {
    switch (fp->state) {
    case ST_CLOSING:
    case ST_STOPPING:
      FsmSendTerminateReq(fp);
      break;
    case ST_REQSENT:
    case ST_ACKSENT:
      FsmSendConfigReq(fp);
      break;
    case ST_ACKRCVD:
      FsmSendConfigReq(fp);
      NewState(fp, ST_REQSENT);
      break;
    }
    StartTimer(&fp->FsmTimer);
  } else {
    switch (fp->state) {
    case ST_CLOSING:
      (*fp->fn->LayerFinish)(fp);
      NewState(fp, ST_CLOSED);
      bundle_LayerFinish(fp->bundle, fp);
      break;
    case ST_STOPPING:
      (*fp->fn->LayerFinish)(fp);
      NewState(fp, ST_STOPPED);
      bundle_LayerFinish(fp->bundle, fp);
      break;
    case ST_REQSENT:		/* XXX: 3p */
    case ST_ACKSENT:
    case ST_ACKRCVD:
      (*fp->fn->LayerFinish)(fp);
      NewState(fp, ST_STOPPED);
      bundle_LayerFinish(fp->bundle, fp);
      break;
    }
  }
}

static void
FsmInitRestartCounter(struct fsm * fp)
{
  StopTimer(&fp->FsmTimer);
  fp->FsmTimer.state = TIMER_STOPPED;
  fp->FsmTimer.func = FsmTimeout;
  fp->FsmTimer.arg = (void *) fp;
  (*fp->fn->InitRestartCounter)(fp);
}

/*
 *   Actions when receive packets
 */
static void
FsmRecvConfigReq(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RCR */
{
  int plen, flen;
  int ackaction = 0;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof *lhp;
  if (plen < flen) {
    LogPrintf(LogERROR, "FsmRecvConfigReq: plen (%d) < flen (%d)\n",
	      plen, flen);
    pfree(bp);
    return;
  }

  /*
   * Check and process easy case
   */
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(fp->LogLevel, "Oops, RCR in %s.\n", StateNames[fp->state]);
    pfree(bp);
    return;
  case ST_CLOSED:
    (*fp->fn->SendTerminateAck)(fp);
    pfree(bp);
    return;
  case ST_CLOSING:
    LogPrintf(fp->LogLevel, "Error: Got ConfigReq while state = %d\n",
              fp->state);
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (*fp->fn->DecodeConfig)(fp, MBUF_CTOP(bp), flen, MODE_REQ);

  if (nakp == NakBuff && rejp == RejBuff)
    ackaction = 1;

  switch (fp->state) {
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    bundle_LayerDown(fp->bundle, fp);
    break;
  case ST_STOPPED:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  }

  if (rejp != RejBuff)
    FsmSendConfigRej(fp, lhp, RejBuff, rejp - RejBuff);
  if (nakp != NakBuff)
    FsmSendConfigNak(fp, lhp, NakBuff, nakp - NakBuff);
  if (ackaction)
    FsmSendConfigAck(fp, lhp, AckBuff, ackp - AckBuff);

  switch (fp->state) {
  case ST_OPENED:
  case ST_STOPPED:
    if (ackaction)
      NewState(fp, ST_ACKSENT);
    else
      NewState(fp, ST_REQSENT);
    break;
  case ST_REQSENT:
    if (ackaction)
      NewState(fp, ST_ACKSENT);
    break;
  case ST_ACKRCVD:
    if (ackaction) {
      NewState(fp, ST_OPENED);
      (*fp->fn->LayerUp)(fp);
      bundle_LayerUp(fp->bundle, fp);
    }
    break;
  case ST_ACKSENT:
    if (!ackaction)
      NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

static void
FsmRecvConfigAck(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RCA */
{
  switch (fp->state) {
    case ST_CLOSED:
    case ST_STOPPED:
    (*fp->fn->SendTerminateAck)(fp);
    break;
  case ST_CLOSING:
  case ST_STOPPING:
    break;
  case ST_REQSENT:
    FsmInitRestartCounter(fp);
    NewState(fp, ST_ACKRCVD);
    break;
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    NewState(fp, ST_OPENED);
    (*fp->fn->LayerUp)(fp);
    bundle_LayerUp(fp->bundle, fp);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    bundle_LayerDown(fp->bundle, fp);
    break;
  }
  pfree(bp);
}

static void
FsmRecvConfigNak(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RCN */
{
  int plen, flen;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof *lhp;
  if (plen < flen) {
    pfree(bp);
    return;
  }

  /*
   * Check and process easy case
   */
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(fp->LogLevel, "Oops, RCN in %s.\n", StateNames[fp->state]);
    pfree(bp);
    return;
  case ST_CLOSED:
  case ST_STOPPED:
    (*fp->fn->SendTerminateAck)(fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (*fp->fn->DecodeConfig)(fp, MBUF_CTOP(bp), flen, MODE_NAK);

  switch (fp->state) {
  case ST_REQSENT:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    bundle_LayerDown(fp->bundle, fp);
    break;
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }

  pfree(bp);
}

static void
FsmRecvTermReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RTR */
{
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(fp->LogLevel, "Oops, RTR in %s\n", StateNames[fp->state]);
    break;
  case ST_CLOSED:
  case ST_STOPPED:
  case ST_CLOSING:
  case ST_STOPPING:
  case ST_REQSENT:
    (*fp->fn->SendTerminateAck)(fp);
    break;
  case ST_ACKRCVD:
  case ST_ACKSENT:
    (*fp->fn->SendTerminateAck)(fp);
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    (*fp->fn->SendTerminateAck)(fp);
    StartTimer(&fp->FsmTimer);	/* Start restart timer */
    fp->restart = 0;
    NewState(fp, ST_STOPPING);
    bundle_LayerDown(fp->bundle, fp);
    break;
  }
  pfree(bp);
}

static void
FsmRecvTermAck(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RTA */
{
  switch (fp->state) {
  case ST_CLOSING:
    (*fp->fn->LayerFinish)(fp);
    NewState(fp, ST_CLOSED);
    bundle_LayerFinish(fp->bundle, fp);
    break;
  case ST_STOPPING:
    (*fp->fn->LayerFinish)(fp);
    NewState(fp, ST_STOPPED);
    bundle_LayerFinish(fp->bundle, fp);
    break;
  case ST_ACKRCVD:
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    bundle_LayerDown(fp->bundle, fp);
    break;
  }
  pfree(bp);
}

static void
FsmRecvConfigRej(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RCJ */
{
  int plen, flen;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof *lhp;
  if (plen < flen) {
    pfree(bp);
    return;
  }
  LogPrintf(fp->LogLevel, "RecvConfigRej.\n");

  /*
   * Check and process easy case
   */
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(fp->LogLevel, "Oops, RCJ in %s.\n", StateNames[fp->state]);
    pfree(bp);
    return;
  case ST_CLOSED:
  case ST_STOPPED:
    (*fp->fn->SendTerminateAck)(fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (*fp->fn->DecodeConfig)(fp, MBUF_CTOP(bp), flen, MODE_REJ);

  switch (fp->state) {
  case ST_REQSENT:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    bundle_LayerDown(fp->bundle, fp);
    break;
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

static void
FsmRecvCodeRej(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvCodeRej\n");
  pfree(bp);
}

static void
FsmRecvProtoRej(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  struct physical *p = link2physical(fp->link);
  u_short *sp, proto;

  sp = (u_short *) MBUF_CTOP(bp);
  proto = ntohs(*sp);
  LogPrintf(fp->LogLevel, "-- Protocol 0x%04x (%s) was rejected.\n",
            proto, hdlc_Protocol2Nam(proto));

  switch (proto) {
  case PROTO_LQR:
    if (p)
      StopLqr(p, LQM_LQR);
    else
      LogPrintf(LogERROR, "FsmRecvProtoRej: Not a physical link !\n");
    break;
  case PROTO_CCP:
    fp = &bundle2ccp(fp->bundle, fp->link->name)->fsm;
    (*fp->fn->LayerFinish)(fp);
    switch (fp->state) {
    case ST_CLOSED:
    case ST_CLOSING:
      NewState(fp, ST_CLOSED);
    default:
      NewState(fp, ST_STOPPED);
      break;
    }
    bundle_LayerFinish(fp->bundle, fp);
    break;
  }
  pfree(bp);
}

static void
FsmRecvEchoReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  u_char *cp;
  u_long *lp, magic;

  cp = MBUF_CTOP(bp);
  lp = (u_long *) cp;
  magic = ntohl(*lp);
  if (magic != LcpInfo.his_magic) {
    LogPrintf(LogERROR, "RecvEchoReq: his magic is bad!!\n");
    /* XXX: We should send terminate request */
  }
  if (fp->state == ST_OPENED) {
    *lp = htonl(LcpInfo.want_magic);	/* Insert local magic number */
    LogPrintf(fp->LogLevel, "SendEchoRep(%s)\n", StateNames[fp->state]);
    FsmOutput(fp, CODE_ECHOREP, lhp->id, cp, plength(bp));
  }
  pfree(bp);
}

static void
FsmRecvEchoRep(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  u_long *lp, magic;

  lp = (u_long *) MBUF_CTOP(bp);
  magic = ntohl(*lp);
/*
 * Tolerate echo replies with either magic number
 */
  if (magic != 0 && magic != LcpInfo.his_magic && magic != LcpInfo.want_magic) {
    LogPrintf(LogERROR, "RecvEchoRep: his magic is wrong! expect: %x got: %x\n",
	      LcpInfo.his_magic, magic);

    /*
     * XXX: We should send terminate request. But poor implementation may die
     * as a result.
     */
  }
  RecvEchoLqr(bp);
  pfree(bp);
}

static void
FsmRecvDiscReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvDiscReq\n");
  pfree(bp);
}

static void
FsmRecvIdent(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvIdent\n");
  pfree(bp);
}

static void
FsmRecvTimeRemain(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvTimeRemain\n");
  pfree(bp);
}

static void
FsmRecvResetReq(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  LogPrintf(fp->LogLevel, "RecvResetReq(%d)\n", lhp->id);
  (*fp->fn->RecvResetReq)(fp);
  /*
   * All sendable compressed packets are queued in the PRI_NORMAL modem
   * output queue.... dump 'em to the priority queue so that they arrive
   * at the peer before our ResetAck.
   */
  link_SequenceQueue(fp->link);
  LogPrintf(fp->LogLevel, "SendResetAck(%d)\n", lhp->id);
  FsmOutput(fp, CODE_RESETACK, lhp->id, NULL, 0);
  pfree(bp);
}

static void
FsmRecvResetAck(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  LogPrintf(fp->LogLevel, "RecvResetAck(%d)\n", lhp->id);
  (*fp->fn->RecvResetAck)(fp, lhp->id);
  fp->reqid++;
  pfree(bp);
}

static const struct fsmcodedesc {
  void (*action)(struct fsm *, struct fsmheader *, struct mbuf *);
  const char *name;
} FsmCodes[] = {
  {FsmRecvConfigReq, "Configure Request",},
  {FsmRecvConfigAck, "Configure Ack",},
  {FsmRecvConfigNak, "Configure Nak",},
  {FsmRecvConfigRej, "Configure Reject",},
  {FsmRecvTermReq, "Terminate Request",},
  {FsmRecvTermAck, "Terminate Ack",},
  {FsmRecvCodeRej, "Code Reject",},
  {FsmRecvProtoRej, "Protocol Reject",},
  {FsmRecvEchoReq, "Echo Request",},
  {FsmRecvEchoRep, "Echo Reply",},
  {FsmRecvDiscReq, "Discard Request",},
  {FsmRecvIdent, "Ident",},
  {FsmRecvTimeRemain, "Time Remain",},
  {FsmRecvResetReq, "Reset Request",},
  {FsmRecvResetAck, "Reset Ack",}
};

void
FsmInput(struct fsm * fp, struct mbuf * bp)
{
  int len;
  struct fsmheader *lhp;
  const struct fsmcodedesc *codep;

  len = plength(bp);
  if (len < sizeof(struct fsmheader)) {
    pfree(bp);
    return;
  }
  lhp = (struct fsmheader *) MBUF_CTOP(bp);
  if (lhp->code == 0 || lhp->code > fp->max_code ||
      lhp->code > sizeof FsmCodes / sizeof *FsmCodes) {
    pfree(bp);			/* XXX: Should send code reject */
    return;
  }
  bp->offset += sizeof(struct fsmheader);
  bp->cnt -= sizeof(struct fsmheader);

  codep = FsmCodes + lhp->code - 1;
  LogPrintf(fp->LogLevel, "Received %s (%d) state = %s (%d)\n",
	    codep->name, lhp->id, StateNames[fp->state], fp->state);
  if (LogIsKept(LogDEBUG))
    LogMemory();
  (codep->action)(fp, lhp, bp);
  if (LogIsKept(LogDEBUG))
    LogMemory();
}

void
NullRecvResetReq(struct fsm *fp)
{
  LogPrintf(fp->LogLevel, "Oops - received unexpected reset req\n");
}

void
NullRecvResetAck(struct fsm *fp, u_char id)
{
  LogPrintf(fp->LogLevel, "Oops - received unexpected reset ack\n");
}

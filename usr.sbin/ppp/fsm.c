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
 * $Id: fsm.c,v 1.18 1997/09/10 21:33:32 brian Exp $
 *
 *  TODO:
 *		o Refer loglevel for log output
 *		o Better option log display
 */
#include "fsm.h"
#include "hdlc.h"
#include "lqr.h"
#include "lcpproto.h"
#include "lcp.h"
#include "ccp.h"
#include "modem.h"
#include "loadalias.h"
#include "vars.h"
#include "pred.h"

void FsmSendConfigReq(struct fsm * fp);
void FsmSendTerminateReq(struct fsm * fp);
void FsmInitRestartCounter(struct fsm * fp);
void FsmTimeout(struct fsm * fp);

char const *StateNames[] = {
  "Initial", "Starting", "Closed", "Stopped", "Closing", "Stopping",
  "Req-Sent", "Ack-Rcvd", "Ack-Sent", "Opened",
};

static void
StoppedTimeout(struct fsm * fp)
{
  LogPrintf(fp->LogLevel, "Stopped timer expired\n");
  if (modem != -1)
    DownConnection();
  else
    FsmDown(fp);
}

void
FsmInit(struct fsm * fp)
{
  LogPrintf(LogDEBUG, "FsmInit\n");
  fp->state = ST_INITIAL;
  fp->reqid = 1;
  fp->restart = 1;
  fp->maxconfig = 3;
}

void
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
  bcopy(&lh, MBUF_CTOP(bp), sizeof(struct fsmheader));
  if (count)
    bcopy(ptr, MBUF_CTOP(bp) + sizeof(struct fsmheader), count);
  LogDumpBp(LogDEBUG, "FsmOutput", bp);
  HdlcOutput(PRI_LINK, fp->proto, bp);
}

void
FsmOpen(struct fsm * fp)
{
  switch (fp->state) {
    case ST_INITIAL:
    (fp->LayerStart) (fp);
    NewState(fp, ST_STARTING);
    break;
  case ST_STARTING:
    break;
  case ST_CLOSED:
    if (fp->open_mode == OPEN_PASSIVE) {
      NewState(fp, ST_STOPPED);
    } else {
      FsmInitRestartCounter(fp);
      FsmSendConfigReq(fp);
      NewState(fp, ST_REQSENT);
    }
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
FsmDown(struct fsm * fp)
{
  switch (fp->state) {
    case ST_CLOSED:
    case ST_CLOSING:
    NewState(fp, ST_INITIAL);
    break;
  case ST_STOPPED:
    (fp->LayerStart) (fp);
    /* Fall into.. */
  case ST_STOPPING:
  case ST_REQSENT:
  case ST_ACKRCVD:
  case ST_ACKSENT:
    NewState(fp, ST_STARTING);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    NewState(fp, ST_STARTING);
    break;
  }
}

void
FsmClose(struct fsm * fp)
{
  switch (fp->state) {
    case ST_STARTING:
    NewState(fp, ST_INITIAL);
    break;
  case ST_STOPPED:
    NewState(fp, ST_CLOSED);
    break;
  case ST_STOPPING:
    NewState(fp, ST_CLOSING);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    /* Fall down */
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
void
FsmSendConfigReq(struct fsm * fp)
{
  if (--fp->maxconfig > 0) {
    (fp->SendConfigReq) (fp);
    StartTimer(&fp->FsmTimer);	/* Start restart timer */
    fp->restart--;		/* Decrement restart counter */
  } else {
    FsmClose(fp);
  }
}

void
FsmSendTerminateReq(struct fsm * fp)
{
  LogPrintf(fp->LogLevel, "SendTerminateReq.\n");
  FsmOutput(fp, CODE_TERMREQ, fp->reqid++, NULL, 0);
  (fp->SendTerminateReq) (fp);
  StartTimer(&fp->FsmTimer);	/* Start restart timer */
  fp->restart--;		/* Decrement restart counter */
}

static void
FsmSendConfigAck(struct fsm * fp,
		 struct fsmheader * lhp,
		 u_char * option,
		 int count)
{
  LogPrintf(fp->LogLevel, "SendConfigAck(%s)\n", StateNames[fp->state]);
  (fp->DecodeConfig) (option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGACK, lhp->id, option, count);
}

static void
FsmSendConfigRej(struct fsm * fp,
		 struct fsmheader * lhp,
		 u_char * option,
		 int count)
{
  LogPrintf(fp->LogLevel, "SendConfigRej(%s)\n", StateNames[fp->state]);
  (fp->DecodeConfig) (option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGREJ, lhp->id, option, count);
}

static void
FsmSendConfigNak(struct fsm * fp,
		 struct fsmheader * lhp,
		 u_char * option,
		 int count)
{
  LogPrintf(fp->LogLevel, "SendConfigNak(%s)\n", StateNames[fp->state]);
  (fp->DecodeConfig) (option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGNAK, lhp->id, option, count);
}

/*
 *	Timeout actions
 */
void
FsmTimeout(struct fsm * fp)
{
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
      NewState(fp, ST_CLOSED);
      (fp->LayerFinish) (fp);
      break;
    case ST_STOPPING:
      NewState(fp, ST_STOPPED);
      (fp->LayerFinish) (fp);
      break;
    case ST_REQSENT:		/* XXX: 3p */
    case ST_ACKSENT:
    case ST_ACKRCVD:
      NewState(fp, ST_STOPPED);
      (fp->LayerFinish) (fp);
      break;
    }
  }
}

void
FsmInitRestartCounter(struct fsm * fp)
{
  StopTimer(&fp->FsmTimer);
  fp->FsmTimer.state = TIMER_STOPPED;
  fp->FsmTimer.func = FsmTimeout;
  fp->FsmTimer.arg = (void *) fp;
  (fp->InitRestartCounter) (fp);
}

/*
 *   Actions when receive packets
 */
void
FsmRecvConfigReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RCR */
{
  int plen, flen;
  int ackaction = 0;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof(*lhp);
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
    (fp->SendTerminateAck) (fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    LogPrintf(LogERROR, "Got ConfigReq while state = %d\n", fp->state);
    pfree(bp);
    return;
  }

  (fp->DecodeConfig) (MBUF_CTOP(bp), flen, MODE_REQ);

  if (nakp == NakBuff && rejp == RejBuff)
    ackaction = 1;

  switch (fp->state) {
  case ST_OPENED:
    (fp->LayerDown) (fp);
    FsmSendConfigReq(fp);
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
  case ST_STOPPED:
  case ST_OPENED:
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
      (fp->LayerUp) (fp);
    }
    break;
  case ST_ACKSENT:
    if (!ackaction)
      NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvConfigAck(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RCA */
{
  switch (fp->state) {
    case ST_CLOSED:
    case ST_STOPPED:
    (fp->SendTerminateAck) (fp);
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
    (fp->LayerUp) (fp);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvConfigNak(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RCN */
{
  int plen, flen;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof(*lhp);
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
    (fp->SendTerminateAck) (fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (fp->DecodeConfig) (MBUF_CTOP(bp), flen, MODE_NAK);

  switch (fp->state) {
  case ST_REQSENT:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    /* Fall down */
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }

  pfree(bp);
}

void
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
    (fp->SendTerminateAck) (fp);
    break;
  case ST_ACKRCVD:
  case ST_ACKSENT:
    (fp->SendTerminateAck) (fp);
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    (fp->SendTerminateAck) (fp);
    StartTimer(&fp->FsmTimer);	/* Start restart timer */
    fp->restart = 0;
    NewState(fp, ST_STOPPING);
    break;
  }
  pfree(bp);
}

void
FsmRecvTermAck(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RTA */
{
  switch (fp->state) {
    case ST_CLOSING:
    NewState(fp, ST_CLOSED);
    (fp->LayerFinish) (fp);
    break;
  case ST_STOPPING:
    NewState(fp, ST_STOPPED);
    (fp->LayerFinish) (fp);
    break;
  case ST_ACKRCVD:
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvConfigRej(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
/* RCJ */
{
  int plen, flen;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof(*lhp);
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
    (fp->SendTerminateAck) (fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (fp->DecodeConfig) (MBUF_CTOP(bp), flen, MODE_REJ);

  switch (fp->state) {
  case ST_REQSENT:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  case ST_OPENED:
    (fp->LayerDown) (fp);
    /* Fall down */
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvCodeRej(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvCodeRej\n");
  pfree(bp);
}

void
FsmRecvProtoRej(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  u_short *sp, proto;

  sp = (u_short *) MBUF_CTOP(bp);
  proto = ntohs(*sp);
  LogPrintf(fp->LogLevel, "-- Protocol (%04x) was rejected.\n", proto);

  switch (proto) {
  case PROTO_LQR:
    StopLqr(LQM_LQR);
    break;
  case PROTO_CCP:
    fp = &CcpFsm;
    (fp->LayerFinish) (fp);
    switch (fp->state) {
    case ST_CLOSED:
    case ST_CLOSING:
      NewState(fp, ST_CLOSED);
    default:
      NewState(fp, ST_STOPPED);
      break;
    }
    break;
  }
  pfree(bp);
}

void
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

void
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

void
FsmRecvDiscReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvDiscReq\n");
  pfree(bp);
}

void
FsmRecvIdent(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvIdent\n");
  pfree(bp);
}

void
FsmRecvTimeRemain(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvTimeRemain\n");
  pfree(bp);
}

void
FsmRecvResetReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvResetReq\n");
  CcpRecvResetReq(fp);
  LogPrintf(fp->LogLevel, "SendResetAck\n");
  FsmOutput(fp, CODE_RESETACK, fp->reqid, NULL, 0);
  pfree(bp);
}

void
FsmRecvResetAck(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  LogPrintf(fp->LogLevel, "RecvResetAck\n");
  Pred1Init(1);			/* Initialize Input part */
  fp->reqid++;
  pfree(bp);
}

struct fsmcodedesc FsmCodes[] = {
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
  {FsmRecvResetAck, "Reset Ack",},
};

void
FsmInput(struct fsm * fp, struct mbuf * bp)
{
  int len;
  struct fsmheader *lhp;
  struct fsmcodedesc *codep;

  len = plength(bp);
  if (len < sizeof(struct fsmheader)) {
    pfree(bp);
    return;
  }
  lhp = (struct fsmheader *) MBUF_CTOP(bp);
  if (lhp->code == 0 || lhp->code > fp->max_code) {
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
  (codep->action) (fp, lhp, bp);
  if (LogIsKept(LogDEBUG))
    LogMemory();
}

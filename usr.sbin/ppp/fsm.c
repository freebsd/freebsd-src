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
 * $Id: fsm.c,v 1.6 1996/01/30 11:08:29 dfr Exp $
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

void FsmSendConfigReq(struct fsm *fp);
void FsmSendTerminateReq(struct fsm *fp);
void FsmInitRestartCounter(struct fsm *fp);
void FsmTimeout(struct fsm *fp);

char const *StateNames[] = {
  "Initial", "Starting", "Closed", "Stopped", "Closing", "Stopping",
  "Req-Sent", "Ack-Rcvd", "Ack-Sent", "Opend",
};

void
FsmInit(fp)
struct fsm *fp;
{
#ifdef DEBUG
  logprintf("FsmInit\n");
#endif
  fp->state = ST_INITIAL;
  fp->reqid = 1;
  fp->restart = 1;
  fp->maxconfig = 3;
}

void
NewState(fp, new)
struct fsm *fp;
int new;
{
  LogPrintf(LOG_LCP_BIT, "%s: state change %s --> %s\n",
	  fp->name, StateNames[fp->state], StateNames[new]);
  fp->state = new;
  if ((new >= ST_INITIAL && new <= ST_STOPPED) || (new == ST_OPENED))
    StopTimer(&fp->FsmTimer);
}

void
FsmOutput(fp, code, id, ptr, count)
struct fsm *fp;
u_int code, id;
u_char *ptr;
int count;
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  plen =  sizeof(struct fsmheader) + count;
  lh.code = code;
  lh.id = id;
  lh.length = htons(plen);
  bp = mballoc(plen, MB_FSM);
  bcopy(&lh, MBUF_CTOP(bp), sizeof(struct fsmheader));
  if (count)
    bcopy(ptr, MBUF_CTOP(bp) + sizeof(struct fsmheader), count);
#ifdef DEBUG
  DumpBp(bp);
#endif
  HdlcOutput(PRI_LINK, fp->proto, bp);
}

void
FsmOpen(fp)
struct fsm *fp;
{
  switch (fp->state) {
  case ST_INITIAL:
    (fp->LayerStart)(fp);
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
  case ST_STOPPED:	/* XXX: restart option */
  case ST_REQSENT:
  case ST_ACKRCVD:
  case ST_ACKSENT:
  case ST_OPENED:	/* XXX: restart option */
    break;
  case ST_CLOSING:	/* XXX: restart option */
  case ST_STOPPING:	/* XXX: restart option */
    NewState(fp, ST_STOPPING);
    break;
  }
}

void
FsmUp(fp)
struct fsm *fp;
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
    LogPrintf(LOG_LCP_BIT, "%s: Oops, Up at %s\n",
	    fp->name, StateNames[fp->state]);
    break;
  }
}

void
FsmDown(fp)
struct fsm *fp;
{
  switch (fp->state) {
  case ST_CLOSED:
  case ST_CLOSING:
    NewState(fp, ST_INITIAL);
    break;
  case ST_STOPPED:
    (fp->LayerStart)(fp);
    /* Fall into.. */
  case ST_STOPPING:
  case ST_REQSENT:
  case ST_ACKRCVD:
  case ST_ACKSENT:
    NewState(fp, ST_STARTING);
    break;
  case ST_OPENED:
    (fp->LayerDown)(fp);
    NewState(fp, ST_STARTING);
    break;
  }
}

void
FsmClose(fp)
struct fsm *fp;
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
    (fp->LayerDown)(fp);
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
FsmSendConfigReq(fp)
struct fsm *fp;
{
  if (--fp->maxconfig > 0) {
    (fp->SendConfigReq)(fp);
    StartTimer(&fp->FsmTimer);	/* Start restart timer */
    fp->restart--;		/* Decrement restart counter */
  } else {
    FsmClose(fp);
  }
}

void
FsmSendTerminateReq(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: SendTerminateReq.\n", fp->name);
  FsmOutput(fp, CODE_TERMREQ, fp->reqid++, NULL, 0);
  (fp->SendTerminateReq)(fp);
  StartTimer(&fp->FsmTimer);	/* Start restart timer */
  fp->restart--;		/* Decrement restart counter */
}

static void
FsmSendConfigAck(fp, lhp, option, count)
struct fsm *fp;
struct fsmheader *lhp;
u_char *option;
int count;
{
  LogPrintf(LOG_LCP_BIT, "%s:  SendConfigAck(%s)\n", fp->name, StateNames[fp->state]);
  (fp->DecodeConfig)(option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGACK, lhp->id, option, count);
}

static void
FsmSendConfigRej(fp, lhp, option, count)
struct fsm *fp;
struct fsmheader *lhp;
u_char *option;
int count;
{
  LogPrintf(LOG_LCP_BIT, "%s:  SendConfigRej(%s)\n", fp->name, StateNames[fp->state]);
  (fp->DecodeConfig)(option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGREJ, lhp->id, option, count);
}

static void
FsmSendConfigNak(fp, lhp, option, count)
struct fsm *fp;
struct fsmheader *lhp;
u_char *option;
int count;
{
  LogPrintf(LOG_LCP_BIT, "%s:  SendConfigNak(%s)\n",
	    fp->name, StateNames[fp->state]);
  (fp->DecodeConfig)(option, count, MODE_NOP);
  FsmOutput(fp, CODE_CONFIGNAK, lhp->id, option, count);
}

/*
 *	Timeout actions
 */
void
FsmTimeout(fp)
struct fsm *fp;
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
      (fp->LayerFinish)(fp);
      break;
    case ST_STOPPING:
      NewState(fp, ST_STOPPED);
      (fp->LayerFinish)(fp);
      break;
    case ST_REQSENT:		/* XXX: 3p */
    case ST_ACKSENT:
    case ST_ACKRCVD:
      NewState(fp, ST_STOPPED);
      (fp->LayerFinish)(fp);
      break;
    }
  }
}

void
FsmInitRestartCounter(fp)
struct fsm *fp;
{
  StopTimer(&fp->FsmTimer);
  fp->FsmTimer.state = TIMER_STOPPED;
  fp->FsmTimer.func = FsmTimeout;
  fp->FsmTimer.arg = (void *)fp;
  (fp->InitRestartCounter)(fp);
}

/*
 *   Actions when receive packets
 */
void
FsmRecvConfigReq(fp, lhp, bp)			/* RCR */
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  int plen, flen;
  int ackaction = 0;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof(*lhp);
  if (plen < flen) {
    logprintf("** plen (%d) < flen (%d)\n", plen, flen);
    pfree(bp);
    return;
  }


  /*
   *  Check and process easy case
   */
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(LOG_LCP_BIT, "%s: Oops, RCR in %s.\n",
	    fp->name, StateNames[fp->state]);
    pfree(bp);
    return;
  case ST_CLOSED:
    (fp->SendTerminateAck)(fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
logprintf("## state = %d\n", fp->state);
    pfree(bp);
    return;
  }

  (fp->DecodeConfig)(MBUF_CTOP(bp), flen, MODE_REQ);

  if (nakp == NakBuff && rejp == RejBuff)
    ackaction = 1;

  switch (fp->state) {
  case ST_OPENED:
    (fp->LayerDown)(fp);
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
      (fp->LayerUp)(fp);
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
FsmRecvConfigAck(fp, lhp, bp)			/* RCA */
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  switch (fp->state) {
  case ST_CLOSED:
  case ST_STOPPED:
    (fp->SendTerminateAck)(fp);
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
    (fp->LayerUp)(fp);
    break;
  case ST_OPENED:
    (fp->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvConfigNak(fp, lhp, bp)			/* RCN */
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  int plen, flen;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof(*lhp);
  if (plen < flen) {
    pfree(bp);
    return;
  }

  /*
   *  Check and process easy case
   */
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(LOG_LCP_BIT, "%s: Oops, RCN in %s.\n",
	    fp->name, StateNames[fp->state]);
    pfree(bp);
    return;
  case ST_CLOSED:
  case ST_STOPPED:
    (fp->SendTerminateAck)(fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (fp->DecodeConfig)(MBUF_CTOP(bp), flen, MODE_NAK);

  switch (fp->state) {
  case ST_REQSENT:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  case ST_OPENED:
    (fp->LayerDown)(fp);
    /* Fall down */
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }

  pfree(bp);
}

void
FsmRecvTermReq(fp, lhp, bp)				/* RTR */
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(LOG_LCP_BIT, "%s: Oops, RTR in %s\n", fp->name,
	    StateNames[fp->state]);
    break;
  case ST_CLOSED:
  case ST_STOPPED:
  case ST_CLOSING:
  case ST_STOPPING:
  case ST_REQSENT:
    (fp->SendTerminateAck)(fp);
    break;
  case ST_ACKRCVD:
  case ST_ACKSENT:
    (fp->SendTerminateAck)(fp);
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (fp->LayerDown)(fp);
    /* Zero Restart counter */
    (fp->SendTerminateAck)(fp);
    NewState(fp, ST_STOPPING);
    break;
  }
  pfree(bp);
}

void
FsmRecvTermAck(fp, lhp, bp)			/* RTA */
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  switch (fp->state) {
  case ST_CLOSING:
    NewState(fp, ST_CLOSED);
    (fp->LayerFinish)(fp);
    break;
  case ST_STOPPING:
    NewState(fp, ST_STOPPED);
    (fp->LayerFinish)(fp);
    break;
  case ST_ACKRCVD:
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (fp->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvConfigRej(fp, lhp, bp)			/* RCJ */
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  int plen, flen;

  plen = plength(bp);
  flen = ntohs(lhp->length) - sizeof(*lhp);
  if (plen < flen) {
    pfree(bp);
    return;
  }
  LogPrintf(LOG_LCP_BIT, "%s: RecvConfigRej.\n", fp->name);

  /*
   *  Check and process easy case
   */
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(LOG_LCP_BIT, "%s: Oops, RCJ in %s.\n",
	    fp->name, StateNames[fp->state]);
    pfree(bp);
    return;
  case ST_CLOSED:
  case ST_STOPPED:
    (fp->SendTerminateAck)(fp);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  (fp->DecodeConfig)(MBUF_CTOP(bp), flen, MODE_REJ);

  switch (fp->state) {
  case ST_REQSENT:
  case ST_ACKSENT:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  case ST_OPENED:
    (fp->LayerDown)(fp);
    /* Fall down */
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

void
FsmRecvCodeRej(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  LogPrintf(LOG_LCP_BIT, "%s: RecvCodeRej\n", fp->name);
  pfree(bp);
}

void
FsmRecvProtoRej(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  u_short *sp, proto;

  sp = (u_short *)MBUF_CTOP(bp);
  proto = ntohs(*sp);
  LogPrintf(LOG_LCP_BIT, "-- Protocol (%04x) was rejected.\n", proto);

  switch (proto) {
  case PROTO_LQR:
    StopLqr(LQM_LQR);
    break;
  case PROTO_CCP:
    fp = &CcpFsm;
    (fp->LayerFinish)(fp);
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
FsmRecvEchoReq(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  u_char *cp;
  u_long *lp, magic;

  cp = MBUF_CTOP(bp);
  lp = (u_long *)cp;
  magic = ntohl(*lp);
  if (magic != LcpInfo.his_magic) {
    logprintf("RecvEchoReq: his magic is bad!!\n");
    /* XXX: We should send terminate request */
  }

  if (fp->state == ST_OPENED) {
    *lp = htonl(LcpInfo.want_magic);	/* Insert local magic number */
    LogPrintf(LOG_LCP_BIT, "%s:  SendEchoRep(%s)\n", fp->name, StateNames[fp->state]);
    FsmOutput(fp, CODE_ECHOREP, lhp->id, cp, plength(bp));
  }
  pfree(bp);
}

void
FsmRecvEchoRep(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  u_long *lp, magic;

  lp = (u_long *)MBUF_CTOP(bp);
  magic = ntohl(*lp);
/*
 * Tolerate echo replies with either magic number
 */
  if (magic != 0 && magic != LcpInfo.his_magic && magic != LcpInfo.want_magic) {
    logprintf("RecvEchoRep: his magic is wrong! expect: %x got: %x\n",
	LcpInfo.his_magic, magic);
    /*
     *  XXX: We should send terminate request. But poor implementation
     *       may die as a result.
     */
  }
  RecvEchoLqr(bp);
  pfree(bp);
}

void
FsmRecvDiscReq(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  LogPrintf(LOG_LCP_BIT, "%s: RecvDiscReq\n", fp->name);
  pfree(bp);
}

void
FsmRecvIdent(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  LogPrintf(LOG_LCP_BIT, "%s: RecvIdent\n", fp->name);
  pfree(bp);
}

void
FsmRecvTimeRemain(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  LogPrintf(LOG_LCP_BIT, "%s: RecvTimeRemain\n", fp->name);
  pfree(bp);
}

void
FsmRecvResetReq(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  LogPrintf(LOG_LCP_BIT, "%s: RecvResetReq\n", fp->name);
  CcpRecvResetReq(fp);
  LogPrintf(LOG_LCP_BIT, "%s: SendResetAck\n", fp->name);
  FsmOutput(fp, CODE_RESETACK, fp->reqid, NULL, 0);
  pfree(bp);
}

void
FsmRecvResetAck(fp, lhp, bp)
struct fsm *fp;
struct fsmheader *lhp;
struct mbuf *bp;
{
  LogPrintf(LOG_LCP_BIT, "%s: RecvResetAck\n", fp->name);
  fp->reqid++;
  pfree(bp);
}

struct fsmcodedesc FsmCodes[] = {
 { FsmRecvConfigReq,  "Configure Request", },
 { FsmRecvConfigAck,  "Configure Ack", },
 { FsmRecvConfigNak,  "Configure Nak", },
 { FsmRecvConfigRej,  "Configure Reject", },
 { FsmRecvTermReq,    "Terminate Request", },
 { FsmRecvTermAck,    "Terminate Ack", },
 { FsmRecvCodeRej,    "Code Reject", },
 { FsmRecvProtoRej,   "Protocol Reject", },
 { FsmRecvEchoReq,    "Echo Request", },
 { FsmRecvEchoRep,    "Echo Reply", },
 { FsmRecvDiscReq,    "Discard Request", },
 { FsmRecvIdent,      "Ident", },
 { FsmRecvTimeRemain, "Time Remain", },
 { FsmRecvResetReq,   "Reset Request", },
 { FsmRecvResetAck,   "Reset Ack", },
};

void
FsmInput(fp, bp)
struct fsm *fp;
struct mbuf *bp;
{
  int len;
  struct fsmheader *lhp;
  struct fsmcodedesc *codep;

  len = plength(bp);
  if (len < sizeof(struct fsmheader)) {
    pfree(bp);
    return;
  }
  lhp = (struct fsmheader *)MBUF_CTOP(bp);
  if (lhp->code == 0 || lhp->code > fp->max_code) {
    pfree(bp);		/* XXX: Should send code reject */
    return;
  }

  bp->offset += sizeof(struct fsmheader);
  bp->cnt -= sizeof(struct fsmheader);

  codep = FsmCodes + lhp->code - 1;
  LogPrintf(LOG_LCP_BIT, "%s: Received %s (%d) state = %s (%d)\n",
    fp->name, codep->name, lhp->id, StateNames[fp->state], fp->state);
#ifdef DEBUG
  LogMemory();
#endif
  (codep->action)(fp, lhp, bp);
#ifdef DEBUG
  LogMemory();
#endif
}

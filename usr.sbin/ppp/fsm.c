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
 * $Id: fsm.c,v 1.27.2.29 1998/04/19 15:24:41 brian Exp $
 *
 *  TODO:
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <string.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "bundle.h"
#include "async.h"
#include "physical.h"
#include "lcpproto.h"

static void FsmSendConfigReq(struct fsm *);
static void FsmSendTerminateReq(struct fsm *);
static void FsmInitRestartCounter(struct fsm *);

typedef void (recvfn)(struct fsm *, struct fsmheader *, struct mbuf *);
static recvfn FsmRecvConfigReq, FsmRecvConfigAck, FsmRecvConfigNak,
              FsmRecvConfigRej, FsmRecvTermReq, FsmRecvTermAck,
              FsmRecvCodeRej, FsmRecvProtoRej, FsmRecvEchoReq,
              FsmRecvEchoRep, FsmRecvDiscReq, FsmRecvIdent,
              FsmRecvTimeRemain, FsmRecvResetReq, FsmRecvResetAck;

static const struct fsmcodedesc {
  recvfn *recv;
  unsigned check_reqid : 1;
  unsigned inc_reqid : 1;
  const char *name;
} FsmCodes[] = {
  { FsmRecvConfigReq, 0, 0, "ConfigReq"    },
  { FsmRecvConfigAck, 1, 1, "ConfigAck"    },
  { FsmRecvConfigNak, 1, 1, "ConfigNak"    },
  { FsmRecvConfigRej, 1, 1, "ConfigRej"    },
  { FsmRecvTermReq,   0, 0, "TerminateReq" },
  { FsmRecvTermAck,   1, 1, "TerminateAck" },
  { FsmRecvCodeRej,   0, 0, "CodeRej"      },
  { FsmRecvProtoRej,  0, 0, "ProtocolRej"  },
  { FsmRecvEchoReq,   0, 0, "EchoRequest"  },
  { FsmRecvEchoRep,   0, 0, "EchoReply"    },
  { FsmRecvDiscReq,   0, 0, "DiscardReq"   },
  { FsmRecvIdent,     0, 0, "Ident"        },
  { FsmRecvTimeRemain,0, 0, "TimeRemain"   },
  { FsmRecvResetReq,  0, 0, "ResetReqt"    },
  { FsmRecvResetAck,  0, 1, "ResetAck"     }
};

static const char *
Code2Nam(u_int code)
{
  if (code == 0 || code > sizeof FsmCodes / sizeof FsmCodes[0])
    return "Unknown";
  return FsmCodes[code-1].name;
}

const char *
State2Nam(u_int state)
{
  static const char *StateNames[] = {
    "Initial", "Starting", "Closed", "Stopped", "Closing", "Stopping",
    "Req-Sent", "Ack-Rcvd", "Ack-Sent", "Opened",
  };

  if (state >= sizeof StateNames / sizeof StateNames[0])
    return "unknown";
  return StateNames[state];
}

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
  if (fp->state == ST_STOPPED) {
    /* Force ourselves back to initial */
    FsmDown(fp);
    FsmClose(fp);
  }
}

void
fsm_Init(struct fsm *fp, const char *name, u_short proto, int mincode,
         int maxcode, int maxcfg, int LogLevel, struct bundle *bundle,
         struct link *l, const struct fsm_parent *parent,
         struct fsm_callbacks *fn, const char *timer_names[3])
{
  fp->name = name;
  fp->proto = proto;
  fp->min_code = mincode;
  fp->max_code = maxcode;
  fp->state = fp->min_code > CODE_TERMACK ? ST_OPENED : ST_INITIAL;
  fp->reqid = 1;
  fp->restart = 1;
  fp->maxconfig = maxcfg;
  memset(&fp->FsmTimer, '\0', sizeof fp->FsmTimer);
  memset(&fp->OpenTimer, '\0', sizeof fp->OpenTimer);
  memset(&fp->StoppedTimer, '\0', sizeof fp->StoppedTimer);
  fp->LogLevel = LogLevel;
  fp->link = l;
  fp->bundle = bundle;
  fp->parent = parent;
  fp->fn = fn;
  fp->FsmTimer.name = timer_names[0];
  fp->OpenTimer.name = timer_names[1];
  fp->StoppedTimer.name = timer_names[2];
}

static void
NewState(struct fsm * fp, int new)
{
  LogPrintf(fp->LogLevel, "State change %s --> %s\n",
	    State2Nam(fp->state), State2Nam(new));
  if (fp->state == ST_STOPPED && fp->StoppedTimer.state == TIMER_RUNNING)
    StopTimer(&fp->StoppedTimer);
  fp->state = new;
  if ((new >= ST_INITIAL && new <= ST_STOPPED) || (new == ST_OPENED)) {
    StopTimer(&fp->FsmTimer);
    if (new == ST_STOPPED && fp->StoppedTimer.load) {
      StopTimer(&fp->StoppedTimer);
      fp->StoppedTimer.func = StoppedTimeout;
      fp->StoppedTimer.arg = (void *) fp;
      StartTimer(&fp->StoppedTimer);
    }
  }
}

void
FsmOutput(struct fsm *fp, u_int code, u_int id, u_char *ptr, int count)
{
  int plen;
  struct fsmheader lh;
  struct mbuf *bp;

  if (LogIsKept(fp->LogLevel)) {
    LogPrintf(fp->LogLevel, "Send%s(%d) state = %s\n", Code2Nam(code),
              id, State2Nam(fp->state));
    switch (code) {
      case CODE_CONFIGREQ:
      case CODE_CONFIGACK:
      case CODE_CONFIGREJ:
      case CODE_CONFIGNAK:
        (*fp->fn->DecodeConfig)(fp, ptr, count, MODE_NOP, NULL);
        if (count < sizeof(struct fsmconfig))
          LogPrintf(fp->LogLevel, "  [EMPTY]\n");
        break;
    }
  }

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
    (*fp->parent->LayerStart)(fp->parent->object, fp);
    break;
  case ST_CLOSED:
    if (fp->open_mode == OPEN_PASSIVE) {
      NewState(fp, ST_STOPPED);
    } else if (fp->open_mode > 0) {
      if (fp->open_mode > 1)
        LogPrintf(LogPHASE, "Entering STOPPED state for %d seconds\n",
                  fp->open_mode);
      NewState(fp, ST_STOPPED);
      StopTimer(&fp->OpenTimer);
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
    LogPrintf(fp->LogLevel, "Oops, Up at %s\n", State2Nam(fp->state));
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
    (*fp->parent->LayerFinish)(fp->parent->object, fp);
    break;
  case ST_STOPPED:
    NewState(fp, ST_STARTING);
    (*fp->fn->LayerStart)(fp);
    (*fp->parent->LayerStart)(fp->parent->object, fp);
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
    (*fp->parent->LayerDown)(fp->parent->object, fp);
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
    (*fp->parent->LayerFinish)(fp->parent->object, fp);
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
    (*fp->parent->LayerDown)(fp->parent->object, fp);
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
FsmSendTerminateReq(struct fsm *fp)
{
  FsmOutput(fp, CODE_TERMREQ, fp->reqid, NULL, 0);
  (*fp->fn->SentTerminateReq)(fp);
  StartTimer(&fp->FsmTimer);	/* Start restart timer */
  fp->restart--;		/* Decrement restart counter */
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
      (*fp->parent->LayerFinish)(fp->parent->object, fp);
      break;
    case ST_STOPPING:
      (*fp->fn->LayerFinish)(fp);
      NewState(fp, ST_STOPPED);
      (*fp->parent->LayerFinish)(fp->parent->object, fp);
      break;
    case ST_REQSENT:		/* XXX: 3p */
    case ST_ACKSENT:
    case ST_ACKRCVD:
      (*fp->fn->LayerFinish)(fp);
      NewState(fp, ST_STOPPED);
      (*fp->parent->LayerFinish)(fp->parent->object, fp);
      break;
    }
  }
}

static void
FsmInitRestartCounter(struct fsm * fp)
{
  StopTimer(&fp->FsmTimer);
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
  struct fsm_decode dec;
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
    LogPrintf(fp->LogLevel, "Oops, RCR in %s.\n", State2Nam(fp->state));
    pfree(bp);
    return;
  case ST_CLOSED:
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
    pfree(bp);
    return;
  case ST_CLOSING:
    LogPrintf(fp->LogLevel, "Error: Got ConfigReq while state = %d\n",
              fp->state);
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  dec.ackend = dec.ack;
  dec.nakend = dec.nak;
  dec.rejend = dec.rej;
  (*fp->fn->DecodeConfig)(fp, MBUF_CTOP(bp), flen, MODE_REQ, &dec);
  if (flen < sizeof(struct fsmconfig))
    LogPrintf(fp->LogLevel, "  [EMPTY]\n");

  if (dec.nakend == dec.nak && dec.rejend == dec.rej)
    ackaction = 1;

  switch (fp->state) {
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    (*fp->parent->LayerDown)(fp->parent->object, fp);
    break;
  case ST_STOPPED:
    FsmInitRestartCounter(fp);
    FsmSendConfigReq(fp);
    break;
  }

  if (dec.rejend != dec.rej)
    FsmOutput(fp, CODE_CONFIGREJ, lhp->id, dec.rej, dec.rejend - dec.rej);
  if (dec.nakend != dec.nak)
    FsmOutput(fp, CODE_CONFIGNAK, lhp->id, dec.nak, dec.nakend - dec.nak);
  if (ackaction)
    FsmOutput(fp, CODE_CONFIGACK, lhp->id, dec.ack, dec.ackend - dec.ack);

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
      (*fp->parent->LayerUp)(fp->parent->object, fp);
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
FsmRecvConfigAck(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RCA */
{
  switch (fp->state) {
    case ST_CLOSED:
    case ST_STOPPED:
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
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
    (*fp->parent->LayerUp)(fp->parent->object, fp);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    (*fp->parent->LayerDown)(fp->parent->object, fp);
    break;
  }
  pfree(bp);
}

static void
FsmRecvConfigNak(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RCN */
{
  struct fsm_decode dec;
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
    LogPrintf(fp->LogLevel, "Oops, RCN in %s.\n", State2Nam(fp->state));
    pfree(bp);
    return;
  case ST_CLOSED:
  case ST_STOPPED:
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  dec.ackend = dec.ack;
  dec.nakend = dec.nak;
  dec.rejend = dec.rej;
  (*fp->fn->DecodeConfig)(fp, MBUF_CTOP(bp), flen, MODE_NAK, &dec);
  if (flen < sizeof(struct fsmconfig))
    LogPrintf(fp->LogLevel, "  [EMPTY]\n");

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
    (*fp->parent->LayerDown)(fp->parent->object, fp);
    break;
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }

  pfree(bp);
}

static void
FsmRecvTermReq(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RTR */
{
  switch (fp->state) {
  case ST_INITIAL:
  case ST_STARTING:
    LogPrintf(fp->LogLevel, "Oops, RTR in %s\n", State2Nam(fp->state));
    break;
  case ST_CLOSED:
  case ST_STOPPED:
  case ST_CLOSING:
  case ST_STOPPING:
  case ST_REQSENT:
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
    break;
  case ST_ACKRCVD:
  case ST_ACKSENT:
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
    StartTimer(&fp->FsmTimer);	/* Start restart timer */
    fp->restart = 0;
    NewState(fp, ST_STOPPING);
    (*fp->parent->LayerDown)(fp->parent->object, fp);
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
    (*fp->parent->LayerFinish)(fp->parent->object, fp);
    break;
  case ST_STOPPING:
    (*fp->fn->LayerFinish)(fp);
    NewState(fp, ST_STOPPED);
    (*fp->parent->LayerFinish)(fp->parent->object, fp);
    break;
  case ST_ACKRCVD:
    NewState(fp, ST_REQSENT);
    break;
  case ST_OPENED:
    (*fp->fn->LayerDown)(fp);
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    (*fp->parent->LayerDown)(fp->parent->object, fp);
    break;
  }
  pfree(bp);
}

static void
FsmRecvConfigRej(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
/* RCJ */
{
  struct fsm_decode dec;
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
    LogPrintf(fp->LogLevel, "Oops, RCJ in %s.\n", State2Nam(fp->state));
    pfree(bp);
    return;
  case ST_CLOSED:
  case ST_STOPPED:
    (*fp->fn->SendTerminateAck)(fp, lhp->id);
    pfree(bp);
    return;
  case ST_CLOSING:
  case ST_STOPPING:
    pfree(bp);
    return;
  }

  dec.ackend = dec.ack;
  dec.nakend = dec.nak;
  dec.rejend = dec.rej;
  (*fp->fn->DecodeConfig)(fp, MBUF_CTOP(bp), flen, MODE_REJ, &dec);
  if (flen < sizeof(struct fsmconfig))
    LogPrintf(fp->LogLevel, "  [EMPTY]\n");

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
    (*fp->parent->LayerDown)(fp->parent->object, fp);
    break;
  case ST_ACKRCVD:
    FsmSendConfigReq(fp);
    NewState(fp, ST_REQSENT);
    break;
  }
  pfree(bp);
}

static void
FsmRecvCodeRej(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
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
    if (fp->proto == PROTO_LCP) {
      fp = &fp->link->ccp.fsm;
      (*fp->fn->LayerFinish)(fp);
      switch (fp->state) {
      case ST_CLOSED:
      case ST_CLOSING:
        NewState(fp, ST_CLOSED);
      default:
        NewState(fp, ST_STOPPED);
        break;
      }
      (*fp->parent->LayerFinish)(fp->parent->object, fp);
    }
    break;
  }
  pfree(bp);
}

static void
FsmRecvEchoReq(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  struct lcp *lcp = fsm2lcp(fp);
  u_char *cp;
  u_int32_t magic;

  if (lcp) {
    cp = MBUF_CTOP(bp);
    magic = ntohl(*(u_int32_t *)cp);
    if (magic != lcp->his_magic) {
      LogPrintf(LogERROR, "RecvEchoReq: his magic is bad!!\n");
      /* XXX: We should send terminate request */
    }
    if (fp->state == ST_OPENED) {
      *(u_int32_t *)cp = htonl(lcp->want_magic); /* local magic */
      FsmOutput(fp, CODE_ECHOREP, lhp->id, cp, plength(bp));
    }
  }
  pfree(bp);
}

static void
FsmRecvEchoRep(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  struct lcp *lcp = fsm2lcp(fp);
  u_int32_t magic;

  if (lcp) {
    magic = ntohl(*(u_int32_t *)MBUF_CTOP(bp));
    /* Tolerate echo replies with either magic number */
    if (magic != 0 && magic != lcp->his_magic && magic != lcp->want_magic) {
      LogPrintf(LogWARN,
                "RecvEchoRep: Bad magic: expected 0x%08x,  got: 0x%08x\n",
	        lcp->his_magic, magic);
      /*
       * XXX: We should send terminate request. But poor implementations may
       * die as a result.
       */
    }
    RecvEchoLqr(fp, bp);
  }
  pfree(bp);
}

static void
FsmRecvDiscReq(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  pfree(bp);
}

static void
FsmRecvIdent(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  pfree(bp);
}

static void
FsmRecvTimeRemain(struct fsm * fp, struct fsmheader * lhp, struct mbuf * bp)
{
  pfree(bp);
}

static void
FsmRecvResetReq(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  (*fp->fn->RecvResetReq)(fp);
  /*
   * All sendable compressed packets are queued in the PRI_NORMAL modem
   * output queue.... dump 'em to the priority queue so that they arrive
   * at the peer before our ResetAck.
   */
  link_SequenceQueue(fp->link);
  FsmOutput(fp, CODE_RESETACK, lhp->id, NULL, 0);
  pfree(bp);
}

static void
FsmRecvResetAck(struct fsm *fp, struct fsmheader *lhp, struct mbuf *bp)
{
  (*fp->fn->RecvResetAck)(fp, lhp->id);
  pfree(bp);
}

void
FsmInput(struct fsm *fp, struct mbuf *bp)
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
  if (lhp->code < fp->min_code || lhp->code > fp->max_code ||
      lhp->code > sizeof FsmCodes / sizeof *FsmCodes) {
    /*
     * Use a private id.  This is really a response-type packet, but we
     * MUST send a unique id for each REQ....
     */
    static u_char id;
    FsmOutput(fp, CODE_CODEREJ, id++, MBUF_CTOP(bp), bp->cnt);
    pfree(bp);
    return;
  }
  bp->offset += sizeof(struct fsmheader);
  bp->cnt -= sizeof(struct fsmheader);

  codep = FsmCodes + lhp->code - 1;
  if (lhp->id != fp->reqid && codep->check_reqid &&
      Enabled(fp->bundle, OPT_IDCHECK)) {
    LogPrintf(fp->LogLevel, "Recv%s(%d), dropped (expected %d)\n",
	      codep->name, lhp->id, fp->reqid);
    return;
  }

  LogPrintf(fp->LogLevel, "Recv%s(%d) state = %s\n",
	    codep->name, lhp->id, State2Nam(fp->state));

  if (LogIsKept(LogDEBUG))
    LogMemory();

  if (codep->inc_reqid && (lhp->id == fp->reqid ||
      (!Enabled(fp->bundle, OPT_IDCHECK) && codep->check_reqid)))
    fp->reqid++;	/* That's the end of that ``exchange''.... */

  (*codep->recv)(fp, lhp, bp);

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

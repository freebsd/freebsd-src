/*
 *	   PPP Compression Control Protocol (CCP) Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1994, Internet Initiative Japan, Inc. All rights reserverd.
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
 * $Id: ccp.c,v 1.30.2.15 1998/02/27 01:22:18 brian Exp $
 *
 *	TODO:
 *		o Support other compression protocols
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
#include "lcpproto.h"
#include "lcp.h"
#include "ccp.h"
#include "loadalias.h"
#include "vars.h"
#include "pred.h"
#include "deflate.h"
#include "bundle.h"
#include "descriptor.h"
#include "prompt.h"
#include "hdlc.h"
#include "throughput.h"
#include "link.h"
#include "chat.h"
#include "datalink.h"

static void CcpSendConfigReq(struct fsm *);
static void CcpSendTerminateReq(struct fsm *);
static void CcpSendTerminateAck(struct fsm *);
static void CcpDecodeConfig(struct fsm *, u_char *, int, int);
static void CcpLayerStart(struct fsm *);
static void CcpLayerFinish(struct fsm *);
static void CcpLayerUp(struct fsm *);
static void CcpLayerDown(struct fsm *);
static void CcpInitRestartCounter(struct fsm *);
static void CcpRecvResetReq(struct fsm *);
static void CcpRecvResetAck(struct fsm *, u_char);

static struct fsm_callbacks ccp_Callbacks = {
  CcpLayerUp,
  CcpLayerDown,
  CcpLayerStart,
  CcpLayerFinish,
  CcpInitRestartCounter,
  CcpSendConfigReq,
  CcpSendTerminateReq,
  CcpSendTerminateAck,
  CcpDecodeConfig,
  CcpRecvResetReq,
  CcpRecvResetAck
};

static char const *cftypes[] = {
  /* Check out the latest ``Compression Control Protocol'' rfc (rfc1962.txt) */
  "OUI",		/* 0: OUI */
  "PRED1",		/* 1: Predictor type 1 */
  "PRED2",		/* 2: Predictor type 2 */
  "PUDDLE",		/* 3: Puddle Jumber */
  "???", "???", "???", "???", "???", "???",
  "???", "???", "???", "???", "???", "???",
  "HWPPC",		/* 16: Hewlett-Packard PPC */
  "STAC",		/* 17: Stac Electronics LZS (rfc1974) */
  "MSPPC",		/* 18: Microsoft PPC */
  "GAND",		/* 19: Gandalf FZA (rfc1993) */
  "V42BIS",		/* 20: ARG->DATA.42bis compression */
  "BSD",		/* 21: BSD LZW Compress */
  "???",
  "LZS-DCP",		/* 23: LZS-DCP Compression Protocol (rfc1967) */
  "MAGNALINK/DEFLATE",	/* 24: Magnalink Variable Resource (rfc1975) */
			/* 24: Deflate (according to pppd-2.3.1) */
  "DCE",		/* 25: Data Circuit-Terminating Equip (rfc1976) */
  "DEFLATE",		/* 26: Deflate (rfc1979) */
};

#define NCFTYPES (sizeof cftypes/sizeof cftypes[0])

static const char *
protoname(int proto)
{
  if (proto < 0 || proto > NCFTYPES)
    return "none";
  return cftypes[proto];
}

/* We support these algorithms, and Req them in the given order */
static const struct ccp_algorithm *algorithm[] = {
  &DeflateAlgorithm,
  &Pred1Algorithm,
  &PppdDeflateAlgorithm
};

#define NALGORITHMS (sizeof algorithm/sizeof algorithm[0])

int
ccp_ReportStatus(struct cmdargs const *arg)
{
  struct ccp *ccp = bundle2ccp(arg->bundle, arg->cx ? arg->cx->name : NULL);

  prompt_Printf(&prompt, "%s [%s]\n", ccp->fsm.name,
                StateNames[ccp->fsm.state]);
  prompt_Printf(&prompt, "My protocol = %s, His protocol = %s\n",
                protoname(ccp->my_proto), protoname(ccp->his_proto));
  prompt_Printf(&prompt, "Output: %ld --> %ld,  Input: %ld --> %ld\n",
                ccp->uncompout, ccp->compout,
                ccp->compin, ccp->uncompin);
  return 0;
}

void
ccp_Init(struct ccp *ccp, struct bundle *bundle, struct link *l,
         const struct fsm_parent *parent)
{
  /* Initialise ourselves */
  fsm_Init(&ccp->fsm, "CCP", PROTO_CCP, CCP_MAXCODE, 10, LogCCP,
           bundle, l, parent, &ccp_Callbacks);
  ccp_Setup(ccp);
}

void
ccp_Setup(struct ccp *ccp)
{
  /* Set ourselves up for a startup */
  ccp->fsm.open_mode = 0;
  ccp->fsm.maxconfig = 10;
  ccp->his_proto = ccp->my_proto = -1;
  ccp->reset_sent = ccp->last_reset = -1;
  ccp->in_algorithm = ccp->out_algorithm = -1;
  ccp->his_reject = ccp->my_reject = 0;
  ccp->out_init = ccp->in_init = 0;
  ccp->uncompout = ccp->compout = 0;
  ccp->uncompin = ccp->compin = 0;
}

static void
CcpInitRestartCounter(struct fsm *fp)
{
  /* Set fsm timer load */
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

static void
CcpSendConfigReq(struct fsm *fp)
{
  /* Send config REQ please */
  struct ccp *ccp = fsm2ccp(fp);
  u_char *cp;
  int f;

  LogPrintf(LogCCP, "CcpSendConfigReq\n");
  cp = ReqBuff;
  ccp->my_proto = -1;
  ccp->out_algorithm = -1;
  for (f = 0; f < NALGORITHMS; f++)
    if (Enabled(algorithm[f]->Conf) && !REJECTED(ccp, algorithm[f]->id)) {
      struct lcp_opt o;

      (*algorithm[f]->o.Get)(&o);
      cp += LcpPutConf(LogCCP, cp, &o, cftypes[o.id],
                       (*algorithm[f]->Disp)(&o));
      ccp->my_proto = o.id;
      ccp->out_algorithm = f;
    }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
CcpSendResetReq(struct fsm *fp)
{
  /* We can't read our input - ask peer to reset */
  struct ccp *ccp = fsm2ccp(fp);
  LogPrintf(LogCCP, "SendResetReq(%d)\n", fp->reqid);
  ccp->reset_sent = fp->reqid;
  ccp->last_reset = -1;
  FsmOutput(fp, CODE_RESETREQ, fp->reqid, NULL, 0);
}

static void
CcpSendTerminateReq(struct fsm *fp)
{
  /* Term REQ just sent by FSM */
}

static void
CcpSendTerminateAck(struct fsm *fp)
{
  /* Send Term ACK please */
  LogPrintf(LogCCP, "CcpSendTerminateAck\n");
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

static void
CcpRecvResetReq(struct fsm *fp)
{
  /* Got a reset REQ, reset outgoing dictionary */
  struct ccp *ccp = fsm2ccp(fp);
  if (ccp->out_init)
    (*algorithm[ccp->out_algorithm]->o.Reset)();
}

static void
CcpLayerStart(struct fsm *fp)
{
  /* We're about to start up ! */
  LogPrintf(LogCCP, "CcpLayerStart.\n");
}

static void
CcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  struct ccp *ccp = fsm2ccp(fp);
  LogPrintf(LogCCP, "CcpLayerFinish.\n");
  if (ccp->in_init) {
    (*algorithm[ccp->in_algorithm]->i.Term)();
    ccp->in_init = 0;
  }
  if (ccp->out_init) {
    (*algorithm[ccp->out_algorithm]->o.Term)();
    ccp->out_init = 0;
  }
}

static void
CcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  LogPrintf(LogCCP, "CcpLayerDown.\n");
}

/*
 *  Called when CCP has reached the OPEN state
 */
static void
CcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct ccp *ccp = fsm2ccp(fp);
  LogPrintf(LogCCP, "CcpLayerUp.\n");
  if (!ccp->in_init && ccp->in_algorithm >= 0 &&
      ccp->in_algorithm < NALGORITHMS)
    if ((*algorithm[ccp->in_algorithm]->i.Init)())
      ccp->in_init = 1;
    else {
      LogPrintf(LogERROR, "%s (in) initialisation failure\n",
                protoname(ccp->his_proto));
      ccp->his_proto = ccp->my_proto = -1;
      FsmClose(fp);
    }
  if (!ccp->out_init && ccp->out_algorithm >= 0 &&
      ccp->out_algorithm < NALGORITHMS)
    if ((*algorithm[ccp->out_algorithm]->o.Init)())
      ccp->out_init = 1;
    else {
      LogPrintf(LogERROR, "%s (out) initialisation failure\n",
                protoname(ccp->my_proto));
      ccp->his_proto = ccp->my_proto = -1;
      FsmClose(fp);
    }
  LogPrintf(LogCCP, "Out = %s[%d], In = %s[%d]\n",
            protoname(ccp->my_proto), ccp->my_proto,
            protoname(ccp->his_proto), ccp->his_proto);
}

static void
CcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type)
{
  /* Deal with incoming data */
  struct ccp *ccp = fsm2ccp(fp);
  int type, length;
  int f;

  ackp = AckBuff;
  nakp = NakBuff;
  rejp = RejBuff;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];
    if (type < NCFTYPES)
      LogPrintf(LogCCP, " %s[%d]\n", cftypes[type], length);
    else
      LogPrintf(LogCCP, " ???[%d]\n", length);

    for (f = NALGORITHMS-1; f > -1; f--)
      if (algorithm[f]->id == type)
        break;

    if (f == -1) {
      /* Don't understand that :-( */
      if (mode_type == MODE_REQ) {
        ccp->my_reject |= (1 << type);
        memcpy(rejp, cp, length);
        rejp += length;
      }
    } else {
      struct lcp_opt o;

      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(algorithm[f]->Conf) && ccp->in_algorithm == -1) {
	  memcpy(&o, cp, length);
          switch ((*algorithm[f]->i.Set)(&o)) {
          case MODE_REJ:
	    memcpy(rejp, &o, o.len);
	    rejp += o.len;
            break;
          case MODE_NAK:
	    memcpy(nakp, &o, o.len);
	    nakp += o.len;
            break;
          case MODE_ACK:
	    memcpy(ackp, cp, length);
	    ackp += length;
	    ccp->his_proto = type;
            ccp->in_algorithm = f;		/* This one'll do ! */
            break;
          }
	} else {
	  memcpy(rejp, cp, length);
	  rejp += length;
	}
	break;
      case MODE_NAK:
	memcpy(&o, cp, length);
        if ((*algorithm[f]->o.Set)(&o) == MODE_ACK)
          ccp->my_proto = algorithm[f]->id;
        else {
	  ccp->his_reject |= (1 << type);
	  ccp->my_proto = -1;
        }
        break;
      case MODE_REJ:
	ccp->his_reject |= (1 << type);
	ccp->my_proto = -1;
	break;
      }
    }

    plen -= length;
    cp += length;
  }

  if (rejp != RejBuff) {
    ackp = AckBuff;	/* let's not send both ! */
    if (!ccp->in_init) {
      ccp->his_proto = -1;
      ccp->in_algorithm = -1;
    }
  }
}

void
CcpInput(struct ccp *ccp, struct bundle *bundle, struct mbuf *bp)
{
  /* Got PROTO_CCP from link */
  if (bundle_Phase(bundle) == PHASE_NETWORK)
    FsmInput(&ccp->fsm, bp);
  else if (bundle_Phase(bundle) < PHASE_NETWORK) {
    LogPrintf(LogCCP, "Error: Unexpected CCP in phase %s (ignored)\n",
              bundle_PhaseName(bundle));
    pfree(bp);
  }
}

static void
CcpRecvResetAck(struct fsm *fp, u_char id)
{
  /* Got a reset ACK, reset incoming dictionary */
  struct ccp *ccp = fsm2ccp(fp);

  if (ccp->reset_sent != -1) {
    if (id != ccp->reset_sent) {
      LogPrintf(LogWARN, "CCP: Incorrect ResetAck (id %d, not %d) ignored\n",
                id, ccp->reset_sent);
      return;
    }
    /* Whaddaya know - a correct reset ack */
  } else if (id == ccp->last_reset)
    LogPrintf(LogCCP, "Duplicate ResetAck (resetting again)\n");
  else {
    LogPrintf(LogWARN, "CCP: Unexpected ResetAck (id %d) ignored\n", id);
    return;
  }

  ccp->last_reset = ccp->reset_sent;
  ccp->reset_sent = -1;
  if (ccp->in_init)
    (*algorithm[ccp->in_algorithm]->i.Reset)();
}

int
ccp_Output(struct ccp *ccp, struct link *l, int pri, u_short proto,
           struct mbuf *m)
{
  /* Compress outgoing Network Layer data */
  if ((proto & 0xfff1) == 0x21 && ccp->fsm.state == ST_OPENED && ccp->out_init)
    return (*algorithm[ccp->out_algorithm]->o.Write)(ccp, l, pri, proto, m);
  return 0;
}

struct mbuf *
ccp_Decompress(struct ccp *ccp, u_short *proto, struct mbuf *bp)
{
  /*
   * If proto isn't PROTO_COMPD, we still want to pass it to the
   * decompression routines so that the dictionary's updated
   */
  if (ccp->fsm.state == ST_OPENED)
    if (*proto == PROTO_COMPD) {
      /* Decompress incoming data */
      if (ccp->reset_sent != -1) {
        /* Send another REQ and put the packet in the bit bucket */
        LogPrintf(LogCCP, "ReSendResetReq(%d)\n", ccp->reset_sent);
        FsmOutput(&ccp->fsm, CODE_RESETREQ, ccp->reset_sent, NULL, 0);
      } else if (ccp->in_init)
        return (*algorithm[ccp->in_algorithm]->i.Read)(ccp, proto, bp);
      pfree(bp);
      bp = NULL;
    } else if ((*proto & 0xfff1) == 0x21 && ccp->in_init)
      /* Add incoming Network Layer traffic to our dictionary */
      (*algorithm[ccp->in_algorithm]->i.DictSetup)(ccp, *proto, bp);

  return bp;
}

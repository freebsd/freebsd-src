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
 * $Id: ccp.c,v 1.30.2.3 1998/01/30 19:45:27 brian Exp $
 *
 *	TODO:
 *		o Support other compression protocols
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
#include "lcp.h"
#include "ccp.h"
#include "phase.h"
#include "loadalias.h"
#include "vars.h"
#include "pred.h"
#include "deflate.h"

static void CcpSendConfigReq(struct fsm *);
static void CcpSendTerminateReq(struct fsm *);
static void CcpSendTerminateAck(struct fsm *);
static void CcpDecodeConfig(u_char *, int, int);
static void CcpLayerStart(struct fsm *);
static void CcpLayerFinish(struct fsm *);
static void CcpLayerUp(struct fsm *);
static void CcpLayerDown(struct fsm *);
static void CcpInitRestartCounter(struct fsm *);

struct ccpstate CcpInfo = {
  {
    "CCP",
    PROTO_CCP,
    CCP_MAXCODE,
    0,
    ST_INITIAL,
    0, 0, 0,
    {0, 0, 0, NULL, NULL, NULL},	/* FSM timer */
    {0, 0, 0, NULL, NULL, NULL},	/* Open timer */
    {0, 0, 0, NULL, NULL, NULL},	/* Stopped timer */
    LogCCP,
    NULL,
    CcpLayerUp,
    CcpLayerDown,
    CcpLayerStart,
    CcpLayerFinish,
    CcpInitRestartCounter,
    CcpSendConfigReq,
    CcpSendTerminateReq,
    CcpSendTerminateAck,
    CcpDecodeConfig,
  },
  -1, -1, -1, -1, -1, -1
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
ReportCcpStatus(struct cmdargs const *arg)
{
  if (VarTerm) {
    fprintf(VarTerm, "%s [%s]\n", CcpInfo.fsm.name,
            StateNames[CcpInfo.fsm.state]);
    fprintf(VarTerm, "My protocol = %s, His protocol = %s\n",
            protoname(CcpInfo.my_proto), protoname(CcpInfo.his_proto));
    fprintf(VarTerm, "Output: %ld --> %ld,  Input: %ld --> %ld\n",
            CcpInfo.uncompout, CcpInfo.compout,
            CcpInfo.compin, CcpInfo.uncompin);
  }
  return 0;
}

void
CcpInit(struct link *l)
{
  /* Initialise ourselves */
  FsmInit(&CcpInfo.fsm, l);
  CcpInfo.his_proto = CcpInfo.my_proto = -1;
  CcpInfo.reset_sent = CcpInfo.last_reset = -1;
  CcpInfo.in_algorithm = CcpInfo.out_algorithm = -1;
  CcpInfo.his_reject = CcpInfo.my_reject = 0;
  CcpInfo.out_init = CcpInfo.in_init = 0;
  CcpInfo.uncompout = CcpInfo.compout = 0;
  CcpInfo.uncompin = CcpInfo.compin = 0;
  CcpInfo.fsm.maxconfig = 10;
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
  u_char *cp;
  int f;

  LogPrintf(LogCCP, "CcpSendConfigReq\n");
  cp = ReqBuff;
  CcpInfo.my_proto = -1;
  CcpInfo.out_algorithm = -1;
  for (f = 0; f < NALGORITHMS; f++)
    if (Enabled(algorithm[f]->Conf) && !REJECTED(&CcpInfo, algorithm[f]->id)) {
      struct lcp_opt o;

      (*algorithm[f]->o.Get)(&o);
      cp += LcpPutConf(LogCCP, cp, &o, cftypes[o.id],
                       (*algorithm[f]->Disp)(&o));
      CcpInfo.my_proto = o.id;
      CcpInfo.out_algorithm = f;
    }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
CcpSendResetReq(struct fsm *fp)
{
  /* We can't read our input - ask peer to reset */
  LogPrintf(LogCCP, "SendResetReq(%d)\n", fp->reqid);
  CcpInfo.reset_sent = fp->reqid;
  CcpInfo.last_reset = -1;
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

void
CcpRecvResetReq(struct fsm *fp)
{
  /* Got a reset REQ, reset outgoing dictionary */
  if (CcpInfo.out_init)
    (*algorithm[CcpInfo.out_algorithm]->o.Reset)();
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
  LogPrintf(LogCCP, "CcpLayerFinish.\n");
  if (CcpInfo.in_init) {
    (*algorithm[CcpInfo.in_algorithm]->i.Term)();
    CcpInfo.in_init = 0;
  }
  if (CcpInfo.out_init) {
    (*algorithm[CcpInfo.out_algorithm]->o.Term)();
    CcpInfo.out_init = 0;
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
  LogPrintf(LogCCP, "CcpLayerUp(%d).\n", fp->state);
  if (!CcpInfo.in_init && CcpInfo.in_algorithm >= 0 &&
      CcpInfo.in_algorithm < NALGORITHMS)
    if ((*algorithm[CcpInfo.in_algorithm]->i.Init)())
      CcpInfo.in_init = 1;
    else {
      LogPrintf(LogERROR, "%s (in) initialisation failure\n",
                protoname(CcpInfo.his_proto));
      CcpInfo.his_proto = CcpInfo.my_proto = -1;
      FsmClose(fp);
    }
  if (!CcpInfo.out_init && CcpInfo.out_algorithm >= 0 &&
      CcpInfo.out_algorithm < NALGORITHMS)
    if ((*algorithm[CcpInfo.out_algorithm]->o.Init)())
      CcpInfo.out_init = 1;
    else {
      LogPrintf(LogERROR, "%s (out) initialisation failure\n",
                protoname(CcpInfo.my_proto));
      CcpInfo.his_proto = CcpInfo.my_proto = -1;
      FsmClose(fp);
    }
  LogPrintf(LogCCP, "Out = %s[%d], In = %s[%d]\n",
            protoname(CcpInfo.my_proto), CcpInfo.my_proto,
            protoname(CcpInfo.his_proto), CcpInfo.his_proto);
}

void
CcpUp()
{
  /* Lower layers are ready.... go */
  FsmUp(&CcpInfo.fsm);
  LogPrintf(LogCCP, "CCP Up event!!\n");
}

void
CcpDown()
{
  /* Physical link is gone - sudden death */
  if (CcpInfo.fsm.state >= ST_CLOSED) {
    FsmDown(&CcpInfo.fsm);
    /* FsmDown() results in a CcpLayerDown() if we're currently open. */
    CcpLayerFinish(&CcpInfo.fsm);
  }
}

void
CcpOpen()
{
  /* Start CCP please */
  int f;

  for (f = 0; f < NALGORITHMS; f++)
    if (Enabled(algorithm[f]->Conf)) {
      CcpInfo.fsm.open_mode = 0;
      FsmOpen(&CcpInfo.fsm);
      break;
    }

  if (f == NALGORITHMS)
    for (f = 0; f < NALGORITHMS; f++)
      if (Acceptable(algorithm[f]->Conf)) {
        CcpInfo.fsm.open_mode = OPEN_PASSIVE;
        FsmOpen(&CcpInfo.fsm);
        break;
      }
}

static void
CcpDecodeConfig(u_char *cp, int plen, int mode_type)
{
  /* Deal with incoming data */
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
        CcpInfo.my_reject |= (1 << type);
        memcpy(rejp, cp, length);
        rejp += length;
      }
    } else {
      struct lcp_opt o;

      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(algorithm[f]->Conf) && CcpInfo.in_algorithm == -1) {
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
	    CcpInfo.his_proto = type;
            CcpInfo.in_algorithm = f;		/* This one'll do ! */
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
          CcpInfo.my_proto = algorithm[f]->id;
        else {
	  CcpInfo.his_reject |= (1 << type);
	  CcpInfo.my_proto = -1;
        }
        break;
      case MODE_REJ:
	CcpInfo.his_reject |= (1 << type);
	CcpInfo.my_proto = -1;
	break;
      }
    }

    plen -= length;
    cp += length;
  }

  if (rejp != RejBuff) {
    ackp = AckBuff;	/* let's not send both ! */
    if (!CcpInfo.in_init) {
      CcpInfo.his_proto = -1;
      CcpInfo.in_algorithm = -1;
    }
  }
}

void
CcpInput(struct mbuf *bp)
{
  /* Got PROTO_CCP from link */
  if (phase == PHASE_NETWORK)
    FsmInput(&CcpInfo.fsm, bp);
  else {
    if (phase < PHASE_NETWORK)
      LogPrintf(LogCCP, "Error: Unexpected CCP in phase %d (ignored)\n", phase);
    pfree(bp);
  }
}

void
CcpResetInput(u_char id)
{
  /* Got a reset ACK, reset incoming dictionary */
  if (CcpInfo.reset_sent != -1) {
    if (id != CcpInfo.reset_sent) {
      LogPrintf(LogWARN, "CCP: Incorrect ResetAck (id %d, not %d) ignored\n",
                id, CcpInfo.reset_sent);
      return;
    }
    /* Whaddaya know - a correct reset ack */
  } else if (id == CcpInfo.last_reset)
    LogPrintf(LogCCP, "Duplicate ResetAck (resetting again)\n");
  else {
    LogPrintf(LogWARN, "CCP: Unexpected ResetAck (id %d) ignored\n", id);
    return;
  }

  CcpInfo.last_reset = CcpInfo.reset_sent;
  CcpInfo.reset_sent = -1;
  if (CcpInfo.in_init)
    (*algorithm[CcpInfo.in_algorithm]->i.Reset)();
}

int
CcpOutput(struct link *l, int pri, u_short proto, struct mbuf *m)
{
  /* Compress outgoing data */
  if (CcpInfo.out_init)
    return (*algorithm[CcpInfo.out_algorithm]->o.Write)(l, pri, proto, m);
  return 0;
}

struct mbuf *
CompdInput(u_short *proto, struct mbuf *m)
{
  /* Decompress incoming data */
  if (CcpInfo.reset_sent != -1) {
    /* Send another REQ and put the packet in the bit bucket */
    LogPrintf(LogCCP, "ReSendResetReq(%d)\n", CcpInfo.reset_sent);
    FsmOutput(&CcpInfo.fsm, CODE_RESETREQ, CcpInfo.reset_sent, NULL, 0);
    pfree(m);
  } else if (CcpInfo.in_init)
    return (*algorithm[CcpInfo.in_algorithm]->i.Read)(proto, m);
  return NULL;
}

void
CcpDictSetup(u_short proto, struct mbuf *m)
{
  /* Add incoming data to the dictionary */
  if (CcpInfo.in_init)
    (*algorithm[CcpInfo.in_algorithm]->i.DictSetup)(proto, m);
}

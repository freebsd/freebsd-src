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
 * $Id: ccp.c,v 1.29 1998/01/20 22:47:34 brian Exp $
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

struct ccpstate CcpInfo = { -1, -1, -1, -1 };

static void CcpSendConfigReq(struct fsm *);
static void CcpSendTerminateReq(struct fsm *);
static void CcpSendTerminateAck(struct fsm *);
static void CcpDecodeConfig(u_char *, int, int);
static void CcpLayerStart(struct fsm *);
static void CcpLayerFinish(struct fsm *);
static void CcpLayerUp(struct fsm *);
static void CcpLayerDown(struct fsm *);
static void CcpInitRestartCounter(struct fsm *);

struct fsm CcpFsm = {
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

  CcpLayerUp,
  CcpLayerDown,
  CcpLayerStart,
  CcpLayerFinish,
  CcpInitRestartCounter,
  CcpSendConfigReq,
  CcpSendTerminateReq,
  CcpSendTerminateAck,
  CcpDecodeConfig,
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

static int in_algorithm = -1;
static int out_algorithm = -1;
#define NALGORITHMS (sizeof algorithm/sizeof algorithm[0])

int
ReportCcpStatus(struct cmdargs const *arg)
{
  if (VarTerm) {
    fprintf(VarTerm, "%s [%s]\n", CcpFsm.name, StateNames[CcpFsm.state]);
    fprintf(VarTerm, "My protocol = %s, His protocol = %s\n",
            protoname(CcpInfo.my_proto), protoname(CcpInfo.his_proto));
    fprintf(VarTerm, "Output: %ld --> %ld,  Input: %ld --> %ld\n",
            CcpInfo.uncompout, CcpInfo.compout,
            CcpInfo.compin, CcpInfo.uncompin);
  }
  return 0;
}

static void
ccpstateInit(void)
{
  if (CcpInfo.in_init)
    (*algorithm[in_algorithm]->i.Term)();
  if (CcpInfo.out_init)
    (*algorithm[out_algorithm]->o.Term)();
  in_algorithm = -1;
  out_algorithm = -1;
  memset(&CcpInfo, '\0', sizeof CcpInfo);
  CcpInfo.his_proto = CcpInfo.my_proto = -1;
  CcpInfo.reset_sent = CcpInfo.last_reset = -1;
}

void
CcpInit()
{
  FsmInit(&CcpFsm);
  ccpstateInit();
  CcpFsm.maxconfig = 10;
}

static void
CcpInitRestartCounter(struct fsm *fp)
{
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

static void
CcpSendConfigReq(struct fsm *fp)
{
  u_char *cp;
  int f;

  LogPrintf(LogCCP, "CcpSendConfigReq\n");
  cp = ReqBuff;
  CcpInfo.my_proto = -1;
  out_algorithm = -1;
  for (f = 0; f < NALGORITHMS; f++)
    if (Enabled(algorithm[f]->Conf) && !REJECTED(&CcpInfo, algorithm[f]->id)) {
      struct lcp_opt o;

      (*algorithm[f]->o.Get)(&o);
      cp += LcpPutConf(LogCCP, cp, &o, cftypes[o.id],
                       (*algorithm[f]->Disp)(&o));
      CcpInfo.my_proto = o.id;
      out_algorithm = f;
    }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
CcpSendResetReq(struct fsm *fp)
{
  LogPrintf(LogCCP, "SendResetReq(%d)\n", fp->reqid);
  CcpInfo.reset_sent = fp->reqid;
  CcpInfo.last_reset = -1;
  FsmOutput(fp, CODE_RESETREQ, fp->reqid, NULL, 0);
}

static void
CcpSendTerminateReq(struct fsm *fp)
{
  /* XXX: No code yet */
}

static void
CcpSendTerminateAck(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpSendTerminateAck\n");
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

void
CcpRecvResetReq(struct fsm *fp)
{
  if (out_algorithm >= 0 && out_algorithm < NALGORITHMS)
    (*algorithm[out_algorithm]->o.Reset)();
}

static void
CcpLayerStart(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpLayerStart.\n");
}

static void
CcpLayerFinish(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpLayerFinish.\n");
  ccpstateInit();
}

static void
CcpLayerDown(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpLayerDown.\n");
  ccpstateInit();
}

/*
 *  Called when CCP has reached the OPEN state
 */
static void
CcpLayerUp(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpLayerUp(%d).\n", fp->state);
  LogPrintf(LogCCP, "Out = %s[%d], In = %s[%d]\n",
            protoname(CcpInfo.my_proto), CcpInfo.my_proto,
            protoname(CcpInfo.his_proto), CcpInfo.his_proto);
  if (!CcpInfo.in_init && in_algorithm >= 0 && in_algorithm < NALGORITHMS) {
    (*algorithm[in_algorithm]->i.Init)();
    CcpInfo.in_init = 1;
  }
  if (!CcpInfo.out_init && out_algorithm >= 0 && out_algorithm < NALGORITHMS) {
    (*algorithm[out_algorithm]->o.Init)();
    CcpInfo.out_init = 1;
  }
}

void
CcpUp()
{
  FsmUp(&CcpFsm);
  LogPrintf(LogCCP, "CCP Up event!!\n");
}

void
CcpOpen()
{
  int f;

  for (f = 0; f < NALGORITHMS; f++)
    if (Enabled(algorithm[f]->Conf)) {
      CcpFsm.open_mode = 0;
      FsmOpen(&CcpFsm);
      break;
    }

  if (f == NALGORITHMS)
    for (f = 0; f < NALGORITHMS; f++)
      if (Acceptable(algorithm[f]->Conf)) {
        CcpFsm.open_mode = OPEN_PASSIVE;
        FsmOpen(&CcpFsm);
        break;
      }
}

static void
CcpDecodeConfig(u_char *cp, int plen, int mode_type)
{
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
	if (Acceptable(algorithm[f]->Conf) && in_algorithm == -1) {
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
            in_algorithm = f;		/* This one'll do ! */
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
    CcpInfo.his_proto = -1;
    in_algorithm = -1;
  }
}

void
CcpInput(struct mbuf *bp)
{
  if (phase == PHASE_NETWORK)
    FsmInput(&CcpFsm, bp);
  else {
    if (phase > PHASE_NETWORK)
      LogPrintf(LogCCP, "Error: Unexpected CCP in phase %d\n", phase);
    pfree(bp);
  }
}

void
CcpResetInput(u_char id)
{
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
  if (in_algorithm >= 0 && in_algorithm < NALGORITHMS)
    (*algorithm[in_algorithm]->i.Reset)();
}

int
CcpOutput(int pri, u_short proto, struct mbuf *m)
{
  if (out_algorithm >= 0 && out_algorithm < NALGORITHMS)
    return (*algorithm[out_algorithm]->o.Write)(pri, proto, m);
  return 0;
}

struct mbuf *
CompdInput(u_short *proto, struct mbuf *m)
{
  if (CcpInfo.reset_sent != -1) {
    /* Send another REQ and put the packet in the bit bucket */
    LogPrintf(LogCCP, "ReSendResetReq(%d)\n", CcpInfo.reset_sent);
    FsmOutput(&CcpFsm, CODE_RESETREQ, CcpInfo.reset_sent, NULL, 0);
    pfree(m);
  } else if (in_algorithm >= 0 && in_algorithm < NALGORITHMS)
    return (*algorithm[in_algorithm]->i.Read)(proto, m);
  return NULL;
}

void
CcpDictSetup(u_short proto, struct mbuf *m)
{
  if (in_algorithm >= 0 && in_algorithm < NALGORITHMS)
    (*algorithm[in_algorithm]->i.DictSetup)(proto, m);
}

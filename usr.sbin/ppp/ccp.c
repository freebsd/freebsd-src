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
 * $Id: ccp.c,v 1.19 1997/11/14 15:39:14 brian Exp $
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

struct ccpstate CcpInfo;

static void CcpSendConfigReq(struct fsm *);
static void CcpSendTerminateReq(struct fsm *);
static void CcpSendTerminateAck(struct fsm *);
static void CcpDecodeConfig(u_char *, int, int);
static void CcpLayerStart(struct fsm *);
static void CcpLayerFinish(struct fsm *);
static void CcpLayerUp(struct fsm *);
static void CcpLayerDown(struct fsm *);
static void CcpInitRestartCounter(struct fsm *);

#define	REJECTED(p, x)	(p->his_reject & (1<<x))

struct fsm CcpFsm = {
  "CCP",
  PROTO_CCP,
  CCP_MAXCODE,
  OPEN_ACTIVE,
  ST_INITIAL,
  0, 0, 0,
  0,
  {0, 0, 0, NULL, NULL, NULL},
  {0, 0, 0, NULL, NULL, NULL},
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
   "OUI",	/* 0: OUI */
   "PRED1",	/* 1: Predictor type 1 */
   "PRED2",	/* 2: Predictor type 2 */
   "PUDDLE",	/* 3: Puddle Jumber */
   "???", "???", "???", "???", "???", "???",
   "???", "???", "???", "???", "???", "???",
   "HWPPC",	/* 16: Hewlett-Packard PPC */
   "STAC",	/* 17: Stac Electronics LZS */
   "MSPPC",	/* 18: Microsoft PPC */
   "GAND",	/* 19: Gandalf FZA */
   "V42BIS",	/* 20: ARG->DATA.42bis compression */
   "BSD",	/* BSD LZW Compress */
};

#define NCFTYPES (sizeof(cftypes)/sizeof(char *))

int
ReportCcpStatus(struct cmdargs const *arg)
{
  struct ccpstate *icp = &CcpInfo;
  struct fsm *fp = &CcpFsm;

  if (VarTerm) {
    fprintf(VarTerm, "%s [%s]\n", fp->name, StateNames[fp->state]);
    fprintf(VarTerm, "myproto = %s, hisproto = %s\n",
	    cftypes[icp->want_proto], cftypes[icp->his_proto]);
    fprintf(VarTerm, "Input: %ld --> %ld,  Output: %ld --> %ld\n",
	    icp->orgin, icp->compin, icp->orgout, icp->compout);
  }
  return 0;
}

void
CcpInit()
{
  struct ccpstate *icp = &CcpInfo;

  FsmInit(&CcpFsm);
  memset(icp, '\0', sizeof(struct ccpstate));
  if (Enabled(ConfPred1))
    icp->want_proto = TY_PRED1;
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
  struct ccpstate *icp = &CcpInfo;

  cp = ReqBuff;
  LogPrintf(LogCCP, "CcpSendConfigReq\n");
  if (icp->want_proto && !REJECTED(icp, TY_PRED1)) {
    *cp++ = TY_PRED1;
    *cp++ = 2;
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
CcpSendResetReq(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpSendResetReq\n");
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
  Pred1Init(2);			/* Initialize Output part */
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
}

static void
CcpLayerDown(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpLayerDown.\n");
}

/*
 *  Called when CCP has reached to OPEN state
 */
static void
CcpLayerUp(struct fsm *fp)
{
  LogPrintf(LogCCP, "CcpLayerUp(%d).\n", fp->state);
  LogPrintf(LogCCP, "myproto = %d, hisproto = %d\n",
	    CcpInfo.want_proto, CcpInfo.his_proto);
  Pred1Init(3);			/* Initialize Input and Output */
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
  if (Enabled(ConfPred1))
    FsmOpen(&CcpFsm);
}

static void
CcpDecodeConfig(u_char *cp, int plen, int mode_type)
{
  int type, length;
  char tbuff[100];

  ackp = AckBuff;
  nakp = NakBuff;
  rejp = RejBuff;

  while (plen >= sizeof(struct fsmconfig)) {
    if (plen < 0)
      break;
    type = *cp;
    length = cp[1];
    if (type < NCFTYPES)
      snprintf(tbuff, sizeof(tbuff), " %s[%d] ", cftypes[type], length);
    else
      snprintf(tbuff, sizeof(tbuff), " ");

    LogPrintf(LogCCP, "%s\n", tbuff);

    switch (type) {
    case TY_PRED1:
      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(ConfPred1)) {
	  memcpy(ackp, cp, length);
	  ackp += length;
	  CcpInfo.his_proto = type;
	} else {
	  memcpy(rejp, cp, length);
	  rejp += length;
	}
	break;
      case MODE_NAK:
      case MODE_REJ:
	CcpInfo.his_reject |= (1 << type);
	CcpInfo.want_proto = 0;
	break;
      }
      break;
    case TY_BSD:
    default:
      CcpInfo.my_reject |= (1 << type);
      memcpy(rejp, cp, length);
      rejp += length;
      break;
    }
    plen -= length;
    cp += length;
  }
}

void
CcpInput(struct mbuf *bp)
{
  if (phase == PHASE_NETWORK)
    FsmInput(&CcpFsm, bp);
  else {
    if (phase > PHASE_NETWORK)
      LogPrintf(LogERROR, "Unexpected CCP in phase %d\n", phase);
    pfree(bp);
  }
}

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
 * $Id: ccp.c,v 1.6 1996/01/30 20:04:24 phk Exp $
 *
 *	TODO:
 *		o Support other compression protocols
 */
#include "fsm.h"
#include "lcpproto.h"
#include "lcp.h"
#include "ccp.h"
#include "phase.h"
#include "vars.h"
#include "pred.h"
#include "cdefs.h"

extern void PutConfValue __P((void));

struct ccpstate CcpInfo;

static void CcpSendConfigReq __P((struct fsm *));
static void CcpSendTerminateReq __P((struct fsm *fp));
static void CcpSendTerminateAck __P((struct fsm *fp));
static void CcpDecodeConfig __P((u_char *cp, int flen, int mode));
static void CcpLayerStart __P((struct fsm *));
static void CcpLayerFinish __P((struct fsm *));
static void CcpLayerUp __P((struct fsm *));
static void CcpLayerDown __P((struct fsm *));
static void CcpInitRestartCounter __P((struct fsm *));

#define	REJECTED(p, x)	(p->his_reject & (1<<x))

struct fsm CcpFsm = {
  "CCP",
  PROTO_CCP,
  CCP_MAXCODE,
  OPEN_ACTIVE,
  ST_INITIAL,
  0, 0, 0,

  0,
  { 0, 0, 0, NULL, NULL, NULL },

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
/*  0 */  "OUI",    "PRED1", "PRED2", "PUDDLE",
/*  4 */  "???",    "???",   "???",   "???",
/*  8 */  "???",    "???",   "???",   "???",
/* 12 */  "???",    "???",   "???",   "???",
/* 16 */  "HWPPC",  "STAC",  "MSPPC", "GAND",
/* 20 */  "V42BIS", "BSD",
};

void
ReportCcpStatus()
{
  struct ccpstate *icp = &CcpInfo;
  struct fsm *fp = &CcpFsm;

  printf("%s [%s]\n", fp->name, StateNames[fp->state]);
  printf("myproto = %s, hisproto = %s\n",
	cftypes[icp->want_proto], cftypes[icp->his_proto]);
  printf("Input: %ld --> %ld,  Output: %ld --> %ld\n",
	icp->orgin, icp->compin, icp->orgout, icp->compout);
}

void
CcpInit()
{
  struct ccpstate *icp = &CcpInfo;

  FsmInit(&CcpFsm);
  bzero(icp, sizeof(struct ccpstate));
  if (Enabled(ConfPred1))
    icp->want_proto = TY_PRED1;
  CcpFsm.maxconfig = 10;
}

static void
CcpInitRestartCounter(fp)
struct fsm *fp;
{
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

static void
CcpSendConfigReq(fp)
struct fsm *fp;
{
  u_char *cp;
  struct ccpstate *icp = &CcpInfo;

  cp = ReqBuff;
  LogPrintf(LOG_LCP_BIT, "%s: SendConfigReq\n", fp->name);
  if (icp->want_proto && !REJECTED(icp, TY_PRED1)) {
    *cp++ = TY_PRED1; *cp++ = 2;
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
CcpSendResetReq(fp)
struct fsm *fp;
{
  Pred1Init(1);		/* Initialize Input part */
  LogPrintf(LOG_LCP_BIT, "%s: SendResetReq\n", fp->name);
  FsmOutput(fp, CODE_RESETREQ, fp->reqid, NULL, 0);
}

static void
CcpSendTerminateReq(fp)
struct fsm *fp;
{
  /* XXX: No code yet */
}

static void
CcpSendTerminateAck(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "  %s: SendTerminateAck\n", fp->name);
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

void
CcpRecvResetReq(fp)
struct fsm *fp;
{
  Pred1Init(2);		/* Initialize Output part */
}

static void
CcpLayerStart(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: LayerStart.\n", fp->name);
}

static void
CcpLayerFinish(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: LayerFinish.\n", fp->name);
}

static void
CcpLayerDown(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: LayerDown.\n", fp->name);
}

/*
 *  Called when CCP has reached to OPEN state
 */
static void
CcpLayerUp(fp)
struct fsm *fp;
{
#ifdef VERBOSE
  fprintf(stderr, "%s: LayerUp(%d).\r\n", fp->name, fp->state);
#endif
  LogPrintf(LOG_LCP_BIT, "%s: LayerUp.\n", fp->name);
  LogPrintf(LOG_LCP_BIT, "myproto = %d, hisproto = %d\n",
	CcpInfo.want_proto, CcpInfo.his_proto);
  Pred1Init(3);		/* Initialize Input and Output */
}

void
CcpUp()
{
  FsmUp(&CcpFsm);
  LogPrintf(LOG_LCP_BIT, "CCP Up event!!\n");
}

void
CcpOpen()
{
  if (Enabled(ConfPred1))
    FsmOpen(&CcpFsm);
}

static void
CcpDecodeConfig(cp, plen, mode)
u_char *cp;
int plen;
int mode;
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
    if (type <= TY_BSD)
      sprintf(tbuff, " %s[%d] ", cftypes[type], length);
    else
      sprintf(tbuff, " ");

    LogPrintf(LOG_LCP_BIT, "%s\n", tbuff);

    switch (type) {
    case TY_PRED1:
      switch (mode) {
      case MODE_REQ:
	if (Acceptable(ConfPred1)) {
	  bcopy(cp, ackp, length);
	  ackp += length;
	  CcpInfo.his_proto = type;
	} else {
	  bcopy(cp, rejp, length);
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
      bcopy(cp, rejp, length);
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
    logprintf("ccp in phase %d\n", phase);
    pfree(bp);
  }
}

/*
 *	      PPP Link Control Protocol (LCP) Module
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
 * $Id: lcp.c,v 1.45 1997/11/14 15:39:15 brian Exp $
 *
 * TODO:
 *      o Validate magic number received from peer.
 *	o Limit data field length by MRU
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_tun.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "lcpproto.h"
#include "os.h"
#include "hdlc.h"
#include "ccp.h"
#include "lqr.h"
#include "phase.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "auth.h"
#include "pap.h"
#include "chap.h"
#include "async.h"
#include "main.h"
#include "ip.h"
#include "modem.h"
#include "tun.h"

struct lcpstate LcpInfo;

static void LcpSendConfigReq(struct fsm *);
static void LcpSendTerminateReq(struct fsm *);
static void LcpSendTerminateAck(struct fsm *);
static void LcpDecodeConfig(u_char *, int, int);
static void LcpInitRestartCounter(struct fsm *);
static void LcpLayerUp(struct fsm *);
static void LcpLayerDown(struct fsm *);
static void LcpLayerStart(struct fsm *);
static void LcpLayerFinish(struct fsm *);

#define	REJECTED(p, x)	(p->his_reject & (1<<x))

static char *cftypes[] = {
  /* Check out the latest ``Assigned numbers'' rfc (rfc1700.txt) */
  "???",
  "MRU",	/* 1: Maximum-Receive-Unit */
  "ACCMAP",	/* 2: Async-Control-Character-Map */
  "AUTHPROTO",	/* 3: Authentication-Protocol */
  "QUALPROTO",	/* 4: Quality-Protocol */
  "MAGICNUM",	/* 5: Magic-Number */
  "RESERVED",	/* 6: RESERVED */
  "PROTOCOMP",	/* 7: Protocol-Field-Compression */
  "ACFCOMP",	/* 8: Address-and-Control-Field-Compression */
  "FCSALT",	/* 9: FCS-Alternatives */
  "SDP",	/* 10: Self-Describing-Pad */
  "NUMMODE",	/* 11: Numbered-Mode */
  "MULTIPROC",	/* 12: Multi-Link-Procedure */
  "CALLBACK",	/* 13: Callback */
  "CONTIME",	/* 14: Connect-Time */
  "COMPFRAME",	/* 15: Compound-Frames */
  "NDE",	/* 16: Nominal-Data-Encapsulation */
  "MULTIMRRU",	/* 17: Multilink-MRRU */
  "MULTISSNH",	/* 18: Multilink-Short-Sequence-Number-Header */
  "MULTIED",	/* 19: Multilink-Endpoint-Descriminator */
  "PROPRIETRY",	/* 20: Proprietary */
  "DCEID",	/* 21: DCE-Identifier */
  "MULTIPP",	/* 22: Multi-Link-Plus-Procedure */
  "LDBACP",	/* 23: Link Discriminator for BACP */
};

#define NCFTYPES (sizeof(cftypes)/sizeof(char *))

struct fsm LcpFsm = {
  "LCP",			/* Name of protocol */
  PROTO_LCP,			/* Protocol Number */
  LCP_MAXCODE,
  OPEN_ACTIVE,
  ST_INITIAL,			/* State of machine */
  0, 0, 0,
  0,
  {0, 0, 0, NULL, NULL, NULL},
  {0, 0, 0, NULL, NULL, NULL},
  LogLCP,

  LcpLayerUp,
  LcpLayerDown,
  LcpLayerStart,
  LcpLayerFinish,
  LcpInitRestartCounter,
  LcpSendConfigReq,
  LcpSendTerminateReq,
  LcpSendTerminateAck,
  LcpDecodeConfig,
};

static struct pppTimer LcpReportTimer;
static int LcpFailedMagic;

static void
LcpReportTime()
{
  if (LogIsKept(LogDEBUG)) {
    time_t t;

    time(&t);
    LogPrintf(LogDEBUG, "LcpReportTime: %s\n", ctime(&t));
  }
  StopTimer(&LcpReportTimer);
  LcpReportTimer.state = TIMER_STOPPED;
  StartTimer(&LcpReportTimer);
  HdlcErrorCheck();
}

int
ReportLcpStatus()
{
  struct lcpstate *lcp = &LcpInfo;
  struct fsm *fp = &LcpFsm;

  if (!VarTerm)
    return 1;

  fprintf(VarTerm, "%s [%s]\n", fp->name, StateNames[fp->state]);
  fprintf(VarTerm,
	  " his side: MRU %ld, ACCMAP %08lx, PROTOCOMP %d, ACFCOMP %d, MAGIC %08lx,\n"
	  "           REJECT %04lx\n",
	lcp->his_mru, lcp->his_accmap, lcp->his_protocomp, lcp->his_acfcomp,
	  lcp->his_magic, lcp->his_reject);
  fprintf(VarTerm,
	  " my  side: MRU %ld, ACCMAP %08lx, PROTOCOMP %d, ACFCOMP %d, MAGIC %08lx,\n"
	  "           REJECT %04lx\n",
    lcp->want_mru, lcp->want_accmap, lcp->want_protocomp, lcp->want_acfcomp,
	  lcp->want_magic, lcp->my_reject);
  fprintf(VarTerm, "\nDefaults:   MRU = %ld, ACCMAP = %08x\t", VarMRU, VarAccmap);
  fprintf(VarTerm, "Open Mode: %s\n", (VarOpenMode == OPEN_ACTIVE) ? "active" : "passive");
  return 0;
}

/*
 * Generate random number which will be used as magic number.
 */
static u_long
GenerateMagic()
{
  randinit();
  return (random());
}

void
LcpInit()
{
  struct lcpstate *lcp = &LcpInfo;

  FsmInit(&LcpFsm);
  HdlcInit();

  memset(lcp, '\0', sizeof(struct lcpstate));
  lcp->want_mru = VarMRU;
  lcp->his_mru = DEF_MRU;
  lcp->his_accmap = 0xffffffff;
  lcp->want_accmap = VarAccmap;
  lcp->want_magic = GenerateMagic();
  lcp->want_auth = lcp->his_auth = 0;
  if (Enabled(ConfChap))
    lcp->want_auth = PROTO_CHAP;
  else if (Enabled(ConfPap))
    lcp->want_auth = PROTO_PAP;
  if (Enabled(ConfLqr))
    lcp->want_lqrperiod = VarLqrTimeout * 100;
  if (Enabled(ConfAcfcomp))
    lcp->want_acfcomp = 1;
  if (Enabled(ConfProtocomp))
    lcp->want_protocomp = 1;
  LcpFsm.maxconfig = 10;
}

static void
LcpInitRestartCounter(struct fsm * fp)
{
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

void
PutConfValue(int level, u_char ** cpp, char **types, u_char type, int len,
             u_long val)
{
  u_char *cp;
  struct in_addr ina;

  cp = *cpp;
  *cp++ = type;
  *cp++ = len;
  if (len == 6) {
    if (type == TY_IPADDR) {
      ina.s_addr = htonl(val);
      LogPrintf(level, " %s [%d] %s\n",
		types[type], len, inet_ntoa(ina));
    } else
      LogPrintf(level, " %s [%d] %08x\n", types[type], len, val);
    *cp++ = (val >> 24) & 0377;
    *cp++ = (val >> 16) & 0377;
  } else
    LogPrintf(level, " %s [%d] %d\n", types[type], len, val);
  *cp++ = (val >> 8) & 0377;
  *cp++ = val & 0377;
  *cpp = cp;
}

static void
LcpSendConfigReq(struct fsm * fp)
{
  u_char *cp;
  struct lcpstate *lcp = &LcpInfo;
  struct lqrreq *req;

  LogPrintf(LogLCP, "LcpSendConfigReq\n");
  cp = ReqBuff;
  if (!DEV_IS_SYNC) {
    if (lcp->want_acfcomp && !REJECTED(lcp, TY_ACFCOMP)) {
      *cp++ = TY_ACFCOMP;
      *cp++ = 2;
      LogPrintf(LogLCP, " %s\n", cftypes[TY_ACFCOMP]);
    }
    if (lcp->want_protocomp && !REJECTED(lcp, TY_PROTOCOMP)) {
      *cp++ = TY_PROTOCOMP;
      *cp++ = 2;
      LogPrintf(LogLCP, " %s\n", cftypes[TY_PROTOCOMP]);
    }
    if (!REJECTED(lcp, TY_ACCMAP))
      PutConfValue(LogLCP, &cp, cftypes, TY_ACCMAP, 6, lcp->want_accmap);
  }
  if (!REJECTED(lcp, TY_MRU))
    PutConfValue(LogLCP, &cp, cftypes, TY_MRU, 4, lcp->want_mru);
  if (lcp->want_magic && !REJECTED(lcp, TY_MAGICNUM))
    PutConfValue(LogLCP, &cp, cftypes, TY_MAGICNUM, 6, lcp->want_magic);
  if (lcp->want_lqrperiod && !REJECTED(lcp, TY_QUALPROTO)) {
    req = (struct lqrreq *) cp;
    req->type = TY_QUALPROTO;
    req->length = sizeof(struct lqrreq);
    req->proto = htons(PROTO_LQR);
    req->period = htonl(lcp->want_lqrperiod);
    cp += sizeof(struct lqrreq);
    LogPrintf(LogLCP, " %s (%d)\n", cftypes[TY_QUALPROTO], lcp->want_lqrperiod);
  }
  switch (lcp->want_auth) {
  case PROTO_PAP:
    PutConfValue(LogLCP, &cp, cftypes, TY_AUTHPROTO, 4, lcp->want_auth);
    break;
  case PROTO_CHAP:
    PutConfValue(LogLCP, &cp, cftypes, TY_AUTHPROTO, 5, lcp->want_auth);
#ifdef HAVE_DES
    *cp++ = VarMSChap ? 0x80 : 0x05;	/* Use MSChap vs. RFC 1994 (MD5) */
#else
    *cp++ = 0x05;			/* Use MD5 */
#endif
    break;
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
LcpSendProtoRej(u_char * option, int count)
{
  struct fsm *fp = &LcpFsm;

  LogPrintf(LogLCP, "LcpSendProtoRej\n");
  FsmOutput(fp, CODE_PROTOREJ, fp->reqid, option, count);
}

static void
LcpSendTerminateReq(struct fsm * fp)
{
  /* Most thins are done in fsm layer. Nothing to to. */
}

static void
LcpSendTerminateAck(struct fsm * fp)
{
  LogPrintf(LogLCP, "LcpSendTerminateAck.\n");
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

static void
LcpLayerStart(struct fsm * fp)
{
  LogPrintf(LogLCP, "LcpLayerStart\n");
  NewPhase(PHASE_ESTABLISH);
}

static void
StopAllTimers()
{
  StopTimer(&LcpReportTimer);
  StopTimer(&IpcpReportTimer);
  StopIdleTimer();
  StopTimer(&AuthPapInfo.authtimer);
  StopTimer(&AuthChapInfo.authtimer);
  StopLqrTimer();
}

static void
LcpLayerFinish(struct fsm * fp)
{
  LogPrintf(LogLCP, "LcpLayerFinish\n");
  HangupModem(0);
  StopAllTimers();
  /* We're down at last.  Lets tell background and direct mode to get out */
  NewPhase(PHASE_DEAD);
  LcpInit();
  IpcpInit();
  CcpInit();
  Prompt();
}

static void
LcpLayerUp(struct fsm * fp)
{
  LogPrintf(LogLCP, "LcpLayerUp\n");
  tun_configure(LcpInfo.his_mru, ModemSpeed());
  SetLinkParams(&LcpInfo);

  NewPhase(PHASE_AUTHENTICATE);

  StartLqm();
  StopTimer(&LcpReportTimer);
  LcpReportTimer.state = TIMER_STOPPED;
  LcpReportTimer.load = 60 * SECTICKS;
  LcpReportTimer.func = LcpReportTime;
  StartTimer(&LcpReportTimer);
}

static void
LcpLayerDown(struct fsm * fp)
{
  StopAllTimers();
  OsLinkdown();
  LogPrintf(LogLCP, "LcpLayerDown\n");
  /*
   * OsLinkdown() brings CCP & IPCP down, then waits 'till we go from
   * STOPPING to STOPPED.  At this point, the FSM gives us a LayerFinish
   */
}

void
LcpUp()
{
  FsmUp(&LcpFsm);
  LcpFailedMagic = 0;
}

void
LcpDown()
{				/* Sudden death */
  LcpFailedMagic = 0;
  FsmDown(&LcpFsm);
  /* FsmDown() results in a LcpLayerDown() if we're currently open. */
  LcpLayerFinish(&LcpFsm);
}

void
LcpOpen(int open_mode)
{
  LcpFsm.open_mode = open_mode;
  LcpFailedMagic = 0;
  FsmOpen(&LcpFsm);
}

void
LcpClose()
{
  NewPhase(PHASE_TERMINATE);
  OsInterfaceDown(0);
  FsmClose(&LcpFsm);
  LcpFailedMagic = 0;
}

/*
 *	XXX: Should validate option length
 */
static void
LcpDecodeConfig(u_char * cp, int plen, int mode_type)
{
  char *request;
  int type, length, mru, mtu;
  u_long *lp, magic, accmap;
  u_short *sp, proto;
  struct lqrreq *req;

  ackp = AckBuff;
  nakp = NakBuff;
  rejp = RejBuff;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];
    if (type < NCFTYPES)
      request = cftypes[type];
    else
      request = "???";

    switch (type) {
    case TY_MRU:
      sp = (u_short *) (cp + 2);
      mru = htons(*sp);
      LogPrintf(LogLCP, " %s %d\n", request, mru);

      switch (mode_type) {
      case MODE_REQ:
        mtu = VarPrefMTU;
        if (mtu == 0)
          mtu = MAX_MTU;
	if (mru > mtu) {
	  *sp = htons(mtu);
	  memcpy(nakp, cp, 4);
	  nakp += 4;
	} else if (mru < MIN_MRU) {
	  *sp = htons(MIN_MRU);
	  memcpy(nakp, cp, 4);
	  nakp += 4;
	} else {
	  LcpInfo.his_mru = mru;
	  memcpy(ackp, cp, 4);
	  ackp += 4;
	}
	break;
      case MODE_NAK:
	if (mru >= MIN_MRU || mru <= MAX_MRU)
	  LcpInfo.want_mru = mru;
	break;
      case MODE_REJ:
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_ACCMAP:
      lp = (u_long *) (cp + 2);
      accmap = htonl(*lp);
      LogPrintf(LogLCP, " %s %08x\n", request, accmap);

      switch (mode_type) {
      case MODE_REQ:
	LcpInfo.his_accmap = accmap;
	memcpy(ackp, cp, 6);
	ackp += 6;
	break;
      case MODE_NAK:
	LcpInfo.want_accmap = accmap;
	break;
      case MODE_REJ:
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_AUTHPROTO:
      sp = (u_short *) (cp + 2);
      proto = ntohs(*sp);
      LogPrintf(LogLCP, " %s proto = %04x\n", request, proto);

      switch (mode_type) {
      case MODE_REQ:
	switch (proto) {
	case PROTO_PAP:
	  if (length != 4) {
	    LogPrintf(LogLCP, " %s bad length (%d)\n", request, length);
	    goto reqreject;
	  }
	  if (Acceptable(ConfPap)) {
	    LcpInfo.his_auth = proto;
	    memcpy(ackp, cp, length);
	    ackp += length;
	  } else if (Acceptable(ConfChap)) {
	    *nakp++ = *cp;
	    *nakp++ = 5;
	    *nakp++ = (unsigned char) (PROTO_CHAP >> 8);
	    *nakp++ = (unsigned char) PROTO_CHAP;
	    *nakp++ = 5;
	  } else
	    goto reqreject;
	  break;
	case PROTO_CHAP:
	  if (length < 5) {
	    LogPrintf(LogLCP, " %s bad length (%d)\n", request, length);
	    goto reqreject;
	  }
#ifdef HAVE_DES
          if (Acceptable(ConfChap) && (cp[4] == 5 || cp[4] == 0x80))
#else
          if (Acceptable(ConfChap) && cp[4] == 5)
#endif
	  {
	    LcpInfo.his_auth = proto;
	    memcpy(ackp, cp, length);
	    ackp += length;
#ifdef HAVE_DES
            VarMSChap = cp[4] == 0x80;
#endif
	  } else if (Acceptable(ConfPap)) {
	    *nakp++ = *cp;
	    *nakp++ = 4;
	    *nakp++ = (unsigned char) (PROTO_PAP >> 8);
	    *nakp++ = (unsigned char) PROTO_PAP;
	  } else
	    goto reqreject;
	  break;
	default:
	  LogPrintf(LogLCP, " %s not implemented, NAK.\n", request);
	  memcpy(nakp, cp, length);
	  nakp += length;
	  break;
	}
	break;
      case MODE_NAK:
	break;
      case MODE_REJ:
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_QUALPROTO:
      req = (struct lqrreq *) cp;
      LogPrintf(LogLCP, " %s proto: %x, interval: %dms\n",
		request, ntohs(req->proto), ntohl(req->period) * 10);
      switch (mode_type) {
      case MODE_REQ:
	if (ntohs(req->proto) != PROTO_LQR || !Acceptable(ConfLqr))
	  goto reqreject;
	else {
	  LcpInfo.his_lqrperiod = ntohl(req->period);
	  if (LcpInfo.his_lqrperiod < 500)
	    LcpInfo.his_lqrperiod = 500;
	  req->period = htonl(LcpInfo.his_lqrperiod);
	  memcpy(ackp, cp, length);
	  ackp += length;
	}
	break;
      case MODE_NAK:
	break;
      case MODE_REJ:
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_MAGICNUM:
      lp = (u_long *) (cp + 2);
      magic = ntohl(*lp);
      LogPrintf(LogLCP, " %s %08x\n", request, magic);

      switch (mode_type) {
      case MODE_REQ:
	if (LcpInfo.want_magic) {
	  /* Validate magic number */
	  if (magic == LcpInfo.want_magic) {
	    LogPrintf(LogLCP, "Magic is same (%08x) - %d times\n",
                      magic, ++LcpFailedMagic);
	    LcpInfo.want_magic = GenerateMagic();
	    memcpy(nakp, cp, 6);
	    nakp += 6;
            ualarm(TICKUNIT * (4 + 4 * LcpFailedMagic), 0);
            sigpause(0);
	  } else {
	    LcpInfo.his_magic = magic;
	    memcpy(ackp, cp, length);
	    ackp += length;
            LcpFailedMagic = 0;
	  }
	} else {
	  LcpInfo.my_reject |= (1 << type);
	  goto reqreject;
	}
	break;
      case MODE_NAK:
	LogPrintf(LogLCP, " %s magic %08x has NAKed\n", request, magic);
	LcpInfo.want_magic = GenerateMagic();
	break;
      case MODE_REJ:
	LogPrintf(LogLCP, " %s magic has REJected\n", request);
	LcpInfo.want_magic = 0;
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_PROTOCOMP:
      LogPrintf(LogLCP, " %s\n", request);

      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(ConfProtocomp)) {
	  LcpInfo.his_protocomp = 1;
	  memcpy(ackp, cp, 2);
	  ackp += 2;
	} else {
#ifdef OLDMST
	  /*
	   * MorningStar before v1.3 needs NAK
	   */
	  memcpy(nakp, cp, 2);
	  nakp += 2;
#else
	  memcpy(rejp, cp, 2);
	  rejp += 2;
	  LcpInfo.my_reject |= (1 << type);
#endif
	}
	break;
      case MODE_NAK:
      case MODE_REJ:
	LcpInfo.want_protocomp = 0;
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_ACFCOMP:
      LogPrintf(LogLCP, " %s\n", request);
      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(ConfAcfcomp)) {
	  LcpInfo.his_acfcomp = 1;
	  memcpy(ackp, cp, 2);
	  ackp += 2;
	} else {
#ifdef OLDMST
	  /*
	   * MorningStar before v1.3 needs NAK
	   */
	  memcpy(nakp, cp, 2);
	  nakp += 2;
#else
	  memcpy(rejp, cp, 2);
	  rejp += 2;
	  LcpInfo.my_reject |= (1 << type);
#endif
	}
	break;
      case MODE_NAK:
      case MODE_REJ:
	LcpInfo.want_acfcomp = 0;
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_SDP:
      LogPrintf(LogLCP, " %s\n", request);
      switch (mode_type) {
      case MODE_REQ:
      case MODE_NAK:
      case MODE_REJ:
	break;
      }
      break;
    default:
      LogPrintf(LogLCP, " %s[02x]\n", request, type);
      if (mode_type == MODE_REQ) {
    reqreject:
	memcpy(rejp, cp, length);
	rejp += length;
	LcpInfo.my_reject |= (1 << type);
      }
      break;
    }
    /* to avoid inf. loop */
    if (length == 0) {
      LogPrintf(LogLCP, "LCP size zero\n");
      break;
    }
    plen -= length;
    cp += length;
  }
}

void
LcpInput(struct mbuf * bp)
{
  FsmInput(&LcpFsm, bp);
}

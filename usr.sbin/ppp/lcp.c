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
 * $Id: lcp.c,v 1.15 1997/05/10 01:22:13 brian Exp $
 *
 * TODO:
 *      o Validate magic number received from peer.
 *	o Limit data field length by MRU
 */
#include <sys/time.h>
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "lcpproto.h"
#include "os.h"
#include "hdlc.h"
#include "ccp.h"
#include "lqr.h"
#include "phase.h"
#include "vars.h"
#include "auth.h"
#include <arpa/inet.h>

extern void IpcpUp();
extern void IpcpOpen();
extern void StartPapChallenge();
extern void StartChapChallenge();
extern void SetLinkParams(struct lcpstate *);
extern void Prompt();
extern void StopIdleTimer();
extern void OsLinkdown();
extern void Cleanup();
extern struct pppTimer IpcpReportTimer;
extern int randinit;

struct lcpstate LcpInfo;

static void LcpSendConfigReq __P((struct fsm *));
static void LcpSendTerminateReq __P((struct fsm *fp));
static void LcpSendTerminateAck __P((struct fsm *fp));
static void LcpDecodeConfig __P((u_char *cp, int flen,int mode));
static void LcpInitRestartCounter __P((struct fsm *));
static void LcpLayerUp __P((struct fsm *));
static void LcpLayerDown __P((struct fsm *));
static void LcpLayerStart __P((struct fsm *));
static void LcpLayerFinish __P((struct fsm *));

extern int ModemSpeed();

#define	REJECTED(p, x)	(p->his_reject & (1<<x))

static char *cftypes[] = {
  "???", "MRU", "ACCMAP", "AUTHPROTO", "QUALPROTO", "MAGICNUM",
  "RESERVED", "PROTOCOMP", "ACFCOMP", "FCSALT", "SDP",
};

struct fsm LcpFsm = {
  "LCP",			/* Name of protocol */
  PROTO_LCP,			/* Protocol Number */
  LCP_MAXCODE,
  OPEN_ACTIVE,
  ST_INITIAL,			/* State of machine */
  0, 0, 0,

  0,
  { 0, 0, 0, NULL, NULL, NULL },

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

char *PhaseNames[] = {
  "Dead", "Establish", "Authenticate", "Network", "Terminate"
};

void
NewPhase(new)
int new;
{
  struct lcpstate *lcp = &LcpInfo;

  phase = new;
  LogPrintf(LOG_PHASE_BIT, "Phase: %s\n", PhaseNames[phase]);
  switch (phase) {
  case PHASE_AUTHENTICATE:
    lcp->auth_ineed = lcp->want_auth;
    lcp->auth_iwait = lcp->his_auth;
    if (lcp->his_auth || lcp->want_auth) {
      LogPrintf(LOG_PHASE_BIT, " his = %x, mine = %x\n", lcp->his_auth, lcp->want_auth);
      if (lcp->his_auth == PROTO_PAP)
	StartAuthChallenge(&AuthPapInfo);
      if (lcp->want_auth == PROTO_CHAP)
	StartAuthChallenge(&AuthChapInfo);
    } else
      NewPhase(PHASE_NETWORK);
    break;
  case PHASE_NETWORK:
    IpcpUp();
    IpcpOpen();
    CcpUp();
    CcpOpen();
    if (mode & MODE_BACKGROUND && BGFiledes[1] != -1) {
        char c = EX_NORMAL;
        if (write(BGFiledes[1],&c,1) == 1)
          LogPrintf(LOG_PHASE_BIT,"Parent notified of success.\n");
        else
          LogPrintf(LOG_PHASE_BIT,"Failed to notify parent of success.\n");
        close(BGFiledes[1]);
        BGFiledes[1] = -1;
    }
    break;
  case PHASE_DEAD:
    if (mode & (MODE_DIRECT|MODE_BACKGROUND))
      Cleanup(EX_DEAD);
    break;
  }
}

static void
LcpReportTime()
{
#ifdef VERBOSE
  time_t t;

  time(&t);
  logprintf("%s", ctime(&t));
#endif
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

  printf("%s [%s]\n", fp->name, StateNames[fp->state]);
  printf(
    " his side: MRU %ld, ACCMAP %08lx, PROTOCOMP %d, ACFCOMP %d MAGIC %08lx\n",
    lcp->his_mru, lcp->his_accmap, lcp->his_protocomp, lcp->his_acfcomp, lcp->his_magic);
  printf(
    " my  side: MRU %ld, ACCMAP %08lx, PROTOCOMP %d, ACFCOMP %d MAGIC %08lx\n",
    lcp->want_mru, lcp->want_accmap, lcp->want_protocomp, lcp->want_acfcomp, lcp->want_magic);
  printf("\nDefaults:   MRU = %ld, ACCMAP = %08x\t", VarMRU, VarAccmap);
  printf("Open Mode: %s\n", (VarOpenMode == OPEN_ACTIVE)? "active" : "passive");
  return(1);
}

/*
 * Generate random number which will be used as magic number.
 */
u_long
GenerateMagic()
{
  if (!randinit) {
    randinit = 1;
    if (srandomdev() < 0)
      srandom((unsigned long)(time(NULL) ^ getpid()));
  }

  return (random());
}

void
LcpInit()
{
  struct lcpstate *lcp = &LcpInfo;

  FsmInit(&LcpFsm);
  HdlcInit();

  bzero(lcp, sizeof(struct lcpstate));
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
  if (Enabled(ConfLqr)) lcp->want_lqrperiod = VarLqrTimeout * 100;
  if (Enabled(ConfAcfcomp)) lcp->want_acfcomp = 1;
  if (Enabled(ConfProtocomp)) lcp->want_protocomp = 1;
  LcpFsm.maxconfig = 10;
}

static void
LcpInitRestartCounter(fp)
struct fsm *fp;
{
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

void
PutConfValue(cpp, types, type, len, val)
u_char **cpp;
char **types;
u_char type;
int len;
u_long val;
{
  u_char *cp;
  struct in_addr ina;

  cp = *cpp;
  *cp++ = type; *cp++ = len;
  if (len == 6) {
    if (type == TY_IPADDR) {
      ina.s_addr = htonl(val);
      LogPrintf(LOG_LCP_BIT, " %s [%d] %s\n",
	types[type], len, inet_ntoa(ina));
    } else {
      LogPrintf(LOG_LCP_BIT, " %s [%d] %08x\n", types[type], len, val);
    }
    *cp++ = (val >> 24) & 0377;
    *cp++ = (val >> 16) & 0377;
  } else
    LogPrintf(LOG_LCP_BIT, " %s [%d] %d\n", types[type], len, val);
  *cp++ = (val >> 8) & 0377;
  *cp++ = val & 0377;
  *cpp = cp;
}

static void
LcpSendConfigReq(fp)
struct fsm *fp;
{
  u_char *cp;
  struct lcpstate *lcp = &LcpInfo;
  struct lqrreq *req;

  LogPrintf(LOG_LCP_BIT, "%s: SendConfigReq\n", fp->name);
  cp = ReqBuff;
  if (!DEV_IS_SYNC) {
    if (lcp->want_acfcomp && !REJECTED(lcp, TY_ACFCOMP)) {
      *cp++ = TY_ACFCOMP; *cp++ = 2;
      LogPrintf(LOG_LCP_BIT, " %s\n", cftypes[TY_ACFCOMP]);
    }
    if (lcp->want_protocomp && !REJECTED(lcp, TY_PROTOCOMP)) {
      *cp++ = TY_PROTOCOMP; *cp++ = 2;
      LogPrintf(LOG_LCP_BIT, " %s\n", cftypes[TY_PROTOCOMP]);
    }
    if (!REJECTED(lcp, TY_ACCMAP))
      PutConfValue(&cp, cftypes, TY_ACCMAP, 6, lcp->want_accmap);
  }
  if (!REJECTED(lcp, TY_MRU))
    PutConfValue(&cp, cftypes, TY_MRU, 4, lcp->want_mru);
  if (lcp->want_magic && !REJECTED(lcp, TY_MAGICNUM))
    PutConfValue(&cp, cftypes, TY_MAGICNUM, 6, lcp->want_magic);
  if (lcp->want_lqrperiod && !REJECTED(lcp, TY_QUALPROTO)) {
    req = (struct lqrreq *)cp;
    req->type = TY_QUALPROTO; req->length = sizeof(struct lqrreq);
    req->proto = htons(PROTO_LQR);
    req->period = htonl(lcp->want_lqrperiod);
    cp += sizeof(struct lqrreq);
    LogPrintf(LOG_LCP_BIT, " %s (%d)\n", cftypes[TY_QUALPROTO], lcp->want_lqrperiod);
  }
  switch (lcp->want_auth) {
  case PROTO_PAP:
    PutConfValue(&cp, cftypes, TY_AUTHPROTO, 4, lcp->want_auth);
    break;
  case PROTO_CHAP:
    PutConfValue(&cp, cftypes, TY_AUTHPROTO, 5, lcp->want_auth);
    *cp++ = 5;		/* Use MD5 */
    break;
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

void
LcpSendProtoRej(option, count)
u_char *option;
int count;
{
  struct fsm *fp = &LcpFsm;

  LogPrintf(LOG_LCP_BIT, "%s: SendProtoRej\n", fp->name);
  FsmOutput(fp, CODE_PROTOREJ, fp->reqid, option, count);
}

static void
LcpSendTerminateReq(fp)
struct fsm *fp;
{
   /* Most thins are done in fsm layer. Nothing to to. */
}

static void
LcpSendTerminateAck(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: SendTerminateAck.\n", fp->name);
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

static void
LcpLayerStart(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: LayerStart\n", fp->name);
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
LcpLayerFinish(fp)
struct fsm *fp;
{
#ifdef VERBOSE
  fprintf(stderr, "%s: LayerFinish\r\n", fp->name);
#endif
  Prompt();
  LogPrintf(LOG_LCP_BIT, "%s: LayerFinish\n", fp->name);
#ifdef notdef
  OsCloseLink(0);
#else
  OsCloseLink(1);
#endif
  NewPhase(PHASE_DEAD);
  StopAllTimers();
  (void)OsInterfaceDown(0);
}

static void
LcpLayerUp(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: LayerUp\n", fp->name);
  OsSetInterfaceParams(23, LcpInfo.his_mru, ModemSpeed());
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
LcpLayerDown(fp)
struct fsm *fp;
{
  LogPrintf(LOG_LCP_BIT, "%s: LayerDown\n", fp->name);
  StopAllTimers();
  OsLinkdown();
  NewPhase(PHASE_TERMINATE);
  Prompt();
}

void
LcpUp()
{
  FsmUp(&LcpFsm);
}

void
LcpDown()			/* Sudden death */
{
  NewPhase(PHASE_DEAD);
  StopAllTimers();
  FsmDown(&LcpFsm);
}

void
LcpOpen(mode)
int mode;
{
  LcpFsm.open_mode = mode;
  FsmOpen(&LcpFsm);
}

void
LcpClose()
{
  FsmClose(&LcpFsm);
}

/*
 *	XXX: Should validate option length
 */
static void
LcpDecodeConfig(cp, plen, mode)
u_char *cp;
int plen;
int mode;
{
  char *request;
  int type, length, mru;
  u_long *lp, magic, accmap;
  u_short *sp, proto;
  struct lqrreq *req;

  ackp = AckBuff;
  nakp = NakBuff;
  rejp = RejBuff;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];
    if (type <= TY_ACFCOMP)
      request = cftypes[type];
    else
      request = "???";

    switch (type) {
    case TY_MRU:
      sp = (u_short *)(cp + 2);
      mru = htons(*sp);
      LogPrintf(LOG_LCP_BIT, " %s %d\n", request, mru);

      switch (mode) {
      case MODE_REQ:
	if (mru > MAX_MRU) {
	  *sp = htons(MAX_MRU);
	  bcopy(cp, nakp, 4); nakp += 4;
	} else if (mru < MIN_MRU) {
	  *sp = htons(MIN_MRU);
	  bcopy(cp, nakp, 4); nakp += 4;
	} else {
	  LcpInfo.his_mru = mru;
	  bcopy(cp, ackp, 4); ackp += 4;
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
      lp = (u_long *)(cp + 2);
      accmap = htonl(*lp);
      LogPrintf(LOG_LCP_BIT, " %s %08x\n", request, accmap);

      switch (mode) {
      case MODE_REQ:
	LcpInfo.his_accmap = accmap;
	bcopy(cp, ackp, 6); ackp += 6;
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
      sp = (u_short *)(cp + 2);
      proto = ntohs(*sp);
      LogPrintf(LOG_LCP_BIT, " %s proto = %04x\n", request, proto);

      switch (mode) {
      case MODE_REQ:
	switch (proto) {
	case PROTO_PAP:
	  if (length != 4) {
	    LogPrintf(LOG_LCP_BIT, " %s bad length (%d)\n", request, length);
	    goto reqreject;
	  }
	  if (Acceptable(ConfPap)) {
	    LcpInfo.his_auth = proto;
	    bcopy(cp, ackp, length); ackp += length;
	  } else if (Acceptable(ConfChap)) {
	    *nakp++ = *cp; *nakp++ = 5;
	    *nakp++ = (unsigned char)(PROTO_CHAP >> 8);
	    *nakp++ = (unsigned char)PROTO_CHAP;
	    *nakp++ = 5;
	  } else
	    goto reqreject;
	  break;
	case PROTO_CHAP:
	  if (length < 5) {
	    LogPrintf(LOG_LCP_BIT, " %s bad length (%d)\n", request, length);
	    goto reqreject;
	  }
	  if (Acceptable(ConfChap) && cp[4] == 5) {
	    LcpInfo.his_auth = proto;
	    bcopy(cp, ackp, length); ackp += length;
	  } else if (Acceptable(ConfPap)) {
	    *nakp++ = *cp; *nakp++ = 4;
	    *nakp++ = (unsigned char)(PROTO_PAP >> 8);
	    *nakp++ = (unsigned char)PROTO_PAP;
	  } else
	    goto reqreject;
	  break;
	default:
 	    LogPrintf(LOG_LCP_BIT, " %s not implemented, NAK.\n", request);
            bcopy(cp, nakp, length);
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
      req = (struct lqrreq *)cp;
      LogPrintf(LOG_LCP_BIT, " %s proto: %x, interval: %dms\n",
		request, ntohs(req->proto), ntohl(req->period)*10);
      switch (mode) {
      case MODE_REQ:
	if (ntohs(req->proto) != PROTO_LQR || !Acceptable(ConfLqr))
	  goto reqreject;
	else {
	  LcpInfo.his_lqrperiod = ntohl(req->period);
	  if (LcpInfo.his_lqrperiod < 500)
	    LcpInfo.his_lqrperiod = 500;
	  req->period = htonl(LcpInfo.his_lqrperiod);
	  bcopy(cp, ackp, length); ackp += length;
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
      lp = (u_long *)(cp + 2);
      magic = ntohl(*lp);
      LogPrintf(LOG_LCP_BIT, " %s %08x\n", request, magic);

      switch (mode) {
      case MODE_REQ:
	if (LcpInfo.want_magic) {
	  /* XXX: Shoud validate magic number */
	  if (magic == LcpInfo.want_magic)
	    logprintf("magic is same!! %x, %x, %x\n",
		magic, LcpInfo.want_magic, LcpInfo.his_magic);
	  LcpInfo.his_magic = magic;
	  bcopy(cp, ackp, length); ackp += length;
	} else {
	  LcpInfo.my_reject |= (1 << type);
	  goto reqreject;
	}
	break;
      case MODE_NAK:
	LogPrintf(LOG_LCP_BIT, " %s magic %08x has NAKed\n", request, magic);
	LcpInfo.want_magic = GenerateMagic();
	break;
      case MODE_REJ:
	LogPrintf(LOG_LCP_BIT, " %s magic has REJected\n", request);
	LcpInfo.want_magic = 0;
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_PROTOCOMP:
      LogPrintf(LOG_LCP_BIT, " %s\n", request);

      switch (mode) {
      case MODE_REQ:
        if (Acceptable(ConfProtocomp)) {
	  LcpInfo.his_protocomp = 1;
	  bcopy(cp, ackp, 2); ackp += 2;
        } else {
#ifdef OLDMST
	  /*
	   * MorningStar before v1.3 needs NAK
	   */
	  bcopy(cp, nakp, 2); nakp += 2;
#else
	  bcopy(cp, rejp, 2); rejp += 2;
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
      LogPrintf(LOG_LCP_BIT, " %s\n", request);
      switch (mode) {
      case MODE_REQ:
        if (Acceptable(ConfAcfcomp)) {
	  LcpInfo.his_acfcomp = 1;
	  bcopy(cp, ackp, 2);
	  ackp += 2;
        } else {
#ifdef OLDMST
	  /*
	   * MorningStar before v1.3 needs NAK
	   */
	  bcopy(cp, nakp, 2);
	  nakp += 2;
#else
	  bcopy(cp, rejp, 2);
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
      LogPrintf(LOG_LCP_BIT, " %s\n", request);
      switch (mode) {
      case MODE_REQ:
      case MODE_NAK:
      case MODE_REJ:
	break;
      }
      break;
    default:
      LogPrintf(LOG_LCP_BIT, " ???[%02x]\n", type);
      if (mode == MODE_REQ) {
reqreject:
        bcopy(cp, rejp, length);
        rejp += length;
        LcpInfo.my_reject |= (1 << type);
      }
      break;
    }
    /* to avoid inf. loop */
    if (length == 0) {
      LogPrintf(LOG_LCP, "LCP size zero\n");
      break;
    }

    plen -= length;
    cp += length;
  }
}

void
LcpInput(struct mbuf *bp)
{
  FsmInput(&LcpFsm, bp);
}

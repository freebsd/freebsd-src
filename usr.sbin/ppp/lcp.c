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
 * $Id: lcp.c,v 1.50 1997/12/07 23:55:27 brian Exp $
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
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <net/if_tun.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "command.h"
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
#include "vars.h"
#include "auth.h"
#include "pap.h"
#include "chap.h"
#include "async.h"
#include "main.h"
#include "ip.h"
#include "modem.h"
#include "tun.h"

/* for received LQRs */
struct lqrreq {
  u_char type;
  u_char length;
  u_short proto;		/* Quality protocol */
  u_long period;		/* Reporting interval */
};

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

static const char *cftypes[] = {
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

#define NCFTYPES (sizeof cftypes/sizeof cftypes[0])

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
LcpReportTime(void *data)
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
ReportLcpStatus(struct cmdargs const *arg)
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
GenerateMagic(void)
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

int
LcpPutConf(int log, u_char *tgt, const struct lcp_opt *o, const char *nm,
           const char *arg, ...)
{
  va_list ap;
  char buf[30];

  va_start(ap, arg);
  memcpy(tgt, o, o->len);
  if (arg == NULL || *arg == '\0')
    LogPrintf(log, " %s[%d]\n", nm, o->len);
  else {
    vsnprintf(buf, sizeof buf, arg, ap);
    LogPrintf(log, " %s[%d] %s\n", nm, o->len, buf);
  }
  va_end(ap);

  return o->len;
}

#define PUTN(ty)						\
do {								\
  o.id = ty;							\
  o.len = 2;							\
  cp += LcpPutConf(LogLCP, cp, &o, cftypes[o.id], NULL);	\
} while (0)

#define PUTHEXL(ty, arg)					\
do {								\
  o.id = ty;							\
  o.len = 6;							\
  *(u_long *)o.data = htonl(arg);				\
  cp += LcpPutConf(LogLCP, cp, &o, cftypes[o.id], "0x%08x", (u_int)arg);\
} while (0)

#define PUTACCMAP(arg) PUTHEXL(TY_ACCMAP, arg)
#define PUTMAGIC(arg) PUTHEXL(TY_MAGICNUM, arg)

#define PUTMRU(arg)						\
do {								\
  o.id = TY_MRU;						\
  o.len = 4;							\
  *(u_short *)o.data = htons(arg);				\
  cp += LcpPutConf(LogLCP, cp, &o, cftypes[o.id], "%lu", arg);	\
} while (0)

#define PUTLQR(period)						\
do {								\
  o.id = TY_QUALPROTO;						\
  o.len = 8;							\
  *(u_short *)o.data = htons(PROTO_LQR);			\
  *(u_long *)(o.data+2) = htonl(period);			\
  cp += LcpPutConf(LogLCP, cp, &o, cftypes[o.id], "period %ld", period);\
} while (0)

#define PUTPAP()						\
do {								\
  o.id = TY_AUTHPROTO;						\
  o.len = 4;							\
  *(u_short *)o.data = htons(PROTO_PAP);			\
  cp += LcpPutConf(LogLCP, cp, &o, cftypes[o.id],		\
		   "0x%04x (PAP)", PROTO_PAP);			\
} while (0)
  
#define PUTCHAP(val)						\
do {								\
  o.id = TY_AUTHPROTO;						\
  o.len = 5;							\
  *(u_short *)o.data = htons(PROTO_CHAP);			\
  o.data[2] = val;						\
  cp += LcpPutConf(LogLCP, cp, &o, cftypes[o.id],		\
		   "0x%04x (CHAP 0x%02x)", PROTO_CHAP, val);	\
} while (0)

#define PUTMD5CHAP() PUTCHAP(0x05)
#define PUTMSCHAP()  PUTCHAP(0x80)
  
static void
LcpSendConfigReq(struct fsm * fp)
{
  u_char *cp;
  struct lcpstate *lcp = &LcpInfo;
  struct lcp_opt o;

  LogPrintf(LogLCP, "LcpSendConfigReq\n");
  cp = ReqBuff;
  if (!DEV_IS_SYNC) {
    if (lcp->want_acfcomp && !REJECTED(lcp, TY_ACFCOMP))
      PUTN(TY_ACFCOMP);

    if (lcp->want_protocomp && !REJECTED(lcp, TY_PROTOCOMP))
      PUTN(TY_PROTOCOMP);

    if (!REJECTED(lcp, TY_ACCMAP))
      PUTACCMAP(lcp->want_accmap);
  }

  if (!REJECTED(lcp, TY_MRU))
    PUTMRU(lcp->want_mru);

  if (lcp->want_magic && !REJECTED(lcp, TY_MAGICNUM))
    PUTMAGIC(lcp->want_magic);

  if (lcp->want_lqrperiod && !REJECTED(lcp, TY_QUALPROTO))
    PUTLQR(lcp->want_lqrperiod);

  switch (lcp->want_auth) {
  case PROTO_PAP:
    PUTPAP();
    break;

  case PROTO_CHAP:
#ifdef HAVE_DES
    if (VarMSChap)
      PUTMSCHAP();			/* Use MSChap */
    else
#endif
      PUTMD5CHAP();			/* Use MD5 */
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
StopAllTimers(void)
{
  StopTimer(&LcpReportTimer);
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
LcpDecodeConfig(u_char *cp, int plen, int mode_type)
{
  int type, length, mru, mtu, sz, pos;
  u_long *lp, magic, accmap;
  u_short *sp, proto;
  struct lqrreq *req;
  char request[20], desc[22];

  ackp = AckBuff;
  nakp = NakBuff;
  rejp = RejBuff;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];

    if (type < 0 || type >= NCFTYPES)
      snprintf(request, sizeof request, " <%d>[%d]", type, length);
    else
      snprintf(request, sizeof request, " %s[%d]", cftypes[type], length);

    switch (type) {
    case TY_MRU:
      sp = (u_short *) (cp + 2);
      mru = htons(*sp);
      LogPrintf(LogLCP, "%s %d\n", request, mru);

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
      LogPrintf(LogLCP, "%s 0x%08x\n", request, accmap);

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
      switch (proto) {
      case PROTO_PAP:
        LogPrintf(LogLCP, "%s 0x%04x (PAP)\n", request, proto);
        break;
      case PROTO_CHAP:
        LogPrintf(LogLCP, "%s 0x%04x (CHAP 0x%02x)\n", request, proto, cp[4]);
        break;
      default:
        LogPrintf(LogLCP, "%s 0x%04x\n", request, proto);
        break;
      }

      switch (mode_type) {
      case MODE_REQ:
	switch (proto) {
	case PROTO_PAP:
	  if (length != 4) {
	    LogPrintf(LogLCP, " Bad length!\n");
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
#ifdef HAVE_DES
            if (VarMSChap)
              *nakp++ = 0x80;
            else
#endif
	      *nakp++ = 5;
	  } else
	    goto reqreject;
	  break;

	case PROTO_CHAP:
	  if (length < 5) {
	    LogPrintf(LogLCP, " Bad length!\n");
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
          LogPrintf(LogLCP, "%s 0x%04x - not recognised, NAK\n",
                    request, proto);
	  memcpy(nakp, cp, length);
	  nakp += length;
	  break;
	}
	break;
      case MODE_NAK:
	switch (proto) {
	case PROTO_PAP:
          if (Enabled(ConfPap))
            LcpInfo.want_auth = PROTO_PAP;
          else {
            LogPrintf(LogLCP, "Peer will only send PAP (not enabled)\n");
	    LcpInfo.his_reject |= (1 << type);
          }
          break;
	case PROTO_CHAP:
          if (Enabled(ConfChap))
            LcpInfo.want_auth = PROTO_CHAP;
          else {
            LogPrintf(LogLCP, "Peer will only send CHAP (not enabled)\n");
	    LcpInfo.his_reject |= (1 << type);
          }
          break;
        default:
          /* We've been NAK'd with something we don't understand :-( */
	  LcpInfo.his_reject |= (1 << type);
          break;
        }
	break;
      case MODE_REJ:
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;

    case TY_QUALPROTO:
      req = (struct lqrreq *) cp;
      LogPrintf(LogLCP, "%s proto %x, interval %dms\n",
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
      LogPrintf(LogLCP, "%s 0x%08x\n", request, magic);

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
	LogPrintf(LogLCP, " Magic 0x%08x is NAKed!\n", magic);
	LcpInfo.want_magic = GenerateMagic();
	break;
      case MODE_REJ:
	LogPrintf(LogLCP, " Magic 0x%80x is REJected!\n", magic);
	LcpInfo.want_magic = 0;
	LcpInfo.his_reject |= (1 << type);
	break;
      }
      break;

    case TY_PROTOCOMP:
      LogPrintf(LogLCP, "%s\n", request);

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
      LogPrintf(LogLCP, "%s\n", request);
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
      LogPrintf(LogLCP, "%s\n", request);
      switch (mode_type) {
      case MODE_REQ:
      case MODE_NAK:
      case MODE_REJ:
	break;
      }
      break;

    default:
      sz = (sizeof desc - 2) / 2;
      if (sz > length - 2)
        sz = length - 2;
      pos = 0;
      desc[0] = sz ? ' ' : '\0';
      for (pos = 0; sz--; pos++)
        sprintf(desc+(pos<<1)+1, "%02x", cp[pos+2]);

      LogPrintf(LogLCP, "%s%s\n", request, desc);

      if (mode_type == MODE_REQ) {
reqreject:
        if (length > sizeof RejBuff - (rejp - RejBuff)) {
          length = sizeof RejBuff - (rejp - RejBuff);
          LogPrintf(LogLCP, "Can't REJ length %d - trunating to %d\n",
		    cp[1], length);
        }
	memcpy(rejp, cp, length);
	rejp += length;
	LcpInfo.my_reject |= (1 << type);
        if (length != cp[1])
          return;
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

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
 * $FreeBSD: src/usr.sbin/ppp/lcp.c,v 1.81.2.3 2000/08/19 09:30:04 brian Exp $
 *
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "ua.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "throughput.h"
#include "proto.h"
#include "descriptor.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "async.h"
#include "link.h"
#include "physical.h"
#include "prompt.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "mp.h"
#include "chat.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"

/* for received LQRs */
struct lqrreq {
  u_char type;
  u_char length;
  u_short proto;		/* Quality protocol */
  u_int32_t period;		/* Reporting interval */
};

static int LcpLayerUp(struct fsm *);
static void LcpLayerDown(struct fsm *);
static void LcpLayerStart(struct fsm *);
static void LcpLayerFinish(struct fsm *);
static void LcpInitRestartCounter(struct fsm *, int);
static void LcpSendConfigReq(struct fsm *);
static void LcpSentTerminateReq(struct fsm *);
static void LcpSendTerminateAck(struct fsm *, u_char);
static void LcpDecodeConfig(struct fsm *, u_char *, int, int,
                            struct fsm_decode *);

static struct fsm_callbacks lcp_Callbacks = {
  LcpLayerUp,
  LcpLayerDown,
  LcpLayerStart,
  LcpLayerFinish,
  LcpInitRestartCounter,
  LcpSendConfigReq,
  LcpSentTerminateReq,
  LcpSendTerminateAck,
  LcpDecodeConfig,
  fsm_NullRecvResetReq,
  fsm_NullRecvResetAck
};

static const char * const lcp_TimerNames[] =
  {"LCP restart", "LCP openmode", "LCP stopped"};

static const char *
protoname(int proto)
{
  static const char * const cftypes[] = {
    /* Check out the latest ``Assigned numbers'' rfc (1700) */
    NULL,
    "MRU",		/* 1: Maximum-Receive-Unit */
    "ACCMAP",		/* 2: Async-Control-Character-Map */
    "AUTHPROTO",	/* 3: Authentication-Protocol */
    "QUALPROTO",	/* 4: Quality-Protocol */
    "MAGICNUM",		/* 5: Magic-Number */
    "RESERVED",		/* 6: RESERVED */
    "PROTOCOMP",	/* 7: Protocol-Field-Compression */
    "ACFCOMP",		/* 8: Address-and-Control-Field-Compression */
    "FCSALT",		/* 9: FCS-Alternatives */
    "SDP",		/* 10: Self-Describing-Pad */
    "NUMMODE",		/* 11: Numbered-Mode */
    "MULTIPROC",	/* 12: Multi-Link-Procedure */
    "CALLBACK",		/* 13: Callback */
    "CONTIME",		/* 14: Connect-Time */
    "COMPFRAME",	/* 15: Compound-Frames */
    "NDE",		/* 16: Nominal-Data-Encapsulation */
    "MRRU",		/* 17: Multilink-MRRU */
    "SHORTSEQ",		/* 18: Multilink-Short-Sequence-Number-Header */
    "ENDDISC",		/* 19: Multilink-Endpoint-Discriminator */
    "PROPRIETRY",	/* 20: Proprietary */
    "DCEID",		/* 21: DCE-Identifier */
    "MULTIPP",		/* 22: Multi-Link-Plus-Procedure */
    "LDBACP",		/* 23: Link Discriminator for BACP */
  };

  if (proto < 0 || proto > sizeof cftypes / sizeof *cftypes ||
      cftypes[proto] == NULL)
    return HexStr(proto, NULL, 0);

  return cftypes[proto];
}

int
lcp_ReportStatus(struct cmdargs const *arg)
{
  struct link *l;
  struct lcp *lcp;

  l = command_ChooseLink(arg);
  lcp = &l->lcp;

  prompt_Printf(arg->prompt, "%s: %s [%s]\n", l->name, lcp->fsm.name,
                State2Nam(lcp->fsm.state));
  prompt_Printf(arg->prompt,
	        " his side: MRU %d, ACCMAP %08lx, PROTOCOMP %s, ACFCOMP %s,\n"
	        "           MAGIC %08lx, MRRU %u, SHORTSEQ %s, REJECT %04x\n",
	        lcp->his_mru, (u_long)lcp->his_accmap,
                lcp->his_protocomp ? "on" : "off",
                lcp->his_acfcomp ? "on" : "off",
                (u_long)lcp->his_magic, lcp->his_mrru,
                lcp->his_shortseq ? "on" : "off", lcp->his_reject);
  prompt_Printf(arg->prompt,
	        " my  side: MRU %d, ACCMAP %08lx, PROTOCOMP %s, ACFCOMP %s,\n"
                "           MAGIC %08lx, MRRU %u, SHORTSEQ %s, REJECT %04x\n",
                lcp->want_mru, (u_long)lcp->want_accmap,
                lcp->want_protocomp ? "on" : "off",
                lcp->want_acfcomp ? "on" : "off",
                (u_long)lcp->want_magic, lcp->want_mrru,
                lcp->want_shortseq ? "on" : "off", lcp->my_reject);

  prompt_Printf(arg->prompt, "\n Defaults: MRU = %d, ", lcp->cfg.mru);
  prompt_Printf(arg->prompt, "ACCMAP = %08lx\n", (u_long)lcp->cfg.accmap);
  prompt_Printf(arg->prompt, "           LQR period = %us, ",
                lcp->cfg.lqrperiod);
  prompt_Printf(arg->prompt, "Open Mode = %s",
                lcp->cfg.openmode == OPEN_PASSIVE ? "passive" : "active");
  if (lcp->cfg.openmode > 0)
    prompt_Printf(arg->prompt, " (delay %ds)", lcp->cfg.openmode);
  prompt_Printf(arg->prompt, "\n           FSM retry = %us, max %u Config"
                " REQ%s, %u Term REQ%s\n", lcp->cfg.fsm.timeout,
                lcp->cfg.fsm.maxreq, lcp->cfg.fsm.maxreq == 1 ? "" : "s",
                lcp->cfg.fsm.maxtrm, lcp->cfg.fsm.maxtrm == 1 ? "" : "s");
  prompt_Printf(arg->prompt, "    Ident: %s\n", lcp->cfg.ident);
  prompt_Printf(arg->prompt, "\n Negotiation:\n");
  prompt_Printf(arg->prompt, "           ACFCOMP =   %s\n",
                command_ShowNegval(lcp->cfg.acfcomp));
  prompt_Printf(arg->prompt, "           CHAP =      %s\n",
                command_ShowNegval(lcp->cfg.chap05));
#ifdef HAVE_DES
  prompt_Printf(arg->prompt, "           MSCHAP =    %s\n",
                command_ShowNegval(lcp->cfg.chap80nt));
  prompt_Printf(arg->prompt, "           LANMan =    %s\n",
                command_ShowNegval(lcp->cfg.chap80lm));
#endif
  prompt_Printf(arg->prompt, "           LQR =       %s\n",
                command_ShowNegval(lcp->cfg.lqr));
  prompt_Printf(arg->prompt, "           PAP =       %s\n",
                command_ShowNegval(lcp->cfg.pap));
  prompt_Printf(arg->prompt, "           PROTOCOMP = %s\n",
                command_ShowNegval(lcp->cfg.protocomp));

  return 0;
}

static u_int32_t
GenerateMagic(void)
{
  /* Generate random number which will be used as magic number */
  randinit();
  return random();
}

void
lcp_SetupCallbacks(struct lcp *lcp)
{
  lcp->fsm.fn = &lcp_Callbacks;
  lcp->fsm.FsmTimer.name = lcp_TimerNames[0];
  lcp->fsm.OpenTimer.name = lcp_TimerNames[1];
  lcp->fsm.StoppedTimer.name = lcp_TimerNames[2];
}

void
lcp_Init(struct lcp *lcp, struct bundle *bundle, struct link *l,
         const struct fsm_parent *parent)
{
  /* Initialise ourselves */
  int mincode = parent ? 1 : LCP_MINMPCODE;

  fsm_Init(&lcp->fsm, "LCP", PROTO_LCP, mincode, LCP_MAXCODE, LogLCP,
           bundle, l, parent, &lcp_Callbacks, lcp_TimerNames);

  lcp->cfg.mru = DEF_MRU;
  lcp->cfg.accmap = 0;
  lcp->cfg.openmode = 1;
  lcp->cfg.lqrperiod = DEF_LQRPERIOD;
  lcp->cfg.fsm.timeout = DEF_FSMRETRY;
  lcp->cfg.fsm.maxreq = DEF_FSMTRIES;
  lcp->cfg.fsm.maxtrm = DEF_FSMTRIES;

  lcp->cfg.acfcomp = NEG_ENABLED|NEG_ACCEPTED;
  lcp->cfg.chap05 = NEG_ACCEPTED;
#ifdef HAVE_DES
  lcp->cfg.chap80nt = NEG_ACCEPTED;
  lcp->cfg.chap80lm = NEG_ACCEPTED;
#endif
  lcp->cfg.lqr = NEG_ACCEPTED;
  lcp->cfg.pap = NEG_ACCEPTED;
  lcp->cfg.protocomp = NEG_ENABLED|NEG_ACCEPTED;
  *lcp->cfg.ident = '\0';

  lcp_Setup(lcp, lcp->cfg.openmode);
}

void
lcp_Setup(struct lcp *lcp, int openmode)
{
  lcp->fsm.open_mode = openmode;

  lcp->his_mru = lcp->fsm.bundle->cfg.mtu;
  if (!lcp->his_mru || lcp->his_mru > DEF_MRU)
    lcp->his_mru = DEF_MRU;
  lcp->his_mrru = 0;
  lcp->his_magic = 0;
  lcp->his_lqrperiod = 0;
  lcp->his_acfcomp = 0;
  lcp->his_auth = 0;
  lcp->his_authtype = 0;
  lcp->his_callback.opmask = 0;
  lcp->his_shortseq = 0;

  lcp->want_mru = lcp->cfg.mru;
  lcp->want_mrru = lcp->fsm.bundle->ncp.mp.cfg.mrru;
  lcp->want_shortseq = IsEnabled(lcp->fsm.bundle->ncp.mp.cfg.shortseq) ? 1 : 0;
  lcp->want_acfcomp = IsEnabled(lcp->cfg.acfcomp) ? 1 : 0;

  if (lcp->fsm.parent) {
    struct physical *p = link2physical(lcp->fsm.link);

    lcp->his_accmap = 0xffffffff;
    lcp->want_accmap = lcp->cfg.accmap;
    lcp->his_protocomp = 0;
    lcp->want_protocomp = IsEnabled(lcp->cfg.protocomp) ? 1 : 0;
    lcp->want_magic = GenerateMagic();

    if (IsEnabled(lcp->cfg.chap05)) {
      lcp->want_auth = PROTO_CHAP;
      lcp->want_authtype = 0x05;
#ifdef HAVE_DES
    } else if (IsEnabled(lcp->cfg.chap80nt) ||
               IsEnabled(lcp->cfg.chap80lm)) {
      lcp->want_auth = PROTO_CHAP;
      lcp->want_authtype = 0x80;
#endif
    } else if (IsEnabled(lcp->cfg.pap)) {
      lcp->want_auth = PROTO_PAP;
      lcp->want_authtype = 0;
    } else {
      lcp->want_auth = 0;
      lcp->want_authtype = 0;
    }

    if (p->type != PHYS_DIRECT)
      memcpy(&lcp->want_callback, &p->dl->cfg.callback, sizeof(struct callback));
    else
      lcp->want_callback.opmask = 0;
    lcp->want_lqrperiod = IsEnabled(lcp->cfg.lqr) ?
                          lcp->cfg.lqrperiod * 100 : 0;
  } else {
    lcp->his_accmap = lcp->want_accmap = 0;
    lcp->his_protocomp = lcp->want_protocomp = 1;
    lcp->want_magic = 0;
    lcp->want_auth = 0;
    lcp->want_authtype = 0;
    lcp->want_callback.opmask = 0;
    lcp->want_lqrperiod = 0;
  }

  lcp->his_reject = lcp->my_reject = 0;
  lcp->auth_iwait = lcp->auth_ineed = 0;
  lcp->LcpFailedMagic = 0;
}

static void
LcpInitRestartCounter(struct fsm *fp, int what)
{
  /* Set fsm timer load */
  struct lcp *lcp = fsm2lcp(fp);

  fp->FsmTimer.load = lcp->cfg.fsm.timeout * SECTICKS;
  switch (what) {
    case FSM_REQ_TIMER:
      fp->restart = lcp->cfg.fsm.maxreq;
      break;
    case FSM_TRM_TIMER:
      fp->restart = lcp->cfg.fsm.maxtrm;
      break;
    default:
      fp->restart = 1;
      break;
  }
}

static void
LcpSendConfigReq(struct fsm *fp)
{
  /* Send config REQ please */
  struct physical *p = link2physical(fp->link);
  struct lcp *lcp = fsm2lcp(fp);
  u_char buff[200];
  struct lcp_opt *o;
  struct mp *mp;
  u_int16_t proto;

  if (!p) {
    log_Printf(LogERROR, "%s: LcpSendConfigReq: Not a physical link !\n",
              fp->link->name);
    return;
  }

  o = (struct lcp_opt *)buff;
  if (!physical_IsSync(p)) {
    if (lcp->want_acfcomp && !REJECTED(lcp, TY_ACFCOMP))
      INC_LCP_OPT(TY_ACFCOMP, 2, o);

    if (lcp->want_protocomp && !REJECTED(lcp, TY_PROTOCOMP))
      INC_LCP_OPT(TY_PROTOCOMP, 2, o);

    if (!REJECTED(lcp, TY_ACCMAP)) {
      ua_htonl(&lcp->want_accmap, o->data);
      INC_LCP_OPT(TY_ACCMAP, 6, o);
    }
  }

  if (!REJECTED(lcp, TY_MRU)) {
    ua_htons(&lcp->want_mru, o->data);
    INC_LCP_OPT(TY_MRU, 4, o);
  }

  if (lcp->want_magic && !REJECTED(lcp, TY_MAGICNUM)) {
    ua_htonl(&lcp->want_magic, o->data);
    INC_LCP_OPT(TY_MAGICNUM, 6, o);
  }

  if (lcp->want_lqrperiod && !REJECTED(lcp, TY_QUALPROTO)) {
    proto = PROTO_LQR;
    ua_htons(&proto, o->data);
    ua_htonl(&lcp->want_lqrperiod, o->data + 2);
    INC_LCP_OPT(TY_QUALPROTO, 8, o);
  }

  switch (lcp->want_auth) {
  case PROTO_PAP:
    proto = PROTO_PAP;
    ua_htons(&proto, o->data);
    INC_LCP_OPT(TY_AUTHPROTO, 4, o);
    break;

  case PROTO_CHAP:
    proto = PROTO_CHAP;
    ua_htons(&proto, o->data);
    o->data[2] = lcp->want_authtype;
    INC_LCP_OPT(TY_AUTHPROTO, 5, o);
    break;
  }

  if (!REJECTED(lcp, TY_CALLBACK)) {
    if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_AUTH)) {
      *o->data = CALLBACK_AUTH;
      INC_LCP_OPT(TY_CALLBACK, 3, o);
    } else if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_CBCP)) {
      *o->data = CALLBACK_CBCP;
      INC_LCP_OPT(TY_CALLBACK, 3, o);
    } else if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_E164)) {
      int sz = strlen(lcp->want_callback.msg);

      if (sz > sizeof o->data - 1) {
        sz = sizeof o->data - 1;
        log_Printf(LogWARN, "Truncating E164 data to %d octets (oops!)\n", sz);
      }
      *o->data = CALLBACK_E164;
      memcpy(o->data + 1, lcp->want_callback.msg, sz);
      INC_LCP_OPT(TY_CALLBACK, sz + 3, o);
    }
  }

  if (lcp->want_mrru && !REJECTED(lcp, TY_MRRU)) {
    ua_htons(&lcp->want_mrru, o->data);
    INC_LCP_OPT(TY_MRRU, 4, o);

    if (lcp->want_shortseq && !REJECTED(lcp, TY_SHORTSEQ))
      INC_LCP_OPT(TY_SHORTSEQ, 2, o);
  }

  mp = &lcp->fsm.bundle->ncp.mp;
  if (mp->cfg.enddisc.class != 0 && IsEnabled(mp->cfg.negenddisc) &&
      !REJECTED(lcp, TY_ENDDISC)) {
    *o->data = mp->cfg.enddisc.class;
    memcpy(o->data+1, mp->cfg.enddisc.address, mp->cfg.enddisc.len);
    INC_LCP_OPT(TY_ENDDISC, mp->cfg.enddisc.len + 3, o);
  }

  fsm_Output(fp, CODE_CONFIGREQ, fp->reqid, buff, (u_char *)o - buff,
             MB_LCPOUT);
}

void
lcp_SendProtoRej(struct lcp *lcp, u_char *option, int count)
{
  /* Don't understand `option' */
  fsm_Output(&lcp->fsm, CODE_PROTOREJ, lcp->fsm.reqid, option, count,
             MB_LCPOUT);
}

int
lcp_SendIdentification(struct lcp *lcp)
{
  static u_char id;		/* Use a private id */
  u_char msg[DEF_MRU - 3];
  const char *argv[2];
  char *exp[2];

  if (*lcp->cfg.ident == '\0')
    return 0;

  argv[0] = lcp->cfg.ident;
  argv[1] = NULL;

  command_Expand(exp, 1, argv, lcp->fsm.bundle, 1, getpid());

  ua_htonl(&lcp->want_magic, msg);
  strncpy(msg + 4, exp[0], sizeof msg - 5);
  msg[sizeof msg - 1] = '\0';

  log_Printf(LogLCP, "Sending ident magic %08x text %s\n", lcp->want_magic,
             msg + 4);
  fsm_Output(&lcp->fsm, CODE_IDENT, id++, msg, 4 + strlen(msg + 4), MB_LCPOUT);

  free(exp[0]);
  return 1;
}

void
lcp_RecvIdentification(struct lcp *lcp, char *data)
{
  log_Printf(LogLCP, "Received ident: %s\n", data);
}

static void
LcpSentTerminateReq(struct fsm *fp)
{
  /* Term REQ just sent by FSM */
}

static void
LcpSendTerminateAck(struct fsm *fp, u_char id)
{
  /* Send Term ACK please */
  struct physical *p = link2physical(fp->link);

  if (p && p->dl->state == DATALINK_CBCP)
    cbcp_ReceiveTerminateReq(p);

  fsm_Output(fp, CODE_TERMACK, id, NULL, 0, MB_LCPOUT);
}

static void
LcpLayerStart(struct fsm *fp)
{
  /* We're about to start up ! */
  struct lcp *lcp = fsm2lcp(fp);

  log_Printf(LogLCP, "%s: LayerStart\n", fp->link->name);
  lcp->LcpFailedMagic = 0;
  fp->more.reqs = fp->more.naks = fp->more.rejs = lcp->cfg.fsm.maxreq * 3;
}

static void
LcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  log_Printf(LogLCP, "%s: LayerFinish\n", fp->link->name);
}

static int
LcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct physical *p = link2physical(fp->link);
  struct lcp *lcp = fsm2lcp(fp);

  log_Printf(LogLCP, "%s: LayerUp\n", fp->link->name);
  async_SetLinkParams(&p->async, lcp);
  lqr_Start(lcp);
  hdlc_StartTimer(&p->hdlc);
  fp->more.reqs = fp->more.naks = fp->more.rejs = lcp->cfg.fsm.maxreq * 3;

  lcp_SendIdentification(lcp);

  return 1;
}

static void
LcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  struct physical *p = link2physical(fp->link);

  log_Printf(LogLCP, "%s: LayerDown\n", fp->link->name);
  hdlc_StopTimer(&p->hdlc);
  lqr_StopTimer(p);
  lcp_Setup(fsm2lcp(fp), 0);
}

static int
E164ok(struct callback *cb, char *req, int sz)
{
  char list[sizeof cb->msg], *next;
  int len;

  if (!strcmp(cb->msg, "*"))
    return 1;

  strncpy(list, cb->msg, sizeof list - 1);
  list[sizeof list - 1] = '\0';
  for (next = strtok(list, ","); next; next = strtok(NULL, ",")) {
    len = strlen(next);
    if (sz == len && !memcmp(list, req, sz))
      return 1;
  }
  return 0;
}

static void
LcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type,
                struct fsm_decode *dec)
{
  /* Deal with incoming PROTO_LCP */
  struct lcp *lcp = fsm2lcp(fp);
  int type, length, sz, pos, op, callback_req;
  u_int32_t magic, accmap;
  u_short mtu, mru, proto;
  struct lqrreq *req;
  char request[20], desc[22];
  struct mp *mp;
  struct physical *p = link2physical(fp->link);

  sz = op = callback_req = 0;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];

    snprintf(request, sizeof request, " %s[%d]", protoname(type), length);

    if (length < 2) {
      log_Printf(LogLCP, "%s:%s: Bad LCP length\n", fp->link->name, request);
      break;
    }

    switch (type) {
    case TY_MRRU:
      mp = &lcp->fsm.bundle->ncp.mp;
      ua_ntohs(cp + 2, &mru);
      log_Printf(LogLCP, "%s %u\n", request, mru);

      switch (mode_type) {
      case MODE_REQ:
        if (mp->cfg.mrru) {
          if (REJECTED(lcp, TY_MRRU))
            /* Ignore his previous reject so that we REQ next time */
	    lcp->his_reject &= ~(1 << type);

          if (mru < MIN_MRU) {
            /* Push him up to MIN_MRU */
            lcp->his_mrru = MIN_MRU;
	    memcpy(dec->nakend, cp, 2);
            ua_htons(&lcp->his_mrru, dec->nakend + 2);
	    dec->nakend += 4;
	  } else {
            lcp->his_mrru = mru;
	    memcpy(dec->ackend, cp, 4);
	    dec->ackend += 4;
	  }
	  break;
        } else
	  goto reqreject;
        break;
      case MODE_NAK:
        if (mp->cfg.mrru) {
          if (REJECTED(lcp, TY_MRRU))
            /* Must have changed his mind ! */
	    lcp->his_reject &= ~(1 << type);

          if (mru > MAX_MRU)
            lcp->want_mrru = MAX_MRU;
          else if (mru < MIN_MRU)
            lcp->want_mrru = MIN_MRU;
          else
            lcp->want_mrru = mru;
        }
        /* else we honour our config and don't send the suggested REQ */
        break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
        lcp->want_mrru = 0;		/* Ah well, no multilink :-( */
	break;
      }
      break;

    case TY_MRU:
      ua_ntohs(cp + 2, &mru);
      log_Printf(LogLCP, "%s %d\n", request, mru);

      switch (mode_type) {
      case MODE_REQ:
        mtu = lcp->fsm.bundle->cfg.mtu;
        if (mru < MIN_MRU || (!lcp->want_mrru && mru < mtu)) {
          /* Push him up to MTU or MIN_MRU */
          lcp->his_mru = mru < mtu ? mtu : MIN_MRU;
          memcpy(dec->nakend, cp, 2);
          ua_htons(&lcp->his_mru, dec->nakend + 2);
          dec->nakend += 4;
        } else {
          lcp->his_mru = mtu ? mtu : mru;
          memcpy(dec->ackend, cp, 4);
          dec->ackend += 4;
        }
	break;
      case MODE_NAK:
        if (mru > MAX_MRU)
          lcp->want_mru = MAX_MRU;
        else if (mru < MIN_MRU)
          lcp->want_mru = MIN_MRU;
        else
          lcp->want_mru = mru;
	break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_ACCMAP:
      ua_ntohl(cp + 2, &accmap);
      log_Printf(LogLCP, "%s 0x%08lx\n", request, (u_long)accmap);

      switch (mode_type) {
      case MODE_REQ:
	lcp->his_accmap = accmap;
	memcpy(dec->ackend, cp, 6);
	dec->ackend += 6;
	break;
      case MODE_NAK:
	lcp->want_accmap = accmap;
	break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_AUTHPROTO:
      ua_ntohs(cp + 2, &proto);
      log_Printf(LogLCP, "%s 0x%04x (%s)\n", request, proto,
                 Auth2Nam(proto, length > 4 ? cp[4] : 0));

      switch (mode_type) {
      case MODE_REQ:
	switch (proto) {
	case PROTO_PAP:
	  if (length != 4) {
	    log_Printf(LogLCP, " Bad length!\n");
	    goto reqreject;
	  }
	  if (IsAccepted(lcp->cfg.pap)) {
	    lcp->his_auth = proto;
	    lcp->his_authtype = 0;
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
	  } else if (IsAccepted(lcp->cfg.chap05)) {
	    *dec->nakend++ = *cp;
	    *dec->nakend++ = 5;
	    *dec->nakend++ = (unsigned char) (PROTO_CHAP >> 8);
	    *dec->nakend++ = (unsigned char) PROTO_CHAP;
	    *dec->nakend++ = 0x05;
#ifdef HAVE_DES
	  } else if (IsAccepted(lcp->cfg.chap80nt) ||
	             IsAccepted(lcp->cfg.chap80lm)) {
	    *dec->nakend++ = *cp;
	    *dec->nakend++ = 5;
	    *dec->nakend++ = (unsigned char) (PROTO_CHAP >> 8);
	    *dec->nakend++ = (unsigned char) PROTO_CHAP;
	    *dec->nakend++ = 0x80;
#endif
	  } else
	    goto reqreject;
	  break;

	case PROTO_CHAP:
	  if (length != 5) {
	    log_Printf(LogLCP, " Bad length!\n");
	    goto reqreject;
	  }
          if ((cp[4] == 0x05 && IsAccepted(lcp->cfg.chap05))
#ifdef HAVE_DES
              || (cp[4] == 0x80 && (IsAccepted(lcp->cfg.chap80nt) ||
                                   (IsAccepted(lcp->cfg.chap80lm))))
#endif
             ) {
	    lcp->his_auth = proto;
	    lcp->his_authtype = cp[4];
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
	  } else {
#ifndef HAVE_DES
            if (cp[4] == 0x80)
              log_Printf(LogWARN, "CHAP 0x80 not available without DES\n");
            else
#endif
            if (cp[4] != 0x05)
              log_Printf(LogWARN, "%s not supported\n",
                         Auth2Nam(PROTO_CHAP, cp[4]));

            if (IsAccepted(lcp->cfg.chap05)) {
	      *dec->nakend++ = *cp;
	      *dec->nakend++ = 5;
	      *dec->nakend++ = (unsigned char) (PROTO_CHAP >> 8);
	      *dec->nakend++ = (unsigned char) PROTO_CHAP;
	      *dec->nakend++ = 0x05;
#ifdef HAVE_DES
            } else if (IsAccepted(lcp->cfg.chap80nt) ||
                       IsAccepted(lcp->cfg.chap80lm)) {
	      *dec->nakend++ = *cp;
	      *dec->nakend++ = 5;
	      *dec->nakend++ = (unsigned char) (PROTO_CHAP >> 8);
	      *dec->nakend++ = (unsigned char) PROTO_CHAP;
	      *dec->nakend++ = 0x80;
#endif
            } else if (IsAccepted(lcp->cfg.pap)) {
	      *dec->nakend++ = *cp;
	      *dec->nakend++ = 4;
	      *dec->nakend++ = (unsigned char) (PROTO_PAP >> 8);
	      *dec->nakend++ = (unsigned char) PROTO_PAP;
	    } else
	      goto reqreject;
          }
	  break;

	default:
          log_Printf(LogLCP, "%s 0x%04x - not recognised, NAK\n",
                    request, proto);
	  memcpy(dec->nakend, cp, length);
	  dec->nakend += length;
	  break;
	}
	break;
      case MODE_NAK:
	switch (proto) {
	case PROTO_PAP:
          if (IsEnabled(lcp->cfg.pap)) {
            lcp->want_auth = PROTO_PAP;
            lcp->want_authtype = 0;
          } else {
            log_Printf(LogLCP, "Peer will only send PAP (not enabled)\n");
	    lcp->his_reject |= (1 << type);
          }
          break;
	case PROTO_CHAP:
          if (cp[4] == 0x05 && IsEnabled(lcp->cfg.chap05)) {
            lcp->want_auth = PROTO_CHAP;
            lcp->want_authtype = 0x05;
#ifdef HAVE_DES
          } else if (cp[4] == 0x80 && (IsEnabled(lcp->cfg.chap80nt) ||
                                       IsEnabled(lcp->cfg.chap80lm))) {
            lcp->want_auth = PROTO_CHAP;
            lcp->want_authtype = 0x80;
#endif
          } else {
#ifndef HAVE_DES
            if (cp[4] == 0x80)
              log_Printf(LogLCP, "Peer will only send MSCHAP (not available"
                         " without DES)\n");
            else
#endif
            log_Printf(LogLCP, "Peer will only send %s (not %s)\n",
                       Auth2Nam(PROTO_CHAP, cp[4]),
#ifdef HAVE_DES
                       cp[4] == 0x80 ? "configured" :
#endif
                       "supported");
	    lcp->his_reject |= (1 << type);
          }
          break;
        default:
          /* We've been NAK'd with something we don't understand :-( */
	  lcp->his_reject |= (1 << type);
          break;
        }
	break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_QUALPROTO:
      req = (struct lqrreq *)cp;
      log_Printf(LogLCP, "%s proto %x, interval %lums\n",
                request, ntohs(req->proto), (u_long)ntohl(req->period) * 10);
      switch (mode_type) {
      case MODE_REQ:
	if (ntohs(req->proto) != PROTO_LQR || !IsAccepted(lcp->cfg.lqr))
	  goto reqreject;
	else {
	  lcp->his_lqrperiod = ntohl(req->period);
	  if (lcp->his_lqrperiod < MIN_LQRPERIOD * 100)
	    lcp->his_lqrperiod = MIN_LQRPERIOD * 100;
	  req->period = htonl(lcp->his_lqrperiod);
	  memcpy(dec->ackend, cp, length);
	  dec->ackend += length;
	}
	break;
      case MODE_NAK:
	break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_MAGICNUM:
      ua_ntohl(cp + 2, &magic);
      log_Printf(LogLCP, "%s 0x%08lx\n", request, (u_long)magic);

      switch (mode_type) {
      case MODE_REQ:
	if (lcp->want_magic) {
	  /* Validate magic number */
	  if (magic == lcp->want_magic) {
	    log_Printf(LogLCP, "Magic is same (%08lx) - %d times\n",
                      (u_long)magic, ++lcp->LcpFailedMagic);
	    lcp->want_magic = GenerateMagic();
	    memcpy(dec->nakend, cp, 6);
	    dec->nakend += 6;
            ualarm(TICKUNIT * (4 + 4 * lcp->LcpFailedMagic), 0);
            sigpause(0);
	  } else {
	    lcp->his_magic = magic;
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
            lcp->LcpFailedMagic = 0;
	  }
	} else {
	  goto reqreject;
	}
	break;
      case MODE_NAK:
	log_Printf(LogLCP, " Magic 0x%08lx is NAKed!\n", (u_long)magic);
	lcp->want_magic = GenerateMagic();
	break;
      case MODE_REJ:
	log_Printf(LogLCP, " Magic 0x%08x is REJected!\n", magic);
	lcp->want_magic = 0;
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_PROTOCOMP:
      log_Printf(LogLCP, "%s\n", request);

      switch (mode_type) {
      case MODE_REQ:
	if (IsAccepted(lcp->cfg.protocomp)) {
	  lcp->his_protocomp = 1;
	  memcpy(dec->ackend, cp, 2);
	  dec->ackend += 2;
	} else {
#ifdef OLDMST
	  /* MorningStar before v1.3 needs NAK */
	  memcpy(dec->nakend, cp, 2);
	  dec->nakend += 2;
#else
	  goto reqreject;
#endif
	}
	break;
      case MODE_NAK:
      case MODE_REJ:
	lcp->want_protocomp = 0;
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_ACFCOMP:
      log_Printf(LogLCP, "%s\n", request);
      switch (mode_type) {
      case MODE_REQ:
	if (IsAccepted(lcp->cfg.acfcomp)) {
	  lcp->his_acfcomp = 1;
	  memcpy(dec->ackend, cp, 2);
	  dec->ackend += 2;
	} else {
#ifdef OLDMST
	  /* MorningStar before v1.3 needs NAK */
	  memcpy(dec->nakend, cp, 2);
	  dec->nakend += 2;
#else
	  goto reqreject;
#endif
	}
	break;
      case MODE_NAK:
      case MODE_REJ:
	lcp->want_acfcomp = 0;
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_SDP:
      log_Printf(LogLCP, "%s\n", request);
      switch (mode_type) {
      case MODE_REQ:
      case MODE_NAK:
      case MODE_REJ:
	break;
      }
      break;

    case TY_CALLBACK:
      if (length == 2)
        op = CALLBACK_NONE;
      else
        op = (int)cp[2];
      sz = length - 3;
      switch (op) {
        case CALLBACK_AUTH:
          log_Printf(LogLCP, "%s Auth\n", request);
          break;
        case CALLBACK_DIALSTRING:
          log_Printf(LogLCP, "%s Dialstring %.*s\n", request, sz, cp + 3);
          break;
        case CALLBACK_LOCATION:
          log_Printf(LogLCP, "%s Location %.*s\n", request, sz, cp + 3);
          break;
        case CALLBACK_E164:
          log_Printf(LogLCP, "%s E.164 (%.*s)\n", request, sz, cp + 3);
          break;
        case CALLBACK_NAME:
          log_Printf(LogLCP, "%s Name %.*s\n", request, sz, cp + 3);
          break;
        case CALLBACK_CBCP:
          log_Printf(LogLCP, "%s CBCP\n", request);
          break;
        default:
          log_Printf(LogLCP, "%s ???\n", request);
          break;
      }

      switch (mode_type) {
      case MODE_REQ:
        callback_req = 1;
        if (p->type != PHYS_DIRECT)
	  goto reqreject;
        if ((p->dl->cfg.callback.opmask & CALLBACK_BIT(op)) &&
            (op != CALLBACK_AUTH || p->link.lcp.auth_ineed) &&
            (op != CALLBACK_E164 ||
             E164ok(&p->dl->cfg.callback, cp + 3, sz))) {
	  lcp->his_callback.opmask = CALLBACK_BIT(op);
          if (sz > sizeof lcp->his_callback.msg - 1) {
            sz = sizeof lcp->his_callback.msg - 1;
            log_Printf(LogWARN, "Truncating option arg to %d octets\n", sz);
          }
	  memcpy(lcp->his_callback.msg, cp + 3, sz);
	  lcp->his_callback.msg[sz] = '\0';
	  memcpy(dec->ackend, cp, sz + 3);
	  dec->ackend += sz + 3;
        } else if ((p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_AUTH)) &&
                    p->link.lcp.auth_ineed) {
          *dec->nakend++ = *cp;
          *dec->nakend++ = 3;
          *dec->nakend++ = CALLBACK_AUTH;
        } else if (p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_CBCP)) {
          *dec->nakend++ = *cp;
          *dec->nakend++ = 3;
          *dec->nakend++ = CALLBACK_CBCP;
        } else if (p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_E164)) {
          *dec->nakend++ = *cp;
          *dec->nakend++ = 3;
          *dec->nakend++ = CALLBACK_E164;
        } else if (p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_AUTH)) {
          log_Printf(LogWARN, "Cannot insist on auth callback without"
                     " PAP or CHAP enabled !\n");
          *dec->nakend++ = *cp;
          *dec->nakend++ = 2;
        } else
	  goto reqreject;
        break;
      case MODE_NAK:
        /* We don't do what he NAKs with, we do things in our preferred order */
        if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_AUTH))
          lcp->want_callback.opmask &= ~CALLBACK_BIT(CALLBACK_AUTH);
        else if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_CBCP))
          lcp->want_callback.opmask &= ~CALLBACK_BIT(CALLBACK_CBCP);
        else if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_E164))
          lcp->want_callback.opmask &= ~CALLBACK_BIT(CALLBACK_E164);
        if (lcp->want_callback.opmask == CALLBACK_BIT(CALLBACK_NONE)) {
          log_Printf(LogPHASE, "Peer NAKd all callbacks, trying none\n");
          lcp->want_callback.opmask = 0;
        } else if (!lcp->want_callback.opmask) {
          log_Printf(LogPHASE, "Peer NAKd last configured callback\n");
          fsm_Close(&lcp->fsm);
        }
        break;
      case MODE_REJ:
        if (lcp->want_callback.opmask & CALLBACK_BIT(CALLBACK_NONE)) {
	  lcp->his_reject |= (1 << type);
          lcp->want_callback.opmask = 0;
        } else {
          log_Printf(LogPHASE, "Peer rejected *required* callback\n");
          fsm_Close(&lcp->fsm);
        }
	break;
      }
      break;

    case TY_SHORTSEQ:
      mp = &lcp->fsm.bundle->ncp.mp;
      log_Printf(LogLCP, "%s\n", request);

      switch (mode_type) {
      case MODE_REQ:
        if (lcp->want_mrru && IsAccepted(mp->cfg.shortseq)) {
          lcp->his_shortseq = 1;
	  memcpy(dec->ackend, cp, length);
	  dec->ackend += length;
        } else
	  goto reqreject;
        break;
      case MODE_NAK:
        /*
         * He's trying to get us to ask for short sequence numbers.
         * We ignore the NAK and honour our configuration file instead.
         */
        break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
        lcp->want_shortseq = 0;		/* For when we hit MP */
	break;
      }
      break;

    case TY_ENDDISC:
      mp = &lcp->fsm.bundle->ncp.mp;
      log_Printf(LogLCP, "%s %s\n", request,
                mp_Enddisc(cp[2], cp + 3, length - 3));
      switch (mode_type) {
      case MODE_REQ:
        if (!p) {
          log_Printf(LogLCP, " ENDDISC rejected - not a physical link\n");
	  goto reqreject;
        } else if (!IsAccepted(mp->cfg.negenddisc))
	  goto reqreject;
        else if (length-3 < sizeof p->dl->peer.enddisc.address &&
                   cp[2] <= MAX_ENDDISC_CLASS) {
          p->dl->peer.enddisc.class = cp[2];
          p->dl->peer.enddisc.len = length-3;
          memcpy(p->dl->peer.enddisc.address, cp + 3, length - 3);
          p->dl->peer.enddisc.address[length - 3] = '\0';
          /* XXX: If mp->active, compare and NAK with mp->peer ? */
	  memcpy(dec->ackend, cp, length);
	  dec->ackend += length;
        } else {
          if (cp[2] > MAX_ENDDISC_CLASS)
            log_Printf(LogLCP, " ENDDISC rejected - unrecognised class %d\n",
                      cp[2]);
          else
            log_Printf(LogLCP, " ENDDISC rejected - local max length is %ld\n",
                      (long)(sizeof p->dl->peer.enddisc.address - 1));
	  goto reqreject;
        }
	break;

      case MODE_NAK:	/* Treat this as a REJ, we don't vary our disc (yet) */
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
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

      log_Printf(LogLCP, "%s%s\n", request, desc);

      if (mode_type == MODE_REQ) {
reqreject:
        if (length > sizeof dec->rej - (dec->rejend - dec->rej)) {
          length = sizeof dec->rej - (dec->rejend - dec->rej);
          log_Printf(LogLCP, "Can't REJ length %d - trunating to %d\n",
		    cp[1], length);
        }
	memcpy(dec->rejend, cp, length);
	dec->rejend += length;
	lcp->my_reject |= (1 << type);
        if (length != cp[1])
          length = 0;		/* force our way out of the loop */
      }
      break;
    }
    plen -= length;
    cp += length;
  }

  if (mode_type != MODE_NOP) {
    if (mode_type == MODE_REQ && p && p->type == PHYS_DIRECT &&
        p->dl->cfg.callback.opmask && !callback_req &&
        !(p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_NONE))) {
      /* We *REQUIRE* that the peer requests callback */
      *dec->nakend++ = TY_CALLBACK;
      *dec->nakend++ = 3;
      if ((p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_AUTH)) &&
          p->link.lcp.auth_ineed)
        *dec->nakend++ = CALLBACK_AUTH;
      else if (p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_CBCP))
        *dec->nakend++ = CALLBACK_CBCP;
      else if (p->dl->cfg.callback.opmask & CALLBACK_BIT(CALLBACK_E164))
        *dec->nakend++ = CALLBACK_E164;
      else {
        log_Printf(LogWARN, "Cannot insist on auth callback without"
                   " PAP or CHAP enabled !\n");
        dec->nakend[-1] = 2;	/* XXX: Silly ! */
      }
    }
    if (dec->rejend != dec->rej) {
      /* rejects are preferred */
      dec->ackend = dec->ack;
      dec->nakend = dec->nak;
    } else if (dec->nakend != dec->nak)
      /* then NAKs */
      dec->ackend = dec->ack;
  }
}

extern struct mbuf *
lcp_Input(struct bundle *bundle, struct link *l, struct mbuf *bp)
{
  /* Got PROTO_LCP from link */
  m_settype(bp, MB_LCPIN);
  fsm_Input(&l->lcp.fsm, bp);
  return NULL;
}

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
 * $Id: lcp.c,v 1.57 1998/05/21 21:46:00 brian Exp $
 *
 * TODO:
 *	o Limit data field length by MRU
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "lcp.h"
#include "throughput.h"
#include "lcpproto.h"
#include "descriptor.h"
#include "lqr.h"
#include "hdlc.h"
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
#include "datalink.h"
#include "bundle.h"

/* for received LQRs */
struct lqrreq {
  u_char type;
  u_char length;
  u_short proto;		/* Quality protocol */
  u_long period;		/* Reporting interval */
};

static int LcpLayerUp(struct fsm *);
static void LcpLayerDown(struct fsm *);
static void LcpLayerStart(struct fsm *);
static void LcpLayerFinish(struct fsm *);
static void LcpInitRestartCounter(struct fsm *);
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

static const char *lcp_TimerNames[] =
  {"LCP restart", "LCP openmode", "LCP stopped"};

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
  "MRRU",	/* 17: Multilink-MRRU */
  "SHORTSEQ",	/* 18: Multilink-Short-Sequence-Number-Header */
  "ENDDISC",	/* 19: Multilink-Endpoint-Discriminator */
  "PROPRIETRY",	/* 20: Proprietary */
  "DCEID",	/* 21: DCE-Identifier */
  "MULTIPP",	/* 22: Multi-Link-Plus-Procedure */
  "LDBACP",	/* 23: Link Discriminator for BACP */
};

#define NCFTYPES (sizeof cftypes/sizeof cftypes[0])

int
lcp_ReportStatus(struct cmdargs const *arg)
{
  struct link *l;
  struct lcp *lcp;

  if (!(l = command_ChooseLink(arg)))
    return -1;
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
  prompt_Printf(arg->prompt, "\n           FSM retry = %us\n",
                lcp->cfg.fsmretry);
  prompt_Printf(arg->prompt, "\n Negotiation:\n");
  prompt_Printf(arg->prompt, "           ACFCOMP =   %s\n",
                command_ShowNegval(lcp->cfg.acfcomp));
  prompt_Printf(arg->prompt, "           CHAP =      %s\n",
                command_ShowNegval(lcp->cfg.chap));
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

  fsm_Init(&lcp->fsm, "LCP", PROTO_LCP, mincode, LCP_MAXCODE, 10, LogLCP,
           bundle, l, parent, &lcp_Callbacks, lcp_TimerNames);

  lcp->cfg.mru = DEF_MRU;
  lcp->cfg.accmap = 0;
  lcp->cfg.openmode = 1;
  lcp->cfg.lqrperiod = DEF_LQRPERIOD;
  lcp->cfg.fsmretry = DEF_FSMRETRY;

  lcp->cfg.acfcomp = NEG_ENABLED|NEG_ACCEPTED;
  lcp->cfg.chap = NEG_ACCEPTED;
  lcp->cfg.lqr = NEG_ACCEPTED;
  lcp->cfg.pap = NEG_ACCEPTED;
  lcp->cfg.protocomp = NEG_ENABLED|NEG_ACCEPTED;

  lcp_Setup(lcp, lcp->cfg.openmode);
}

void
lcp_Setup(struct lcp *lcp, int openmode)
{
  lcp->fsm.open_mode = openmode;
  lcp->fsm.maxconfig = 10;

  lcp->his_mru = DEF_MRU;
  lcp->his_mrru = 0;
  lcp->his_magic = 0;
  lcp->his_lqrperiod = 0;
  lcp->his_acfcomp = 0;
  lcp->his_auth = 0;
  lcp->his_shortseq = 0;

  lcp->want_mru = lcp->cfg.mru;
  lcp->want_mrru = lcp->fsm.bundle->ncp.mp.cfg.mrru;
  lcp->want_shortseq = IsEnabled(lcp->fsm.bundle->ncp.mp.cfg.shortseq) ? 1 : 0;
  lcp->want_acfcomp = IsEnabled(lcp->cfg.acfcomp) ? 1 : 0;

  if (lcp->fsm.parent) {
    lcp->his_accmap = 0xffffffff;
    lcp->want_accmap = lcp->cfg.accmap;
    lcp->his_protocomp = 0;
    lcp->want_protocomp = IsEnabled(lcp->cfg.protocomp) ? 1 : 0;
    lcp->want_magic = GenerateMagic();
    lcp->want_auth = IsEnabled(lcp->cfg.chap) ? PROTO_CHAP :
                     IsEnabled(lcp->cfg.pap) ?  PROTO_PAP : 0;
    lcp->want_lqrperiod = IsEnabled(lcp->cfg.lqr) ?
                          lcp->cfg.lqrperiod * 100 : 0;
  } else {
    lcp->his_accmap = lcp->want_accmap = 0;
    lcp->his_protocomp = lcp->want_protocomp = 1;
    lcp->want_magic = 0;
    lcp->want_auth = 0;
    lcp->want_lqrperiod = 0;
  }

  lcp->his_reject = lcp->my_reject = 0;
  lcp->auth_iwait = lcp->auth_ineed = 0;
  lcp->LcpFailedMagic = 0;
}

static void
LcpInitRestartCounter(struct fsm * fp)
{
  /* Set fsm timer load */
  struct lcp *lcp = fsm2lcp(fp);

  fp->FsmTimer.load = lcp->cfg.fsmretry * SECTICKS;
  fp->restart = 5;
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
      *(u_int32_t *)o->data = htonl(lcp->want_accmap);
      INC_LCP_OPT(TY_ACCMAP, 6, o);
    }
  }

  if (!REJECTED(lcp, TY_MRU)) {
    *(u_short *)o->data = htons(lcp->want_mru);
    INC_LCP_OPT(TY_MRU, 4, o);
  }

  if (lcp->want_magic && !REJECTED(lcp, TY_MAGICNUM)) {
    *(u_int32_t *)o->data = htonl(lcp->want_magic);
    INC_LCP_OPT(TY_MAGICNUM, 6, o);
  }

  if (lcp->want_lqrperiod && !REJECTED(lcp, TY_QUALPROTO)) {
    *(u_short *)o->data = htons(PROTO_LQR);
    *(u_long *)(o->data + 2) = htonl(lcp->want_lqrperiod);
    INC_LCP_OPT(TY_QUALPROTO, 8, o);
  }

  switch (lcp->want_auth) {
  case PROTO_PAP:
    *(u_short *)o->data = htons(PROTO_PAP);
    INC_LCP_OPT(TY_AUTHPROTO, 4, o);
    break;

  case PROTO_CHAP:
    *(u_short *)o->data = htons(PROTO_CHAP);
    o->data[2] = 0x05;
    INC_LCP_OPT(TY_AUTHPROTO, 5, o);
    break;
  }

  if (lcp->want_mrru && !REJECTED(lcp, TY_MRRU)) {
    *(u_short *)o->data = htons(lcp->want_mrru);
    INC_LCP_OPT(TY_MRRU, 4, o);

    if (lcp->want_shortseq && !REJECTED(lcp, TY_SHORTSEQ))
      INC_LCP_OPT(TY_SHORTSEQ, 2, o);
  }

  mp = &lcp->fsm.bundle->ncp.mp;
  if (mp->cfg.enddisc.class != 0 && !REJECTED(lcp, TY_ENDDISC)) {
    *o->data = mp->cfg.enddisc.class;
    memcpy(o->data+1, mp->cfg.enddisc.address, mp->cfg.enddisc.len);
    INC_LCP_OPT(TY_ENDDISC, mp->cfg.enddisc.len + 3, o);
  }

  fsm_Output(fp, CODE_CONFIGREQ, fp->reqid, buff, (u_char *)o - buff);
}

void
lcp_SendProtoRej(struct lcp *lcp, u_char *option, int count)
{
  /* Don't understand `option' */
  fsm_Output(&lcp->fsm, CODE_PROTOREJ, lcp->fsm.reqid, option, count);
}

static void
LcpSentTerminateReq(struct fsm * fp)
{
  /* Term REQ just sent by FSM */
}

static void
LcpSendTerminateAck(struct fsm *fp, u_char id)
{
  /* Send Term ACK please */
  fsm_Output(fp, CODE_TERMACK, id, NULL, 0);
}

static void
LcpLayerStart(struct fsm *fp)
{
  /* We're about to start up ! */
  struct lcp *lcp = fsm2lcp(fp);

  log_Printf(LogLCP, "%s: LcpLayerStart\n", fp->link->name);
  lcp->LcpFailedMagic = 0;
}

static void
LcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  log_Printf(LogLCP, "%s: LcpLayerFinish\n", fp->link->name);
}

static int
LcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct physical *p = link2physical(fp->link);
  struct lcp *lcp = fsm2lcp(fp);

  log_Printf(LogLCP, "%s: LcpLayerUp\n", fp->link->name);
  async_SetLinkParams(&p->async, lcp);
  lqr_Start(lcp);
  hdlc_StartTimer(&p->hdlc);
  return 1;
}

static void
LcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  struct physical *p = link2physical(fp->link);

  log_Printf(LogLCP, "%s: LcpLayerDown\n", fp->link->name);
  hdlc_StopTimer(&p->hdlc);
  lqr_StopTimer(p);
}

static void
LcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type,
                struct fsm_decode *dec)
{
  /* Deal with incoming PROTO_LCP */
  struct lcp *lcp = fsm2lcp(fp);
  int type, length, sz, pos;
  u_int32_t magic, accmap;
  u_short mtu, mru, *sp, proto;
  struct lqrreq *req;
  char request[20], desc[22];
  struct mp *mp;
  struct physical *p = link2physical(fp->link);

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];

    if (length == 0) {
      log_Printf(LogLCP, "%s: LCP size zero\n", fp->link->name);
      break;
    }

    if (type < 0 || type >= NCFTYPES)
      snprintf(request, sizeof request, " <%d>[%d]", type, length);
    else
      snprintf(request, sizeof request, " %s[%d]", cftypes[type], length);

    switch (type) {
    case TY_MRRU:
      mp = &lcp->fsm.bundle->ncp.mp;
      sp = (u_short *)(cp + 2);
      mru = htons(*sp);
      log_Printf(LogLCP, "%s %u\n", request, mru);

      switch (mode_type) {
      case MODE_REQ:
        if (mp->cfg.mrru) {
          if (REJECTED(lcp, TY_MRRU))
            /* Ignore his previous reject so that we REQ next time */
	    lcp->his_reject &= ~(1 << type);

          mtu = lcp->fsm.bundle->cfg.mtu;
          if (mru < MIN_MRU || mru < mtu) {
            /* Push him up to MTU or MIN_MRU */
            lcp->his_mrru = mru < mtu ? mtu : MIN_MRU;
	    *sp = htons((u_short)lcp->his_mrru);
	    memcpy(dec->nakend, cp, 4);
	    dec->nakend += 4;
	  } else {
            lcp->his_mrru = mtu ? mtu : mru;
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
      sp = (u_short *) (cp + 2);
      mru = htons(*sp);
      log_Printf(LogLCP, "%s %d\n", request, mru);

      switch (mode_type) {
      case MODE_REQ:
        mtu = lcp->fsm.bundle->cfg.mtu;
        if (mru < MIN_MRU || (!lcp->want_mrru && mru < mtu)) {
          /* Push him up to MTU or MIN_MRU */
          lcp->his_mru = mru < mtu ? mtu : MIN_MRU;
          *sp = htons((u_short)lcp->his_mru);
          memcpy(dec->nakend, cp, 4);
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
      accmap = htonl(*(u_int32_t *)(cp + 2));
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
      sp = (u_short *) (cp + 2);
      proto = ntohs(*sp);
      switch (proto) {
      case PROTO_PAP:
        log_Printf(LogLCP, "%s 0x%04x (PAP)\n", request, proto);
        break;
      case PROTO_CHAP:
        log_Printf(LogLCP, "%s 0x%04x (CHAP 0x%02x)\n", request, proto, cp[4]);
        break;
      default:
        log_Printf(LogLCP, "%s 0x%04x\n", request, proto);
        break;
      }

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
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
	  } else if (IsAccepted(lcp->cfg.chap)) {
	    *dec->nakend++ = *cp;
	    *dec->nakend++ = 5;
	    *dec->nakend++ = (unsigned char) (PROTO_CHAP >> 8);
	    *dec->nakend++ = (unsigned char) PROTO_CHAP;
	    *dec->nakend++ = 0x05;
	  } else
	    goto reqreject;
	  break;

	case PROTO_CHAP:
	  if (length < 5) {
	    log_Printf(LogLCP, " Bad length!\n");
	    goto reqreject;
	  }
#ifdef HAVE_DES
          if (IsAccepted(lcp->cfg.chap) && (cp[4] == 0x05 || cp[4] == 0x80))
#else
          if (IsAccepted(lcp->cfg.chap) && cp[4] == 0x05)
#endif
	  {
	    lcp->his_auth = proto;
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
#ifdef HAVE_DES
            link2physical(fp->link)->dl->chap.using_MSChap = cp[4] == 0x80;
#endif
	  } else if (IsAccepted(lcp->cfg.pap)) {
	    *dec->nakend++ = *cp;
	    *dec->nakend++ = 4;
	    *dec->nakend++ = (unsigned char) (PROTO_PAP >> 8);
	    *dec->nakend++ = (unsigned char) PROTO_PAP;
	  } else
	    goto reqreject;
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
          if (IsEnabled(lcp->cfg.pap))
            lcp->want_auth = PROTO_PAP;
          else {
            log_Printf(LogLCP, "Peer will only send PAP (not enabled)\n");
	    lcp->his_reject |= (1 << type);
          }
          break;
	case PROTO_CHAP:
          if (IsEnabled(lcp->cfg.chap))
            lcp->want_auth = PROTO_CHAP;
          else {
            log_Printf(LogLCP, "Peer will only send CHAP (not enabled)\n");
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
      log_Printf(LogLCP, "%s proto %x, interval %ldms\n",
                request, ntohs(req->proto), (long)ntohl(req->period) * 10);
      switch (mode_type) {
      case MODE_REQ:
	if (ntohs(req->proto) != PROTO_LQR || !IsAccepted(lcp->cfg.lqr))
	  goto reqreject;
	else {
	  lcp->his_lqrperiod = ntohl(req->period);
	  if (lcp->his_lqrperiod < 500)
	    lcp->his_lqrperiod = 500;
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
      magic = ntohl(*(u_int32_t *)(cp + 2));
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
	  /*
	   * MorningStar before v1.3 needs NAK
	   */
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
	  /*
	   * MorningStar before v1.3 needs NAK
	   */
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
      log_Printf(LogLCP, "%s %s\n", request,
                mp_Enddisc(cp[2], cp + 3, length - 3));
      switch (mode_type) {
      case MODE_REQ:
        if (!p) {
          log_Printf(LogLCP, " ENDDISC rejected - not a physical link\n");
	  goto reqreject;
        } else if (length-3 < sizeof p->dl->peer.enddisc.address &&
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
            log_Printf(LogLCP, " ENDDISC rejected - local max length is %d\n",
                      sizeof p->dl->peer.enddisc.address - 1);
	  goto reqreject;
        }
	break;

      case MODE_NAK:	/* Treat this as a REJ, we don't vary or disc */
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
    if (dec->rejend != dec->rej) {
      /* rejects are preferred */
      dec->ackend = dec->ack;
      dec->nakend = dec->nak;
    } else if (dec->nakend != dec->nak)
      /* then NAKs */
      dec->ackend = dec->ack;
  }
}

void
lcp_Input(struct lcp *lcp, struct mbuf * bp)
{
  /* Got PROTO_LCP from link */
  fsm_Input(&lcp->fsm, bp);
}

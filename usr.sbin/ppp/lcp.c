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
 * $Id: lcp.c,v 1.55.2.31 1998/03/20 19:46:55 brian Exp $
 *
 * TODO:
 *	o Limit data field length by MRU
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/select.h>
#include <net/if_tun.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

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
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "lcpproto.h"
#include "filter.h"
#include "descriptor.h"
#include "bundle.h"
#include "lqr.h"
#include "hdlc.h"
#include "ccp.h"
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
#include "link.h"
#include "physical.h"
#include "prompt.h"
#include "chat.h"
#include "datalink.h"

/* for received LQRs */
struct lqrreq {
  u_char type;
  u_char length;
  u_short proto;		/* Quality protocol */
  u_long period;		/* Reporting interval */
};

static void LcpLayerUp(struct fsm *);
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
  NullRecvResetReq,
  NullRecvResetAck
};

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

int
lcp_ReportStatus(struct cmdargs const *arg)
{
  prompt_Printf(&prompt, "%s [%s]\n", arg->cx->lcp.fsm.name,
                StateNames[arg->cx->lcp.fsm.state]);
  prompt_Printf(&prompt,
	        " his side: MRU %d, ACCMAP %08lx, PROTOCOMP %d, ACFCOMP %d,\n"
	        "           MAGIC %08lx, REJECT %04x\n",
	        arg->cx->lcp.his_mru, (u_long)arg->cx->lcp.his_accmap,
                arg->cx->lcp.his_protocomp, arg->cx->lcp.his_acfcomp,
                (u_long)arg->cx->lcp.his_magic, arg->cx->lcp.his_reject);
  prompt_Printf(&prompt,
	        " my  side: MRU %d, ACCMAP %08lx, PROTOCOMP %d, ACFCOMP %d,\n"
                "           MAGIC %08lx, REJECT %04x\n",
                arg->cx->lcp.want_mru, (u_long)arg->cx->lcp.want_accmap,
                arg->cx->lcp.want_protocomp, arg->cx->lcp.want_acfcomp,
                (u_long)arg->cx->lcp.want_magic, arg->cx->lcp.my_reject);
  prompt_Printf(&prompt, "\nDefaults:   MRU = %d, ACCMAP = %08lx\t",
                VarMRU, (u_long)VarAccmap);
  prompt_Printf(&prompt, "Open Mode: %s",
                (VarOpenMode == OPEN_PASSIVE) ? "passive" : "active");
  if (VarOpenMode > 0)
    prompt_Printf(&prompt, " (delay %d)", VarOpenMode);
  prompt_Printf(&prompt, "\n");
  return 0;
}

static u_int32_t
GenerateMagic(void)
{
  /* Generate random number which will be used as magic number */
  randinit();
  return (random());
}

void
lcp_Init(struct lcp *lcp, struct bundle *bundle, struct physical *physical,
         const struct fsm_parent *parent)
{
  /* Initialise ourselves */
  fsm_Init(&lcp->fsm, "LCP", PROTO_LCP, LCP_MAXCODE, 10, LogLCP, bundle,
           &physical->link, parent, &lcp_Callbacks);
  lcp_Setup(lcp, 1);
}

void
lcp_Setup(struct lcp *lcp, int openmode)
{
  struct physical *p = link2physical(lcp->fsm.link);

  lcp->fsm.open_mode = openmode;
  lcp->fsm.maxconfig = 10;

  hdlc_Init(&p->hdlc);
  async_Init(&p->async);

  lcp->his_mru = DEF_MRU;
  lcp->his_accmap = 0xffffffff;
  lcp->his_magic = 0;
  lcp->his_lqrperiod = 0;
  lcp->his_protocomp = 0;
  lcp->his_acfcomp = 0;
  lcp->his_auth = 0;

  lcp->want_mru = VarMRU;
  lcp->want_accmap = VarAccmap;
  lcp->want_magic = GenerateMagic();
  lcp->want_auth = Enabled(ConfChap) ? PROTO_CHAP :
                   Enabled(ConfPap) ?  PROTO_PAP : 0;
  lcp->want_lqrperiod = Enabled(ConfLqr) ?  VarLqrTimeout * 100 : 0;
  lcp->want_protocomp = Enabled(ConfProtocomp) ? 1 : 0;
  lcp->want_acfcomp = Enabled(ConfAcfcomp) ? 1 : 0;

  lcp->his_reject = lcp->my_reject = 0;
  lcp->auth_iwait = lcp->auth_ineed = 0;
}

static void
LcpInitRestartCounter(struct fsm * fp)
{
  /* Set fsm timer load */
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

static void
LcpSendConfigReq(struct fsm *fp)
{
  /* Send config REQ please */
  struct physical *p = link2physical(fp->link);
  struct lcp *lcp = fsm2lcp(fp);
  u_char buff[100];
  struct lcp_opt *o;

  if (!p) {
    LogPrintf(LogERROR, "LcpSendConfigReq: Not a physical link !\n");
    return;
  }

  o = (struct lcp_opt *)buff;
  if (!Physical_IsSync(p)) {
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
#ifdef HAVE_DES
    if (VarMSChap)
      o->data[2] = 0x80;
    else
#endif
      o->data[2] = 0x05;
    INC_LCP_OPT(TY_AUTHPROTO, 5, o);
    break;
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid, buff, (u_char *)o - buff);
}

void
lcp_SendProtoRej(struct lcp *lcp, u_char *option, int count)
{
  /* Don't understand `option' */
  FsmOutput(&lcp->fsm, CODE_PROTOREJ, lcp->fsm.reqid, option, count);
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
  FsmOutput(fp, CODE_TERMACK, id, NULL, 0);
}

static void
LcpLayerStart(struct fsm *fp)
{
  /* We're about to start up ! */
  struct lcp *lcp = fsm2lcp(fp);

  LogPrintf(LogLCP, "LcpLayerStart\n");
  lcp->LcpFailedMagic = 0;
}

static void
LcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  LogPrintf(LogLCP, "LcpLayerFinish\n");
}

static void
LcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct physical *p = link2physical(fp->link);
  struct lcp *lcp = fsm2lcp(fp);

  LogPrintf(LogLCP, "LcpLayerUp\n");

  if (p) {
    async_SetLinkParams(&p->async, lcp);
    StartLqm(lcp);
    hdlc_StartTimer(&p->hdlc);
  } else
    LogPrintf(LogERROR, "LcpLayerUp: Not a physical link !\n");
}

static void
LcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  struct physical *p = link2physical(fp->link);

  LogPrintf(LogLCP, "LcpLayerDown\n");
  hdlc_StopTimer(&p->hdlc);
  StopLqrTimer(p);
}

static void
LcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type,
                struct fsm_decode *dec)
{
  /* Deal with incoming PROTO_LCP */
  struct lcp *lcp = fsm2lcp(fp);
  int type, length, sz, pos;
  u_int32_t *lp, magic, accmap;
  u_short mtu, mru, *sp, proto;
  struct lqrreq *req;
  char request[20], desc[22];

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
	  memcpy(dec->nakend, cp, 4);
	  dec->nakend += 4;
	} else if (mru < MIN_MRU) {
	  *sp = htons(MIN_MRU);
	  memcpy(dec->nakend, cp, 4);
	  dec->nakend += 4;
	} else {
	  lcp->his_mru = mru;
	  memcpy(dec->ackend, cp, 4);
	  dec->ackend += 4;
	}
	break;
      case MODE_NAK:
	if (mru >= MIN_MRU || mru <= MAX_MRU)
	  lcp->want_mru = mru;
	break;
      case MODE_REJ:
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_ACCMAP:
      lp = (u_int32_t *) (cp + 2);
      accmap = htonl(*lp);
      LogPrintf(LogLCP, "%s 0x%08lx\n", request, (u_long)accmap);

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
	    lcp->his_auth = proto;
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
	  } else if (Acceptable(ConfChap)) {
	    *dec->nakend++ = *cp;
	    *dec->nakend++ = 5;
	    *dec->nakend++ = (unsigned char) (PROTO_CHAP >> 8);
	    *dec->nakend++ = (unsigned char) PROTO_CHAP;
#ifdef HAVE_DES
            if (VarMSChap)
              *dec->nakend++ = 0x80;
            else
#endif
	      *dec->nakend++ = 5;
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
	    lcp->his_auth = proto;
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
#ifdef HAVE_DES
            VarMSChap = cp[4] == 0x80;
#endif
	  } else if (Acceptable(ConfPap)) {
	    *dec->nakend++ = *cp;
	    *dec->nakend++ = 4;
	    *dec->nakend++ = (unsigned char) (PROTO_PAP >> 8);
	    *dec->nakend++ = (unsigned char) PROTO_PAP;
	  } else
	    goto reqreject;
	  break;

	default:
          LogPrintf(LogLCP, "%s 0x%04x - not recognised, NAK\n",
                    request, proto);
	  memcpy(dec->nakend, cp, length);
	  dec->nakend += length;
	  break;
	}
	break;
      case MODE_NAK:
	switch (proto) {
	case PROTO_PAP:
          if (Enabled(ConfPap))
            lcp->want_auth = PROTO_PAP;
          else {
            LogPrintf(LogLCP, "Peer will only send PAP (not enabled)\n");
	    lcp->his_reject |= (1 << type);
          }
          break;
	case PROTO_CHAP:
          if (Enabled(ConfChap))
            lcp->want_auth = PROTO_CHAP;
          else {
            LogPrintf(LogLCP, "Peer will only send CHAP (not enabled)\n");
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
      LogPrintf(LogLCP, "%s proto %x, interval %dms\n",
                request, ntohs(req->proto), ntohl(req->period) * 10);
      switch (mode_type) {
      case MODE_REQ:
	if (ntohs(req->proto) != PROTO_LQR || !Acceptable(ConfLqr))
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
      lp = (u_int32_t *) (cp + 2);
      magic = ntohl(*lp);
      LogPrintf(LogLCP, "%s 0x%08lx\n", request, (u_long)magic);

      switch (mode_type) {
      case MODE_REQ:
	if (lcp->want_magic) {
	  /* Validate magic number */
	  if (magic == lcp->want_magic) {
	    LogPrintf(LogLCP, "Magic is same (%08lx) - %d times\n",
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
	  lcp->my_reject |= (1 << type);
	  goto reqreject;
	}
	break;
      case MODE_NAK:
	LogPrintf(LogLCP, " Magic 0x%08lx is NAKed!\n", (u_long)magic);
	lcp->want_magic = GenerateMagic();
	break;
      case MODE_REJ:
	LogPrintf(LogLCP, " Magic 0x%08x is REJected!\n", magic);
	lcp->want_magic = 0;
	lcp->his_reject |= (1 << type);
	break;
      }
      break;

    case TY_PROTOCOMP:
      LogPrintf(LogLCP, "%s\n", request);

      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(ConfProtocomp)) {
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
	  memcpy(dec->rejend, cp, 2);
	  dec->rejend += 2;
	  lcp->my_reject |= (1 << type);
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
      LogPrintf(LogLCP, "%s\n", request);
      switch (mode_type) {
      case MODE_REQ:
	if (Acceptable(ConfAcfcomp)) {
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
	  memcpy(dec->rejend, cp, 2);
	  dec->rejend += 2;
	  lcp->my_reject |= (1 << type);
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
        if (length > sizeof dec->rej - (dec->rejend - dec->rej)) {
          length = sizeof dec->rej - (dec->rejend - dec->rej);
          LogPrintf(LogLCP, "Can't REJ length %d - trunating to %d\n",
		    cp[1], length);
        }
	memcpy(dec->rejend, cp, length);
	dec->rejend += length;
	lcp->my_reject |= (1 << type);
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
LcpInput(struct lcp *lcp, struct mbuf * bp)
{
  /* Got PROTO_LCP from link */
  FsmInput(&lcp->fsm, bp);
}

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
 * $FreeBSD$
 *
 *	TODO:
 *		o Support other compression protocols
 */
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* memcpy() on some archs */
#include <termios.h>

#include "layer.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "proto.h"
#include "pred.h"
#include "deflate.h"
#include "throughput.h"
#include "iplist.h"
#include "slcompress.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "prompt.h"
#include "link.h"
#include "mp.h"
#include "async.h"
#include "physical.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#ifdef HAVE_DES
#include "mppe.h"
#endif
#include "bundle.h"

static void CcpSendConfigReq(struct fsm *);
static void CcpSentTerminateReq(struct fsm *);
static void CcpSendTerminateAck(struct fsm *, u_char);
static void CcpDecodeConfig(struct fsm *, u_char *, int, int,
                            struct fsm_decode *);
static void CcpLayerStart(struct fsm *);
static void CcpLayerFinish(struct fsm *);
static int CcpLayerUp(struct fsm *);
static void CcpLayerDown(struct fsm *);
static void CcpInitRestartCounter(struct fsm *, int);
static void CcpRecvResetReq(struct fsm *);
static void CcpRecvResetAck(struct fsm *, u_char);

static struct fsm_callbacks ccp_Callbacks = {
  CcpLayerUp,
  CcpLayerDown,
  CcpLayerStart,
  CcpLayerFinish,
  CcpInitRestartCounter,
  CcpSendConfigReq,
  CcpSentTerminateReq,
  CcpSendTerminateAck,
  CcpDecodeConfig,
  CcpRecvResetReq,
  CcpRecvResetAck
};

static const char * const ccp_TimerNames[] =
  {"CCP restart", "CCP openmode", "CCP stopped"};

static const char *
protoname(int proto)
{
  static char const * const cftypes[] = {
    /* Check out the latest ``Compression Control Protocol'' rfc (1962) */
    "OUI",		/* 0: OUI */
    "PRED1",		/* 1: Predictor type 1 */
    "PRED2",		/* 2: Predictor type 2 */
    "PUDDLE",		/* 3: Puddle Jumber */
    NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL,
    "HWPPC",		/* 16: Hewlett-Packard PPC */
    "STAC",		/* 17: Stac Electronics LZS (rfc1974) */
    "MPPE",		/* 18: Microsoft PPC (rfc2118) and */
			/*     Microsoft PPE (draft-ietf-pppext-mppe) */
    "GAND",		/* 19: Gandalf FZA (rfc1993) */
    "V42BIS",		/* 20: ARG->DATA.42bis compression */
    "BSD",		/* 21: BSD LZW Compress */
    NULL,
    "LZS-DCP",		/* 23: LZS-DCP Compression Protocol (rfc1967) */
    "MAGNALINK/DEFLATE",/* 24: Magnalink Variable Resource (rfc1975) */
			/* 24: Deflate (according to pppd-2.3.*) */
    "DCE",		/* 25: Data Circuit-Terminating Equip (rfc1976) */
    "DEFLATE",		/* 26: Deflate (rfc1979) */
  };

  if (proto < 0 || proto > sizeof cftypes / sizeof *cftypes ||
      cftypes[proto] == NULL)
    return HexStr(proto, NULL, 0);

  return cftypes[proto];
}

/* We support these algorithms, and Req them in the given order */
static const struct ccp_algorithm * const algorithm[] = {
  &DeflateAlgorithm,
  &Pred1Algorithm,
  &PppdDeflateAlgorithm
#ifdef HAVE_DES
  , &MPPEAlgorithm
#endif
};

#define NALGORITHMS (sizeof algorithm/sizeof algorithm[0])

int
ccp_ReportStatus(struct cmdargs const *arg)
{
  struct link *l;
  struct ccp *ccp;

  l = command_ChooseLink(arg);
  ccp = &l->ccp;

  prompt_Printf(arg->prompt, "%s: %s [%s]\n", l->name, ccp->fsm.name,
                State2Nam(ccp->fsm.state));
  if (ccp->fsm.state == ST_OPENED) {
    prompt_Printf(arg->prompt, " My protocol = %s, His protocol = %s\n",
                  protoname(ccp->my_proto), protoname(ccp->his_proto));
    prompt_Printf(arg->prompt, " Output: %ld --> %ld,  Input: %ld --> %ld\n",
                  ccp->uncompout, ccp->compout,
                  ccp->compin, ccp->uncompin);
  }

  prompt_Printf(arg->prompt, "\n Defaults: ");
  prompt_Printf(arg->prompt, "FSM retry = %us, max %u Config"
                " REQ%s, %u Term REQ%s\n", ccp->cfg.fsm.timeout,
                ccp->cfg.fsm.maxreq, ccp->cfg.fsm.maxreq == 1 ? "" : "s",
                ccp->cfg.fsm.maxtrm, ccp->cfg.fsm.maxtrm == 1 ? "" : "s");
  prompt_Printf(arg->prompt, "           deflate windows: ");
  prompt_Printf(arg->prompt, "incoming = %d, ", ccp->cfg.deflate.in.winsize);
  prompt_Printf(arg->prompt, "outgoing = %d\n", ccp->cfg.deflate.out.winsize);
  prompt_Printf(arg->prompt, "           DEFLATE:    %s\n",
                command_ShowNegval(ccp->cfg.neg[CCP_NEG_DEFLATE]));
  prompt_Printf(arg->prompt, "           PREDICTOR1: %s\n",
                command_ShowNegval(ccp->cfg.neg[CCP_NEG_PRED1]));
  prompt_Printf(arg->prompt, "           DEFLATE24:  %s\n",
                command_ShowNegval(ccp->cfg.neg[CCP_NEG_DEFLATE24]));
#ifdef HAVE_DES
  prompt_Printf(arg->prompt, "           MPPE:       %s",
                command_ShowNegval(ccp->cfg.neg[CCP_NEG_MPPE]));
  prompt_Printf(arg->prompt, " (Key Size = %d-bits)\n", ccp->cfg.mppe.keybits);
#endif
  return 0;
}

void
ccp_SetupCallbacks(struct ccp *ccp)
{
  ccp->fsm.fn = &ccp_Callbacks;
  ccp->fsm.FsmTimer.name = ccp_TimerNames[0];
  ccp->fsm.OpenTimer.name = ccp_TimerNames[1];
  ccp->fsm.StoppedTimer.name = ccp_TimerNames[2];
}

void
ccp_Init(struct ccp *ccp, struct bundle *bundle, struct link *l,
         const struct fsm_parent *parent)
{
  /* Initialise ourselves */

  fsm_Init(&ccp->fsm, "CCP", PROTO_CCP, 1, CCP_MAXCODE, LogCCP,
           bundle, l, parent, &ccp_Callbacks, ccp_TimerNames);

  ccp->cfg.deflate.in.winsize = 0;
  ccp->cfg.deflate.out.winsize = 15;
  ccp->cfg.fsm.timeout = DEF_FSMRETRY;
  ccp->cfg.fsm.maxreq = DEF_FSMTRIES;
  ccp->cfg.fsm.maxtrm = DEF_FSMTRIES;
  ccp->cfg.neg[CCP_NEG_DEFLATE] = NEG_ENABLED|NEG_ACCEPTED;
  ccp->cfg.neg[CCP_NEG_PRED1] = NEG_ENABLED|NEG_ACCEPTED;
  ccp->cfg.neg[CCP_NEG_DEFLATE24] = 0;
#ifdef HAVE_DES
  ccp->cfg.mppe.keybits = 128;
  ccp->cfg.neg[CCP_NEG_MPPE] = 0;
#endif

  ccp_Setup(ccp);
}

void
ccp_Setup(struct ccp *ccp)
{
  /* Set ourselves up for a startup */
  ccp->fsm.open_mode = 0;
  ccp->his_proto = ccp->my_proto = -1;
  ccp->reset_sent = ccp->last_reset = -1;
  ccp->in.algorithm = ccp->out.algorithm = -1;
  ccp->in.state = ccp->out.state = NULL;
  ccp->in.opt.id = -1;
  ccp->out.opt = NULL;
  ccp->his_reject = ccp->my_reject = 0;
  ccp->uncompout = ccp->compout = 0;
  ccp->uncompin = ccp->compin = 0;
}

static void
CcpInitRestartCounter(struct fsm *fp, int what)
{
  /* Set fsm timer load */
  struct ccp *ccp = fsm2ccp(fp);

  fp->FsmTimer.load = ccp->cfg.fsm.timeout * SECTICKS;
  switch (what) {
    case FSM_REQ_TIMER:
      fp->restart = ccp->cfg.fsm.maxreq;
      break;
    case FSM_TRM_TIMER:
      fp->restart = ccp->cfg.fsm.maxtrm;
      break;
    default:
      fp->restart = 1;
      break;
  }
}

static void
CcpSendConfigReq(struct fsm *fp)
{
  /* Send config REQ please */
  struct ccp *ccp = fsm2ccp(fp);
  struct ccp_opt **o;
  u_char *cp, buff[100];
  int f, alloc;

  cp = buff;
  o = &ccp->out.opt;
  alloc = ccp->his_reject == 0 && ccp->out.opt == NULL;
  ccp->my_proto = -1;
  ccp->out.algorithm = -1;
  for (f = 0; f < NALGORITHMS; f++)
    if (IsEnabled(ccp->cfg.neg[algorithm[f]->Neg]) &&
        !REJECTED(ccp, algorithm[f]->id)) {

      if (!alloc)
        for (o = &ccp->out.opt; *o != NULL; o = &(*o)->next)
          if ((*o)->val.id == algorithm[f]->id && (*o)->algorithm == f)
            break;

      if (alloc || *o == NULL) {
        *o = (struct ccp_opt *)malloc(sizeof(struct ccp_opt));
        (*o)->val.id = algorithm[f]->id;
        (*o)->val.len = 2;
        (*o)->next = NULL;
        (*o)->algorithm = f;
        (*algorithm[f]->o.OptInit)(&(*o)->val, &ccp->cfg);
      }

      if (cp + (*o)->val.len > buff + sizeof buff) {
        log_Printf(LogERROR, "%s: CCP REQ buffer overrun !\n", fp->link->name);
        break;
      }
      memcpy(cp, &(*o)->val, (*o)->val.len);
      cp += (*o)->val.len;

      ccp->my_proto = (*o)->val.id;
      ccp->out.algorithm = f;

      if (alloc)
        o = &(*o)->next;
    }

  fsm_Output(fp, CODE_CONFIGREQ, fp->reqid, buff, cp - buff, MB_CCPOUT);
}

void
ccp_SendResetReq(struct fsm *fp)
{
  /* We can't read our input - ask peer to reset */
  struct ccp *ccp = fsm2ccp(fp);

  ccp->reset_sent = fp->reqid;
  ccp->last_reset = -1;
  fsm_Output(fp, CODE_RESETREQ, fp->reqid, NULL, 0, MB_CCPOUT);
}

static void
CcpSentTerminateReq(struct fsm *fp)
{
  /* Term REQ just sent by FSM */
}

static void
CcpSendTerminateAck(struct fsm *fp, u_char id)
{
  /* Send Term ACK please */
  fsm_Output(fp, CODE_TERMACK, id, NULL, 0, MB_CCPOUT);
}

static void
CcpRecvResetReq(struct fsm *fp)
{
  /* Got a reset REQ, reset outgoing dictionary */
  struct ccp *ccp = fsm2ccp(fp);
  if (ccp->out.state != NULL)
    (*algorithm[ccp->out.algorithm]->o.Reset)(ccp->out.state);
}

static void
CcpLayerStart(struct fsm *fp)
{
  /* We're about to start up ! */
  struct ccp *ccp = fsm2ccp(fp);

  log_Printf(LogCCP, "%s: LayerStart.\n", fp->link->name);
  fp->more.reqs = fp->more.naks = fp->more.rejs = ccp->cfg.fsm.maxreq * 3;
}

static void
CcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  struct ccp *ccp = fsm2ccp(fp);
  struct ccp_opt *next;

  log_Printf(LogCCP, "%s: LayerDown.\n", fp->link->name);
  if (ccp->in.state != NULL) {
    (*algorithm[ccp->in.algorithm]->i.Term)(ccp->in.state);
    ccp->in.state = NULL;
    ccp->in.algorithm = -1;
  }
  if (ccp->out.state != NULL) {
    (*algorithm[ccp->out.algorithm]->o.Term)(ccp->out.state);
    ccp->out.state = NULL;
    ccp->out.algorithm = -1;
  }
  ccp->his_reject = ccp->my_reject = 0;
  
  while (ccp->out.opt) {
    next = ccp->out.opt->next;
    free(ccp->out.opt);
    ccp->out.opt = next;
  }
  ccp_Setup(ccp);
}

static void
CcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  log_Printf(LogCCP, "%s: LayerFinish.\n", fp->link->name);
}

/*  Called when CCP has reached the OPEN state */
static int
CcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct ccp *ccp = fsm2ccp(fp);

  log_Printf(LogCCP, "%s: LayerUp.\n", fp->link->name);

  if (ccp->in.state == NULL && ccp->in.algorithm >= 0 &&
      ccp->in.algorithm < NALGORITHMS) {
    ccp->in.state = (*algorithm[ccp->in.algorithm]->i.Init)(&ccp->in.opt);
    if (ccp->in.state == NULL) {
      log_Printf(LogERROR, "%s: %s (in) initialisation failure\n",
                fp->link->name, protoname(ccp->his_proto));
      ccp->his_proto = ccp->my_proto = -1;
      fsm_Close(fp);
      return 0;
    }
  }

  if (ccp->out.state == NULL && ccp->out.algorithm >= 0 &&
      ccp->out.algorithm < NALGORITHMS) {
    ccp->out.state = (*algorithm[ccp->out.algorithm]->o.Init)
                       (&ccp->out.opt->val);
    if (ccp->out.state == NULL) {
      log_Printf(LogERROR, "%s: %s (out) initialisation failure\n",
                fp->link->name, protoname(ccp->my_proto));
      ccp->his_proto = ccp->my_proto = -1;
      fsm_Close(fp);
      return 0;
    }
  }

  fp->more.reqs = fp->more.naks = fp->more.rejs = ccp->cfg.fsm.maxreq * 3;

  log_Printf(LogCCP, "%s: Out = %s[%d], In = %s[%d]\n",
            fp->link->name, protoname(ccp->my_proto), ccp->my_proto,
            protoname(ccp->his_proto), ccp->his_proto);

  return 1;
}

static void
CcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type,
                struct fsm_decode *dec)
{
  /* Deal with incoming data */
  struct ccp *ccp = fsm2ccp(fp);
  int type, length, f;
  const char *end;

  if (mode_type == MODE_REQ)
    ccp->in.algorithm = -1;	/* In case we've received two REQs in a row */

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];

    if (length == 0) {
      log_Printf(LogCCP, "%s: CCP size zero\n", fp->link->name);
      break;
    }

    if (length > sizeof(struct lcp_opt)) {
      length = sizeof(struct lcp_opt);
      log_Printf(LogCCP, "%s: Warning: Truncating length to %d\n",
                fp->link->name, length);
    }

    for (f = NALGORITHMS-1; f > -1; f--)
      if (algorithm[f]->id == type)
        break;

    end = f == -1 ? "" : (*algorithm[f]->Disp)((struct lcp_opt *)cp);
    if (end == NULL)
      end = "";

    log_Printf(LogCCP, " %s[%d] %s\n", protoname(type), length, end);

    if (f == -1) {
      /* Don't understand that :-( */
      if (mode_type == MODE_REQ) {
        ccp->my_reject |= (1 << type);
        memcpy(dec->rejend, cp, length);
        dec->rejend += length;
      }
    } else {
      struct ccp_opt *o;

      switch (mode_type) {
      case MODE_REQ:
	if (IsAccepted(ccp->cfg.neg[algorithm[f]->Neg]) &&
            ccp->in.algorithm == -1) {
	  memcpy(&ccp->in.opt, cp, length);
          switch ((*algorithm[f]->i.Set)(&ccp->in.opt, &ccp->cfg)) {
          case MODE_REJ:
	    memcpy(dec->rejend, &ccp->in.opt, ccp->in.opt.len);
	    dec->rejend += ccp->in.opt.len;
            break;
          case MODE_NAK:
	    memcpy(dec->nakend, &ccp->in.opt, ccp->in.opt.len);
	    dec->nakend += ccp->in.opt.len;
            break;
          case MODE_ACK:
	    memcpy(dec->ackend, cp, length);
	    dec->ackend += length;
	    ccp->his_proto = type;
            ccp->in.algorithm = f;		/* This one'll do :-) */
            break;
          }
	} else {
	  memcpy(dec->rejend, cp, length);
	  dec->rejend += length;
	}
	break;
      case MODE_NAK:
        for (o = ccp->out.opt; o != NULL; o = o->next)
          if (o->val.id == cp[0])
            break;
        if (o == NULL)
          log_Printf(LogCCP, "%s: Warning: Ignoring peer NAK of unsent"
                     " option\n", fp->link->name);
        else {
	  memcpy(&o->val, cp, length);
          if ((*algorithm[f]->o.Set)(&o->val) == MODE_ACK)
            ccp->my_proto = algorithm[f]->id;
          else {
	    ccp->his_reject |= (1 << type);
	    ccp->my_proto = -1;
          }
        }
        break;
      case MODE_REJ:
	ccp->his_reject |= (1 << type);
	ccp->my_proto = -1;
	break;
      }
    }

    plen -= cp[1];
    cp += cp[1];
  }

  if (mode_type != MODE_NOP) {
    if (dec->rejend != dec->rej) {
      /* rejects are preferred */
      dec->ackend = dec->ack;
      dec->nakend = dec->nak;
      if (ccp->in.state == NULL) {
        ccp->his_proto = -1;
        ccp->in.algorithm = -1;
      }
    } else if (dec->nakend != dec->nak) {
      /* then NAKs */
      dec->ackend = dec->ack;
      if (ccp->in.state == NULL) {
        ccp->his_proto = -1;
        ccp->in.algorithm = -1;
      }
    }
  }
}

extern struct mbuf *
ccp_Input(struct bundle *bundle, struct link *l, struct mbuf *bp)
{
  /* Got PROTO_CCP from link */
  m_settype(bp, MB_CCPIN);
  if (bundle_Phase(bundle) == PHASE_NETWORK)
    fsm_Input(&l->ccp.fsm, bp);
  else {
    if (bundle_Phase(bundle) < PHASE_NETWORK)
      log_Printf(LogCCP, "%s: Error: Unexpected CCP in phase %s (ignored)\n",
                 l->ccp.fsm.link->name, bundle_PhaseName(bundle));
    m_freem(bp);
  }
  return NULL;
}

static void
CcpRecvResetAck(struct fsm *fp, u_char id)
{
  /* Got a reset ACK, reset incoming dictionary */
  struct ccp *ccp = fsm2ccp(fp);

  if (ccp->reset_sent != -1) {
    if (id != ccp->reset_sent) {
      log_Printf(LogCCP, "%s: Incorrect ResetAck (id %d, not %d)"
                " ignored\n", fp->link->name, id, ccp->reset_sent);
      return;
    }
    /* Whaddaya know - a correct reset ack */
  } else if (id == ccp->last_reset)
    log_Printf(LogCCP, "%s: Duplicate ResetAck (resetting again)\n",
               fp->link->name);
  else {
    log_Printf(LogCCP, "%s: Unexpected ResetAck (id %d) ignored\n",
               fp->link->name, id);
    return;
  }

  ccp->last_reset = ccp->reset_sent;
  ccp->reset_sent = -1;
  if (ccp->in.state != NULL)
    (*algorithm[ccp->in.algorithm]->i.Reset)(ccp->in.state);
}

static struct mbuf *
ccp_LayerPush(struct bundle *b, struct link *l, struct mbuf *bp,
              int pri, u_short *proto)
{
  if (PROTO_COMPRESSIBLE(*proto) && l->ccp.fsm.state == ST_OPENED &&
      l->ccp.out.state != NULL) {
    bp = (*algorithm[l->ccp.out.algorithm]->o.Write)
           (l->ccp.out.state, &l->ccp, l, pri, proto, bp);
    switch (*proto) {
      case PROTO_ICOMPD:
        m_settype(bp, MB_ICOMPDOUT);
        break;
      case PROTO_COMPD:
        m_settype(bp, MB_COMPDOUT);
        break;
    }
  }

  return bp;
}

static struct mbuf *
ccp_LayerPull(struct bundle *b, struct link *l, struct mbuf *bp, u_short *proto)
{
  /*
   * If proto isn't PROTO_[I]COMPD, we still want to pass it to the
   * decompression routines so that the dictionary's updated
   */
  if (l->ccp.fsm.state == ST_OPENED) {
    if (*proto == PROTO_COMPD || *proto == PROTO_ICOMPD) {
      log_Printf(LogDEBUG, "ccp_LayerPull: PROTO_%sCOMPDP -> PROTO_IP\n",
                 *proto == PROTO_ICOMPD ? "I" : "");
      /* Decompress incoming data */
      if (l->ccp.reset_sent != -1)
        /* Send another REQ and put the packet in the bit bucket */
        fsm_Output(&l->ccp.fsm, CODE_RESETREQ, l->ccp.reset_sent, NULL, 0,
                   MB_CCPOUT);
      else if (l->ccp.in.state != NULL) {
        bp = (*algorithm[l->ccp.in.algorithm]->i.Read)
               (l->ccp.in.state, &l->ccp, proto, bp);
        switch (*proto) {
          case PROTO_ICOMPD:
            m_settype(bp, MB_ICOMPDIN);
            break;
          case PROTO_COMPD:
            m_settype(bp, MB_COMPDIN);
            break;
        }
        return bp;
      }
      m_freem(bp);
      bp = NULL;
    } else if (PROTO_COMPRESSIBLE(*proto) && l->ccp.in.state != NULL) {
      log_Printf(LogDEBUG, "ccp_LayerPull: Ignore packet (dict only)\n");
      /* Add incoming Network Layer traffic to our dictionary */
      (*algorithm[l->ccp.in.algorithm]->i.DictSetup)
        (l->ccp.in.state, &l->ccp, *proto, bp);
    } else
      log_Printf(LogDEBUG, "ccp_LayerPull: Ignore packet\n");
  }

  return bp;
}

u_short
ccp_Proto(struct ccp *ccp)
{
  return !link2physical(ccp->fsm.link) || !ccp->fsm.bundle->ncp.mp.active ?
         PROTO_COMPD : PROTO_ICOMPD;
}

int
ccp_SetOpenMode(struct ccp *ccp)
{
  int f;

  for (f = 0; f < CCP_NEG_TOTAL; f++)
    if (IsEnabled(ccp->cfg.neg[f])) {
      ccp->fsm.open_mode = 0;
      return 1;
    }

  ccp->fsm.open_mode = OPEN_PASSIVE;	/* Go straight to ST_STOPPED ? */

  for (f = 0; f < CCP_NEG_TOTAL; f++)
    if (IsAccepted(ccp->cfg.neg[f]))
      return 1;

  return 0;				/* No CCP at all */
}

struct layer ccplayer = { LAYER_CCP, "ccp", ccp_LayerPush, ccp_LayerPull };

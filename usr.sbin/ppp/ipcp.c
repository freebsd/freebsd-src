/*
 *	PPP IP Control Protocol (IPCP) Module
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
 * $Id: ipcp.c,v 1.50.2.3 1998/01/30 19:45:44 brian Exp $
 *
 *	TODO:
 *		o More RFC1772 backwoard compatibility
 */
#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "lcp.h"
#include "iplist.h"
#include "throughput.h"
#include "ipcp.h"
#include "slcompress.h"
#include "os.h"
#include "phase.h"
#include "loadalias.h"
#include "vars.h"
#include "vjcomp.h"
#include "ip.h"
#include "route.h"
#include "filter.h"
#include "hdlc.h"
#include "link.h"
#include "physical.h"

struct compreq {
  u_short proto;
  u_char slots;
  u_char compcid;
};

static void IpcpLayerUp(struct fsm *);
static void IpcpLayerDown(struct fsm *);
static void IpcpLayerStart(struct fsm *);
static void IpcpLayerFinish(struct fsm *);
static void IpcpInitRestartCounter(struct fsm *);
static void IpcpSendConfigReq(struct fsm *);
static void IpcpSendTerminateReq(struct fsm *);
static void IpcpSendTerminateAck(struct fsm *);
static void IpcpDecodeConfig(u_char *, int, int);

struct ipcpstate IpcpInfo = {
  {
    "IPCP",
    PROTO_IPCP,
    IPCP_MAXCODE,
    0,
    ST_INITIAL,
    0, 0, 0,
    {0, 0, 0, NULL, NULL, NULL},	/* FSM timer */
    {0, 0, 0, NULL, NULL, NULL},	/* Open timer */
    {0, 0, 0, NULL, NULL, NULL},	/* Stopped timer */
    LogIPCP,
    NULL,
    IpcpLayerUp,
    IpcpLayerDown,
    IpcpLayerStart,
    IpcpLayerFinish,
    IpcpInitRestartCounter,
    IpcpSendConfigReq,
    IpcpSendTerminateReq,
    IpcpSendTerminateAck,
    IpcpDecodeConfig,
  },
  MAX_VJ_STATES,
  1
};

static const char *cftypes[] = {
  /* Check out the latest ``Assigned numbers'' rfc (rfc1700.txt) */
  "???",
  "IPADDRS",	/* 1: IP-Addresses */	/* deprecated */
  "COMPPROTO",	/* 2: IP-Compression-Protocol */
  "IPADDR",	/* 3: IP-Address */
};

#define NCFTYPES (sizeof cftypes/sizeof cftypes[0])

static const char *cftypes128[] = {
  /* Check out the latest ``Assigned numbers'' rfc (rfc1700.txt) */
  "???",
  "PRIDNS",	/* 129: Primary DNS Server Address */
  "PRINBNS",	/* 130: Primary NBNS Server Address */
  "SECDNS",	/* 131: Secondary DNS Server Address */
  "SECNBNS",	/* 132: Secondary NBNS Server Address */
};

#define NCFTYPES128 (sizeof cftypes128/sizeof cftypes128[0])

void
IpcpAddInOctets(int n)
{
  throughput_addin(&IpcpInfo.throughput, n);
}

void
IpcpAddOutOctets(int n)
{
  throughput_addout(&IpcpInfo.throughput, n);
}

int
ReportIpcpStatus(struct cmdargs const *arg)
{
  if (!VarTerm)
    return 1;
  fprintf(VarTerm, "%s [%s]\n", IpcpInfo.fsm.name,
          StateNames[IpcpInfo.fsm.state]);
  if (IpcpInfo.fsm.state == ST_OPENED) {
    fprintf(VarTerm, " his side: %s, %s\n",
	    inet_ntoa(IpcpInfo.his_ipaddr), vj2asc(IpcpInfo.his_compproto));
    fprintf(VarTerm, " my  side: %s, %s\n",
	    inet_ntoa(IpcpInfo.want_ipaddr), vj2asc(IpcpInfo.want_compproto));
  }

  fprintf(VarTerm, "Defaults:\n");
  fprintf(VarTerm, " My Address:  %s/%d\n",
	  inet_ntoa(IpcpInfo.DefMyAddress.ipaddr), IpcpInfo.DefMyAddress.width);
  if (iplist_isvalid(&IpcpInfo.DefHisChoice))
    fprintf(VarTerm, " His Address: %s\n", IpcpInfo.DefHisChoice.src);
  else
    fprintf(VarTerm, " His Address: %s/%d\n",
	  inet_ntoa(IpcpInfo.DefHisAddress.ipaddr),
          IpcpInfo.DefHisAddress.width);
  if (IpcpInfo.HaveTriggerAddress)
    fprintf(VarTerm, " Negotiation(trigger): %s\n",
            inet_ntoa(IpcpInfo.TriggerAddress));
  else
    fprintf(VarTerm, " Negotiation(trigger): MYADDR\n");

  fprintf(VarTerm, "\n");
  throughput_disp(&IpcpInfo.throughput, VarTerm);

  return 0;
}

void
IpcpDefAddress()
{
  /* Setup default IP addresses (`hostname` -> 0.0.0.0) */
  struct hostent *hp;
  char name[200];

  memset(&IpcpInfo.DefMyAddress, '\0', sizeof IpcpInfo.DefMyAddress);
  memset(&IpcpInfo.DefHisAddress, '\0', sizeof IpcpInfo.DefHisAddress);
  IpcpInfo.HaveTriggerAddress = 0;
  if (gethostname(name, sizeof name) == 0) {
    hp = gethostbyname(name);
    if (hp && hp->h_addrtype == AF_INET)
      memcpy(&IpcpInfo.DefMyAddress.ipaddr.s_addr, hp->h_addr, hp->h_length);
  }
}

int
SetInitVJ(struct cmdargs const *args)
{
  if (args->argc != 2)
    return -1;
  if (!strcasecmp(args->argv[0], "slots")) {
    int slots;

    slots = atoi(args->argv[1]);
    if (slots < 4 || slots > 16)
      return 1;
    IpcpInfo.VJInitSlots = slots;
    return 0;
  } else if (!strcasecmp(args->argv[0], "slotcomp")) {
    if (!strcasecmp(args->argv[1], "on"))
      IpcpInfo.VJInitComp = 1;
    else if (!strcasecmp(args->argv[1], "off"))
      IpcpInfo.VJInitComp = 0;
    else
      return 2;
    return 0;
  }
  return -1;
}

int
ShowInitVJ(struct cmdargs const *args)
{
  if (VarTerm) {
    fprintf(VarTerm, "Initial slots: %d\n", IpcpInfo.VJInitSlots);
    fprintf(VarTerm, "Initial compression: %s\n",
            IpcpInfo.VJInitComp ? "on" : "off");
  }
  return 0;
}

void
IpcpInit(struct link *l)
{
  /* Initialise ourselves */
  FsmInit(&IpcpInfo.fsm, l);
  if (iplist_isvalid(&IpcpInfo.DefHisChoice))
    iplist_setrandpos(&IpcpInfo.DefHisChoice);
  IpcpInfo.his_compproto = 0;
  IpcpInfo.his_reject = IpcpInfo.my_reject = 0;

  if ((mode & MODE_DEDICATED) && !GetLabel()) {
    IpcpInfo.want_ipaddr.s_addr = IpcpInfo.his_ipaddr.s_addr = INADDR_ANY;
    IpcpInfo.his_ipaddr.s_addr = INADDR_ANY;
  } else {
    IpcpInfo.want_ipaddr.s_addr = IpcpInfo.DefMyAddress.ipaddr.s_addr;
    IpcpInfo.his_ipaddr.s_addr = IpcpInfo.DefHisAddress.ipaddr.s_addr;
  }

  /*
   * Some implementations of PPP require that we send a
   * *special* value as our address, even though the rfc specifies
   * full negotiation (e.g. "0.0.0.0" or Not "0.0.0.0").
   */
  if (IpcpInfo.HaveTriggerAddress) {
    IpcpInfo.want_ipaddr.s_addr = IpcpInfo.TriggerAddress.s_addr;
    LogPrintf(LogIPCP, "Using trigger address %s\n",
              inet_ntoa(IpcpInfo.TriggerAddress));
  }

  if (Enabled(ConfVjcomp))
    IpcpInfo.want_compproto = (PROTO_VJCOMP << 16) +
                              ((IpcpInfo.VJInitSlots - 1) << 8) +
                              IpcpInfo.VJInitComp;
  else
    IpcpInfo.want_compproto = 0;

  IpcpInfo.heis1172 = 0;
  IpcpInfo.fsm.maxconfig = 10;
  throughput_init(&IpcpInfo.throughput);
}

static void
IpcpInitRestartCounter(struct fsm * fp)
{
  /* Set fsm timer load */
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

static void
IpcpSendConfigReq(struct fsm *fp)
{
  /* Send config REQ please */
  struct physical *p = link2physical(fp->link);
  u_char *cp;
  struct lcp_opt o;

  cp = ReqBuff;
  LogPrintf(LogIPCP, "IpcpSendConfigReq\n");
  if ((p && !Physical_IsSync(p)) || !REJECTED(&IpcpInfo, TY_IPADDR)) {
    o.id = TY_IPADDR;
    o.len = 6;
    *(u_long *)o.data = IpcpInfo.want_ipaddr.s_addr;
    cp += LcpPutConf(LogIPCP, cp, &o, cftypes[o.id],
                     inet_ntoa(IpcpInfo.want_ipaddr));
  }

  if (IpcpInfo.want_compproto && !REJECTED(&IpcpInfo, TY_COMPPROTO)) {
    const char *args;
    o.id = TY_COMPPROTO;
    if (IpcpInfo.heis1172) {
      o.len = 4;
      *(u_short *)o.data = htons(PROTO_VJCOMP);
      args = "";
    } else {
      o.len = 6;
      *(u_long *)o.data = htonl(IpcpInfo.want_compproto);
      args = vj2asc(IpcpInfo.want_compproto);
    }
    cp += LcpPutConf(LogIPCP, cp, &o, cftypes[o.id], args);
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

static void
IpcpSendTerminateReq(struct fsm * fp)
{
  /* Term REQ just sent by FSM */
}

static void
IpcpSendTerminateAck(struct fsm * fp)
{
  /* Send Term ACK please */
  LogPrintf(LogIPCP, "IpcpSendTerminateAck\n");
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

static void
IpcpLayerStart(struct fsm * fp)
{
  /* We're about to start up ! */
  LogPrintf(LogIPCP, "IpcpLayerStart.\n");
}

static void
IpcpLayerFinish(struct fsm * fp)
{
  /* We're now down */
  LogPrintf(LogIPCP, "IpcpLayerFinish.\n");
  /* Better tell LCP that it's all over (we're the only NCP) */
  reconnect(RECON_FALSE);
  LcpClose(&LcpInfo.fsm);
}

static void
IpcpLayerDown(struct fsm * fp)
{
  /* About to come down */
  LogPrintf(LogIPCP, "IpcpLayerDown.\n");
  throughput_stop(&IpcpInfo.throughput);
  throughput_log(&IpcpInfo.throughput, LogIPCP, NULL);
}

static void
IpcpLayerUp(struct fsm * fp)
{
  /* We're now up */
  char tbuff[100];

  Prompt();
  LogPrintf(LogIPCP, "IpcpLayerUp(%d).\n", fp->state);
  snprintf(tbuff, sizeof tbuff, "myaddr = %s ",
	   inet_ntoa(IpcpInfo.want_ipaddr));

  if (IpcpInfo.his_compproto >> 16 == PROTO_VJCOMP)
    VjInit((IpcpInfo.his_compproto >> 8) & 255);

  LogPrintf(LogIsKept(LogIPCP) ? LogIPCP : LogLINK, " %s hisaddr = %s\n",
	    tbuff, inet_ntoa(IpcpInfo.his_ipaddr));
  if (OsSetIpaddress(IpcpInfo.want_ipaddr, IpcpInfo.his_ipaddr) < 0) {
    if (VarTerm)
      LogPrintf(LogERROR, "IpcpLayerUp: unable to set ip address\n");
    return;
  }
#ifndef NOALIAS
  if (mode & MODE_ALIAS)
    VarPacketAliasSetAddress(IpcpInfo.want_ipaddr);
#endif
  OsLinkup();
  throughput_start(&IpcpInfo.throughput);
  StartIdleTimer();
}

void
IpcpUp()
{
  /* Lower layers are ready.... go */
  FsmUp(&IpcpInfo.fsm);
  LogPrintf(LogIPCP, "IPCP Up event!!\n");
}

void
IpcpDown()
{
  /* Physical link is gone - sudden death */
  if (IpcpInfo.fsm.state >= ST_CLOSED) {
    FsmDown(&IpcpInfo.fsm);
    /* FsmDown() results in an IpcpLayerDown() if we're currently open. */
    IpcpLayerFinish(&IpcpInfo.fsm);
  }
}

void
IpcpOpen()
{
  /* Start IPCP please */
  FsmOpen(&IpcpInfo.fsm);
}

static int
AcceptableAddr(struct in_range *prange, struct in_addr ipaddr)
{
  /* Is the given IP in the given range ? */
  LogPrintf(LogDEBUG, "requested = %x\n", htonl(ipaddr.s_addr));
  LogPrintf(LogDEBUG, "range = %x\n", htonl(prange->ipaddr.s_addr));
  LogPrintf(LogDEBUG, "/%x\n", htonl(prange->mask.s_addr));
  LogPrintf(LogDEBUG, "%x, %x\n", htonl(prange->ipaddr.s_addr & prange->
		  mask.s_addr), htonl(ipaddr.s_addr & prange->mask.s_addr));
  return (prange->ipaddr.s_addr & prange->mask.s_addr) ==
    (ipaddr.s_addr & prange->mask.s_addr) && ipaddr.s_addr;
}

static void
IpcpDecodeConfig(u_char * cp, int plen, int mode_type)
{
  /* Deal with incoming PROTO_IPCP */
  int type, length;
  u_long *lp, compproto;
  struct compreq *pcomp;
  struct in_addr ipaddr, dstipaddr, dnsstuff, ms_info_req;
  char tbuff[100];
  char tbuff2[100];

  ackp = AckBuff;
  nakp = NakBuff;
  rejp = RejBuff;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];
    if (type < NCFTYPES)
      snprintf(tbuff, sizeof tbuff, " %s[%d] ", cftypes[type], length);
    else if (type > 128 && type < 128 + NCFTYPES128)
      snprintf(tbuff, sizeof tbuff, " %s[%d] ", cftypes128[type-128], length);
    else
      snprintf(tbuff, sizeof tbuff, " <%d>[%d] ", type, length);

    switch (type) {
    case TY_IPADDR:		/* RFC1332 */
      lp = (u_long *) (cp + 2);
      ipaddr.s_addr = *lp;
      LogPrintf(LogIPCP, "%s %s\n", tbuff, inet_ntoa(ipaddr));

      switch (mode_type) {
      case MODE_REQ:
        if (iplist_isvalid(&IpcpInfo.DefHisChoice)) {
          if (ipaddr.s_addr == INADDR_ANY ||
              iplist_ip2pos(&IpcpInfo.DefHisChoice, ipaddr) < 0 ||
              OsTrySetIpaddress(IpcpInfo.DefMyAddress.ipaddr, ipaddr) != 0) {
            LogPrintf(LogIPCP, "%s: Address invalid or already in use\n",
                      inet_ntoa(ipaddr));
            IpcpInfo.his_ipaddr = ChooseHisAddr(IpcpInfo.DefMyAddress.ipaddr);
            if (IpcpInfo.his_ipaddr.s_addr == INADDR_ANY) {
	      memcpy(rejp, cp, length);
	      rejp += length;
            } else {
	      memcpy(nakp, cp, 2);
	      memcpy(nakp+2, &IpcpInfo.his_ipaddr.s_addr, length - 2);
	      nakp += length;
            }
	    break;
          }
	} else if (!AcceptableAddr(&IpcpInfo.DefHisAddress, ipaddr)) {
	  /*
	   * If destination address is not acceptable, insist to use what we
	   * want to use.
	   */
	  memcpy(nakp, cp, 2);
	  memcpy(nakp+2, &IpcpInfo.his_ipaddr.s_addr, length - 2);
	  nakp += length;
	  break;
	}
	IpcpInfo.his_ipaddr = ipaddr;
	memcpy(ackp, cp, length);
	ackp += length;
	break;
      case MODE_NAK:
	if (AcceptableAddr(&IpcpInfo.DefMyAddress, ipaddr)) {
	  /* Use address suggested by peer */
	  snprintf(tbuff2, sizeof tbuff2, "%s changing address: %s ", tbuff,
		   inet_ntoa(IpcpInfo.want_ipaddr));
	  LogPrintf(LogIPCP, "%s --> %s\n", tbuff2, inet_ntoa(ipaddr));
	  IpcpInfo.want_ipaddr = ipaddr;
	} else {
	  LogPrintf(LogIPCP, "%s: Unacceptable address!\n", inet_ntoa(ipaddr));
          FsmClose(&IpcpInfo.fsm);
	}
	break;
      case MODE_REJ:
	IpcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_COMPPROTO:
      lp = (u_long *) (cp + 2);
      compproto = htonl(*lp);
      LogPrintf(LogIPCP, "%s %s\n", tbuff, vj2asc(compproto));

      switch (mode_type) {
      case MODE_REQ:
	if (!Acceptable(ConfVjcomp)) {
	  memcpy(rejp, cp, length);
	  rejp += length;
	} else {
	  pcomp = (struct compreq *) (cp + 2);
	  switch (length) {
	  case 4:		/* RFC1172 */
	    if (ntohs(pcomp->proto) == PROTO_VJCOMP) {
	      LogPrintf(LogWARN, "Peer is speaking RFC1172 compression protocol !\n");
	      IpcpInfo.heis1172 = 1;
	      IpcpInfo.his_compproto = compproto;
	      memcpy(ackp, cp, length);
	      ackp += length;
	    } else {
	      memcpy(nakp, cp, 2);
	      pcomp->proto = htons(PROTO_VJCOMP);
	      memcpy(nakp+2, &pcomp, 2);
	      nakp += length;
	    }
	    break;
	  case 6:		/* RFC1332 */
	    if (ntohs(pcomp->proto) == PROTO_VJCOMP
		&& pcomp->slots < MAX_VJ_STATES && pcomp->slots > 2) {
	      IpcpInfo.his_compproto = compproto;
	      IpcpInfo.heis1172 = 0;
	      memcpy(ackp, cp, length);
	      ackp += length;
	    } else {
	      memcpy(nakp, cp, 2);
	      pcomp->proto = htons(PROTO_VJCOMP);
	      pcomp->slots = MAX_VJ_STATES - 1;
	      pcomp->compcid = 0;
	      memcpy(nakp+2, &pcomp, sizeof pcomp);
	      nakp += length;
	    }
	    break;
	  default:
	    memcpy(rejp, cp, length);
	    rejp += length;
	    break;
	  }
	}
	break;
      case MODE_NAK:
	LogPrintf(LogIPCP, "%s changing compproto: %08x --> %08x\n",
		  tbuff, IpcpInfo.want_compproto, compproto);
	IpcpInfo.want_compproto = compproto;
	break;
      case MODE_REJ:
	IpcpInfo.his_reject |= (1 << type);
	break;
      }
      break;
    case TY_IPADDRS:		/* RFC1172 */
      lp = (u_long *) (cp + 2);
      ipaddr.s_addr = *lp;
      lp = (u_long *) (cp + 6);
      dstipaddr.s_addr = *lp;
      snprintf(tbuff2, sizeof tbuff2, "%s %s,", tbuff, inet_ntoa(ipaddr));
      LogPrintf(LogIPCP, "%s %s\n", tbuff2, inet_ntoa(dstipaddr));

      switch (mode_type) {
      case MODE_REQ:
	IpcpInfo.his_ipaddr = ipaddr;
	IpcpInfo.want_ipaddr = dstipaddr;
	memcpy(ackp, cp, length);
	ackp += length;
	break;
      case MODE_NAK:
        snprintf(tbuff2, sizeof tbuff2, "%s changing address: %s", tbuff,
		 inet_ntoa(IpcpInfo.want_ipaddr));
	LogPrintf(LogIPCP, "%s --> %s\n", tbuff2, inet_ntoa(ipaddr));
	IpcpInfo.want_ipaddr = ipaddr;
	IpcpInfo.his_ipaddr = dstipaddr;
	break;
      case MODE_REJ:
	IpcpInfo.his_reject |= (1 << type);
	break;
      }
      break;

      /*
       * MS extensions for MS's PPP
       */

#ifndef NOMSEXT
    case TY_PRIMARY_DNS:	/* MS PPP DNS negotiation hack */
    case TY_SECONDARY_DNS:
      if (!Enabled(ConfMSExt)) {
	LogPrintf(LogIPCP, "MS NS req - rejected - msext disabled\n");
	IpcpInfo.my_reject |= (1 << type);
	memcpy(rejp, cp, length);
	rejp += length;
	break;
      }
      switch (mode_type) {
      case MODE_REQ:
	lp = (u_long *) (cp + 2);
	dnsstuff.s_addr = *lp;
	ms_info_req.s_addr = IpcpInfo.ns_entries
          [(type - TY_PRIMARY_DNS) ? 1 : 0].s_addr;
	if (dnsstuff.s_addr != ms_info_req.s_addr) {

	  /*
	   * So the client has got the DNS stuff wrong (first request) so
	   * we'll tell 'em how it is
	   */
	  memcpy(nakp, cp, 2);	/* copy first two (type/length) */
	  LogPrintf(LogIPCP, "MS NS req %d:%s->%s - nak\n",
		    type,
		    inet_ntoa(dnsstuff),
		    inet_ntoa(ms_info_req));
	  memcpy(nakp+2, &ms_info_req, length);
	  nakp += length;
	  break;
	}

	/*
	 * Otherwise they have it right (this time) so we send a ack packet
	 * back confirming it... end of story
	 */
	LogPrintf(LogIPCP, "MS NS req %d:%s ok - ack\n",
		  type,
		  inet_ntoa(ms_info_req));
	memcpy(ackp, cp, length);
	ackp += length;
	break;
      case MODE_NAK:		/* what does this mean?? */
	LogPrintf(LogIPCP, "MS NS req %d - NAK??\n", type);
	break;
      case MODE_REJ:		/* confused?? me to :) */
	LogPrintf(LogIPCP, "MS NS req %d - REJ??\n", type);
	break;
      }
      break;

    case TY_PRIMARY_NBNS:	/* MS PPP NetBIOS nameserver hack */
    case TY_SECONDARY_NBNS:
      if (!Enabled(ConfMSExt)) {
	LogPrintf(LogIPCP, "MS NBNS req - rejected - msext disabled\n");
	IpcpInfo.my_reject |= (1 << type);
	memcpy(rejp, cp, length);
	rejp += length;
	break;
      }
      switch (mode_type) {
      case MODE_REQ:
	lp = (u_long *) (cp + 2);
	dnsstuff.s_addr = *lp;
	ms_info_req.s_addr = IpcpInfo.nbns_entries
          [(type - TY_PRIMARY_NBNS) ? 1 : 0].s_addr;
	if (dnsstuff.s_addr != ms_info_req.s_addr) {
	  memcpy(nakp, cp, 2);
	  memcpy(nakp+2, &ms_info_req.s_addr, length);
	  LogPrintf(LogIPCP, "MS NBNS req %d:%s->%s - nak\n",
		    type,
		    inet_ntoa(dnsstuff),
		    inet_ntoa(ms_info_req));
	  nakp += length;
	  break;
	}
	LogPrintf(LogIPCP, "MS NBNS req %d:%s ok - ack\n",
		  type,
		  inet_ntoa(ms_info_req));
	memcpy(ackp, cp, length);
	ackp += length;
	break;
      case MODE_NAK:
	LogPrintf(LogIPCP, "MS NBNS req %d - NAK??\n", type);
	break;
      case MODE_REJ:
	LogPrintf(LogIPCP, "MS NBNS req %d - REJ??\n", type);
	break;
      }
      break;

#endif

    default:
      IpcpInfo.my_reject |= (1 << type);
      memcpy(rejp, cp, length);
      rejp += length;
      break;
    }
    plen -= length;
    cp += length;
  }
}

void
IpcpInput(struct mbuf * bp)
{
  /* Got PROTO_IPCP from link */
  FsmInput(&IpcpInfo.fsm, bp);
}

int
UseHisaddr(const char *hisaddr, int setaddr)
{
  /* Use `hisaddr' for the peers address (set iface if `setaddr') */
  memset(&IpcpInfo.DefHisAddress, '\0', sizeof IpcpInfo.DefHisAddress);
  iplist_reset(&IpcpInfo.DefHisChoice);
  if (strpbrk(hisaddr, ",-")) {
    iplist_setsrc(&IpcpInfo.DefHisChoice, hisaddr);
    if (iplist_isvalid(&IpcpInfo.DefHisChoice)) {
      iplist_setrandpos(&IpcpInfo.DefHisChoice);
      IpcpInfo.his_ipaddr = ChooseHisAddr(IpcpInfo.want_ipaddr);
      if (IpcpInfo.his_ipaddr.s_addr == INADDR_ANY) {
        LogPrintf(LogWARN, "%s: None available !\n", IpcpInfo.DefHisChoice.src);
        return(0);
      }
      IpcpInfo.DefHisAddress.ipaddr.s_addr = IpcpInfo.his_ipaddr.s_addr;
      IpcpInfo.DefHisAddress.mask.s_addr = INADDR_BROADCAST;
      IpcpInfo.DefHisAddress.width = 32;
    } else {
      LogPrintf(LogWARN, "%s: Invalid range !\n", hisaddr);
      return 0;
    }
  } else if (ParseAddr(1, &hisaddr, &IpcpInfo.DefHisAddress.ipaddr,
		       &IpcpInfo.DefHisAddress.mask,
                       &IpcpInfo.DefHisAddress.width) != 0) {
    IpcpInfo.his_ipaddr.s_addr = IpcpInfo.DefHisAddress.ipaddr.s_addr;

    if (setaddr && OsSetIpaddress
	(IpcpInfo.DefMyAddress.ipaddr, IpcpInfo.DefHisAddress.ipaddr) < 0) {
      IpcpInfo.DefMyAddress.ipaddr.s_addr = INADDR_ANY;
      IpcpInfo.DefHisAddress.ipaddr.s_addr = INADDR_ANY;
      return 0;
    }
  } else
    return 0;

  return 1;
}

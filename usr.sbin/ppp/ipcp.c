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
 * $Id: ipcp.c,v 1.37 1997/11/18 14:52:04 brian Exp $
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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "lcp.h"
#include "ipcp.h"
#include "slcompress.h"
#include "os.h"
#include "phase.h"
#include "loadalias.h"
#include "vars.h"
#include "vjcomp.h"
#include "ip.h"
#include "throughput.h"

#ifndef NOMSEXT
struct in_addr ns_entries[2];
struct in_addr nbns_entries[2];
#endif

struct ipcpstate IpcpInfo;
struct in_range  DefMyAddress;
struct in_range  DefHisAddress;
struct in_addr   TriggerAddress;
int HaveTriggerAddress;

static void IpcpSendConfigReq(struct fsm *);
static void IpcpSendTerminateAck(struct fsm *);
static void IpcpSendTerminateReq(struct fsm *);
static void IpcpDecodeConfig(u_char *, int, int);
static void IpcpLayerStart(struct fsm *);
static void IpcpLayerFinish(struct fsm *);
static void IpcpLayerUp(struct fsm *);
static void IpcpLayerDown(struct fsm *);
static void IpcpInitRestartCounter(struct fsm *);

#define	REJECTED(p, x)	(p->his_reject & (1<<x))

struct fsm IpcpFsm = {
  "IPCP",
  PROTO_IPCP,
  IPCP_MAXCODE,
  OPEN_ACTIVE,
  ST_INITIAL,
  0, 0, 0,

  0,
  {0, 0, 0, NULL, NULL, NULL},
  {0, 0, 0, NULL, NULL, NULL},
  LogIPCP,

  IpcpLayerUp,
  IpcpLayerDown,
  IpcpLayerStart,
  IpcpLayerFinish,
  IpcpInitRestartCounter,
  IpcpSendConfigReq,
  IpcpSendTerminateReq,
  IpcpSendTerminateAck,
  IpcpDecodeConfig,
};

static const char *cftypes[] = {
  /* Check out the latest ``Assigned numbers'' rfc (rfc1700.txt) */
  "???",
  "IPADDRS",	/* 1: IP-Addresses */	/* deprecated */
  "COMPPROTO",	/* 2: IP-Compression-Protocol */
  "IPADDR",	/* 3: IP-Address */
};

#define NCFTYPES (sizeof(cftypes)/sizeof(char *))

static const char *cftypes128[] = {
  /* Check out the latest ``Assigned numbers'' rfc (rfc1700.txt) */
  "???",
  "PRIDNS",	/* 129: Primary DNS Server Address */
  "PRINBNS",	/* 130: Primary NBNS Server Address */
  "SECDNS",	/* 131: Secondary DNS Server Address */
  "SECNBNS",	/* 132: Secondary NBNS Server Address */
};

#define NCFTYPES128 (sizeof(cftypes)/sizeof(char *))

struct pppThroughput throughput;

void
IpcpAddInOctets(int n)
{
  throughput_addin(&throughput, n);
}

void
IpcpAddOutOctets(int n)
{
  throughput_addout(&throughput, n);
}

int
ReportIpcpStatus(struct cmdargs const *arg)
{
  struct ipcpstate *icp = &IpcpInfo;
  struct fsm *fp = &IpcpFsm;

  if (!VarTerm)
    return 1;
  fprintf(VarTerm, "%s [%s]\n", fp->name, StateNames[fp->state]);
  fprintf(VarTerm, " his side: %s, %lx\n",
	  inet_ntoa(icp->his_ipaddr), icp->his_compproto);
  fprintf(VarTerm, " my  side: %s, %lx\n",
	  inet_ntoa(icp->want_ipaddr), icp->want_compproto);

  fprintf(VarTerm, "Defaults:\n");
  fprintf(VarTerm, " My Address:  %s/%d\n",
	  inet_ntoa(DefMyAddress.ipaddr), DefMyAddress.width);
  fprintf(VarTerm, " His Address: %s/%d\n",
	  inet_ntoa(DefHisAddress.ipaddr), DefHisAddress.width);
  if (HaveTriggerAddress)
    fprintf(VarTerm, " Negotiation(trigger): %s\n", inet_ntoa(TriggerAddress));
  else
    fprintf(VarTerm, " Negotiation(trigger): MYADDR\n");

  fprintf(VarTerm, "\n");
  throughput_disp(&throughput, VarTerm);

  return 0;
}

void
IpcpDefAddress()
{
  struct hostent *hp;
  char name[200];

  memset(&DefMyAddress, '\0', sizeof(DefMyAddress));
  memset(&DefHisAddress, '\0', sizeof(DefHisAddress));
  TriggerAddress.s_addr = 0;
  HaveTriggerAddress = 0;
  if (gethostname(name, sizeof(name)) == 0) {
    hp = gethostbyname(name);
    if (hp && hp->h_addrtype == AF_INET) {
      memcpy(&DefMyAddress.ipaddr.s_addr, hp->h_addr, hp->h_length);
    }
  }
}

void
IpcpInit()
{
  struct ipcpstate *icp = &IpcpInfo;

  FsmInit(&IpcpFsm);
  memset(icp, '\0', sizeof(struct ipcpstate));
  if ((mode & MODE_DEDICATED) && !GetLabel()) {
    icp->want_ipaddr.s_addr = icp->his_ipaddr.s_addr = 0;
  } else {
    icp->want_ipaddr.s_addr = DefMyAddress.ipaddr.s_addr;
    icp->his_ipaddr.s_addr = DefHisAddress.ipaddr.s_addr;
  }

  /*
   * Some implementations of PPP require that we send a
   * *special* value as our address, even though the rfc specifies
   * full negotiation (e.g. "0.0.0.0" or Not "0.0.0.0").
   */
  if (HaveTriggerAddress) {
    icp->want_ipaddr.s_addr = TriggerAddress.s_addr;
    LogPrintf(LogIPCP, "Using trigger address %s\n", inet_ntoa(TriggerAddress));
  }
  if (Enabled(ConfVjcomp))
    icp->want_compproto = (PROTO_VJCOMP << 16) | ((MAX_STATES - 1) << 8) | 1;
  else
    icp->want_compproto = 0;
  icp->heis1172 = 0;
  IpcpFsm.maxconfig = 10;
  throughput_init(&throughput);
}

static void
IpcpInitRestartCounter(struct fsm * fp)
{
  fp->FsmTimer.load = VarRetryTimeout * SECTICKS;
  fp->restart = 5;
}

static void
IpcpSendConfigReq(struct fsm * fp)
{
  u_char *cp;
  struct ipcpstate *icp = &IpcpInfo;

  cp = ReqBuff;
  LogPrintf(LogIPCP, "IpcpSendConfigReq\n");
  if (!DEV_IS_SYNC || !REJECTED(icp, TY_IPADDR))
    PutConfValue(LogIPCP, &cp, cftypes, TY_IPADDR, 6,
		 ntohl(icp->want_ipaddr.s_addr));
  if (icp->want_compproto && !REJECTED(icp, TY_COMPPROTO)) {
    if (icp->heis1172)
      PutConfValue(LogIPCP, &cp, cftypes, TY_COMPPROTO, 4,
		   icp->want_compproto >> 16);
    else
      PutConfValue(LogIPCP, &cp, cftypes, TY_COMPPROTO, 6, icp->want_compproto);
  }
  FsmOutput(fp, CODE_CONFIGREQ, fp->reqid++, ReqBuff, cp - ReqBuff);
}

static void
IpcpSendTerminateReq(struct fsm * fp)
{
  /* XXX: No code yet */
}

static void
IpcpSendTerminateAck(struct fsm * fp)
{
  LogPrintf(LogIPCP, "IpcpSendTerminateAck\n");
  FsmOutput(fp, CODE_TERMACK, fp->reqid++, NULL, 0);
}

static void
IpcpLayerStart(struct fsm * fp)
{
  LogPrintf(LogIPCP, "IpcpLayerStart.\n");
}

static void
IpcpLayerFinish(struct fsm * fp)
{
  LogPrintf(LogIPCP, "IpcpLayerFinish.\n");
  reconnect(RECON_FALSE);
  LcpClose();
  NewPhase(PHASE_TERMINATE);
}

static void
IpcpLayerDown(struct fsm * fp)
{
  LogPrintf(LogIPCP, "IpcpLayerDown.\n");
  throughput_stop(&throughput);
  throughput_log(&throughput, LogIPCP, NULL);
}

/*
 *  Called when IPCP has reached to OPEN state
 */
static void
IpcpLayerUp(struct fsm * fp)
{
  char tbuff[100];

  Prompt();
  LogPrintf(LogIPCP, "IpcpLayerUp(%d).\n", fp->state);
  snprintf(tbuff, sizeof(tbuff), "myaddr = %s ",
	   inet_ntoa(IpcpInfo.want_ipaddr));

  if (IpcpInfo.his_compproto >> 16 == PROTO_VJCOMP)
    VjInit((IpcpInfo.his_compproto >> 8) & 255);

  LogPrintf(LogIsKept(LogIPCP) ? LogIPCP : LogLINK, " %s hisaddr = %s\n",
	    tbuff, inet_ntoa(IpcpInfo.his_ipaddr));
  if (OsSetIpaddress(IpcpInfo.want_ipaddr, IpcpInfo.his_ipaddr, ifnetmask) < 0) {
    if (VarTerm)
      LogPrintf(LogERROR, "IpcpLayerUp: unable to set ip address\n");
    return;
  }
#ifndef NOALIAS
  if (mode & MODE_ALIAS)
    VarPacketAliasSetAddress(IpcpInfo.want_ipaddr);
#endif
  OsLinkup();
  throughput_start(&throughput);
  StartIdleTimer();
}

void
IpcpUp()
{
  FsmUp(&IpcpFsm);
  LogPrintf(LogIPCP, "IPCP Up event!!\n");
}

void
IpcpOpen()
{
  FsmOpen(&IpcpFsm);
}

static int
AcceptableAddr(struct in_range * prange, struct in_addr ipaddr)
{
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
      snprintf(tbuff, sizeof(tbuff), " %s[%d] ", cftypes[type], length);
    else if (type > 128 && type < 128 + NCFTYPES128)
      snprintf(tbuff, sizeof(tbuff), " %s[%d] ", cftypes128[type], length);
    else
      snprintf(tbuff, sizeof(tbuff), " ??? ");

    switch (type) {
    case TY_IPADDR:		/* RFC1332 */
      lp = (u_long *) (cp + 2);
      ipaddr.s_addr = *lp;
      LogPrintf(LogIPCP, "%s %s\n", tbuff, inet_ntoa(ipaddr));

      switch (mode_type) {
      case MODE_REQ:
	if (!AcceptableAddr(&DefHisAddress, ipaddr)) {
	  /*
	   * If destination address is not acceptable, insist to use what we
	   * want to use.
	   */
	  memcpy(nakp, cp, 2);
	  memcpy(nakp+2, &IpcpInfo.his_ipaddr.s_addr, length);
	  nakp += length;
	  break;
	}
	IpcpInfo.his_ipaddr = ipaddr;
	memcpy(ackp, cp, length);
	ackp += length;
	break;
      case MODE_NAK:
	if (AcceptableAddr(&DefMyAddress, ipaddr)) {

	  /*
	   * Use address suggested by peer.
	   */
	  snprintf(tbuff2, sizeof(tbuff2), "%s changing address: %s ", tbuff,
		   inet_ntoa(IpcpInfo.want_ipaddr));
	  LogPrintf(LogIPCP, "%s --> %s\n", tbuff2, inet_ntoa(ipaddr));
	  IpcpInfo.want_ipaddr = ipaddr;
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
      LogPrintf(LogIPCP, "%s %08x\n", tbuff, compproto);

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
		&& pcomp->slots < MAX_STATES && pcomp->slots > 2) {
	      IpcpInfo.his_compproto = compproto;
	      IpcpInfo.heis1172 = 0;
	      memcpy(ackp, cp, length);
	      ackp += length;
	    } else {
	      memcpy(nakp, cp, 2);
	      pcomp->proto = htons(PROTO_VJCOMP);
	      pcomp->slots = MAX_STATES - 1;
	      pcomp->compcid = 0;
	      memcpy(nakp+2, &pcomp, sizeof(pcomp));
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
      snprintf(tbuff2, sizeof(tbuff2), "%s %s,", tbuff, inet_ntoa(ipaddr));
      LogPrintf(LogIPCP, "%s %s\n", tbuff2, inet_ntoa(dstipaddr));

      switch (mode_type) {
      case MODE_REQ:
	IpcpInfo.his_ipaddr = ipaddr;
	IpcpInfo.want_ipaddr = dstipaddr;
	memcpy(ackp, cp, length);
	ackp += length;
	break;
      case MODE_NAK:
        snprintf(tbuff2, sizeof(tbuff2), "%s changing address: %s", tbuff,
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
	ms_info_req.s_addr = ns_entries[((type - TY_PRIMARY_DNS) ? 1 : 0)].s_addr;
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
	ms_info_req.s_addr = nbns_entries[((type - TY_PRIMARY_NBNS) ? 1 : 0)].s_addr;
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
  FsmInput(&IpcpFsm, bp);
}

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
 * $FreeBSD$
 *
 *	TODO:
 *		o Support IPADDRS properly
 *		o Validate the length in IpcpDecodeConfig
 */
#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netdb.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifndef NONAT
#ifdef __FreeBSD__
#include <alias.h>
#else
#include "alias.h"
#endif
#endif
#include "layer.h"
#include "ua.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "proto.h"
#include "lcp.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "lqr.h"
#include "hdlc.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "vjcomp.h"
#include "async.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "id.h"
#include "arp.h"
#include "systems.h"
#include "prompt.h"
#include "route.h"
#include "iface.h"
#include "ip.h"

#undef REJECTED
#define	REJECTED(p, x)	((p)->peer_reject & (1<<(x)))
#define issep(ch) ((ch) == ' ' || (ch) == '\t')
#define isip(ch) (((ch) >= '0' && (ch) <= '9') || (ch) == '.')

struct compreq {
  u_short proto;
  u_char slots;
  u_char compcid;
};

static int IpcpLayerUp(struct fsm *);
static void IpcpLayerDown(struct fsm *);
static void IpcpLayerStart(struct fsm *);
static void IpcpLayerFinish(struct fsm *);
static void IpcpInitRestartCounter(struct fsm *, int);
static void IpcpSendConfigReq(struct fsm *);
static void IpcpSentTerminateReq(struct fsm *);
static void IpcpSendTerminateAck(struct fsm *, u_char);
static void IpcpDecodeConfig(struct fsm *, u_char *, int, int,
                             struct fsm_decode *);

static struct fsm_callbacks ipcp_Callbacks = {
  IpcpLayerUp,
  IpcpLayerDown,
  IpcpLayerStart,
  IpcpLayerFinish,
  IpcpInitRestartCounter,
  IpcpSendConfigReq,
  IpcpSentTerminateReq,
  IpcpSendTerminateAck,
  IpcpDecodeConfig,
  fsm_NullRecvResetReq,
  fsm_NullRecvResetAck
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
ipcp_AddInOctets(struct ipcp *ipcp, int n)
{
  throughput_addin(&ipcp->throughput, n);
}

void
ipcp_AddOutOctets(struct ipcp *ipcp, int n)
{
  throughput_addout(&ipcp->throughput, n);
}

static void
getdns(struct ipcp *ipcp, struct in_addr addr[2])
{
  FILE *fp;

  addr[0].s_addr = addr[1].s_addr = INADDR_ANY;
  if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
    char buf[LINE_LEN], *cp, *end;
    int n;

    n = 0;
    buf[sizeof buf - 1] = '\0';
    while (fgets(buf, sizeof buf - 1, fp)) {
      if (!strncmp(buf, "nameserver", 10) && issep(buf[10])) {
        for (cp = buf + 11; issep(*cp); cp++)
          ;
        for (end = cp; isip(*end); end++)
          ;
        *end = '\0';
        if (inet_aton(cp, addr+n) && ++n == 2)
          break;
      }
    }
    if (n == 1)
      addr[1] = addr[0];
    fclose(fp);
  }
}

static int
setdns(struct ipcp *ipcp, struct in_addr addr[2])
{
  FILE *fp;
  char wbuf[LINE_LEN + 54];
  int wlen;

  if (addr[0].s_addr == INADDR_ANY || addr[1].s_addr == INADDR_ANY) {
    struct in_addr old[2];

    getdns(ipcp, old);
    if (addr[0].s_addr == INADDR_ANY)
      addr[0] = old[0];
    if (addr[1].s_addr == INADDR_ANY)
      addr[1] = old[1];
  }

  if (addr[0].s_addr == INADDR_ANY && addr[1].s_addr == INADDR_ANY) {
    log_Printf(LogWARN, "%s not modified: All nameservers NAKd\n",
              _PATH_RESCONF);
    return 0;
  }

  wlen = 0;
  if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
    char buf[LINE_LEN];
    int len;

    buf[sizeof buf - 1] = '\0';
    while (fgets(buf, sizeof buf - 1, fp)) {
      if (strncmp(buf, "nameserver", 10) || !issep(buf[10])) {
        len = strlen(buf);
        if (len > sizeof wbuf - wlen) {
          log_Printf(LogWARN, "%s: Can only cope with max file size %d\n",
                    _PATH_RESCONF, LINE_LEN);
          fclose(fp);
          return 0;
        }
        memcpy(wbuf + wlen, buf, len);
        wlen += len;
      }
    }
    fclose(fp);
  }

  if (addr[0].s_addr != INADDR_ANY) {
    snprintf(wbuf + wlen, sizeof wbuf - wlen, "nameserver %s\n",
             inet_ntoa(addr[0]));
    log_Printf(LogIPCP, "Primary nameserver set to %s", wbuf + wlen + 11);
    wlen += strlen(wbuf + wlen);
  }

  if (addr[1].s_addr != INADDR_ANY && addr[1].s_addr != addr[0].s_addr) {
    snprintf(wbuf + wlen, sizeof wbuf - wlen, "nameserver %s\n",
             inet_ntoa(addr[1]));
    log_Printf(LogIPCP, "Secondary nameserver set to %s", wbuf + wlen + 11);
    wlen += strlen(wbuf + wlen);
  }

  if (wlen) {
    int fd;

    if ((fd = ID0open(_PATH_RESCONF, O_WRONLY|O_CREAT, 0644)) != -1) {
      if (write(fd, wbuf, wlen) != wlen) {
        log_Printf(LogERROR, "setdns: write(): %s\n", strerror(errno));
        close(fd);
        return 0;
      }
      if (ftruncate(fd, wlen) == -1) {
        log_Printf(LogERROR, "setdns: truncate(): %s\n", strerror(errno));
        close(fd);
        return 0;
      }
      close(fd);
    } else {
      log_Printf(LogERROR, "setdns: open(): %s\n", strerror(errno));
      return 0;
    }
  }

  return 1;
}

int
ipcp_Show(struct cmdargs const *arg)
{
  struct ipcp *ipcp = &arg->bundle->ncp.ipcp;

  prompt_Printf(arg->prompt, "%s [%s]\n", ipcp->fsm.name,
                State2Nam(ipcp->fsm.state));
  if (ipcp->fsm.state == ST_OPENED) {
    prompt_Printf(arg->prompt, " His side:        %s, %s\n",
	          inet_ntoa(ipcp->peer_ip), vj2asc(ipcp->peer_compproto));
    prompt_Printf(arg->prompt, " My side:         %s, %s\n",
	          inet_ntoa(ipcp->my_ip), vj2asc(ipcp->my_compproto));
    prompt_Printf(arg->prompt, " Queued packets:  %d\n", ip_QueueLen(ipcp));
  }

  if (ipcp->route) {
    prompt_Printf(arg->prompt, "\n");
    route_ShowSticky(arg->prompt, ipcp->route, "Sticky routes", 1);
  }

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  prompt_Printf(arg->prompt, " FSM retry = %us, max %u Config"
                " REQ%s, %u Term REQ%s\n", ipcp->cfg.fsm.timeout,
                ipcp->cfg.fsm.maxreq, ipcp->cfg.fsm.maxreq == 1 ? "" : "s",
                ipcp->cfg.fsm.maxtrm, ipcp->cfg.fsm.maxtrm == 1 ? "" : "s");
  prompt_Printf(arg->prompt, " My Address:      %s/%d",
	        inet_ntoa(ipcp->cfg.my_range.ipaddr), ipcp->cfg.my_range.width);
  prompt_Printf(arg->prompt, ", netmask %s\n", inet_ntoa(ipcp->cfg.netmask));
  if (ipcp->cfg.HaveTriggerAddress)
    prompt_Printf(arg->prompt, " Trigger address: %s\n",
                  inet_ntoa(ipcp->cfg.TriggerAddress));

  prompt_Printf(arg->prompt, " VJ compression:  %s (%d slots %s slot "
                "compression)\n", command_ShowNegval(ipcp->cfg.vj.neg),
                ipcp->cfg.vj.slots, ipcp->cfg.vj.slotcomp ? "with" : "without");

  if (iplist_isvalid(&ipcp->cfg.peer_list))
    prompt_Printf(arg->prompt, " His Address:     %s\n",
                  ipcp->cfg.peer_list.src);
  else
    prompt_Printf(arg->prompt, " His Address:     %s/%d\n",
	          inet_ntoa(ipcp->cfg.peer_range.ipaddr),
                  ipcp->cfg.peer_range.width);

  prompt_Printf(arg->prompt, " DNS:             %s, ",
                inet_ntoa(ipcp->cfg.ns.dns[0]));
  prompt_Printf(arg->prompt, "%s, %s\n", inet_ntoa(ipcp->cfg.ns.dns[1]),
                command_ShowNegval(ipcp->cfg.ns.dns_neg));
  prompt_Printf(arg->prompt, " NetBIOS NS:      %s, ",
	        inet_ntoa(ipcp->cfg.ns.nbns[0]));
  prompt_Printf(arg->prompt, "%s\n", inet_ntoa(ipcp->cfg.ns.nbns[1]));

  prompt_Printf(arg->prompt, "\n");
  throughput_disp(&ipcp->throughput, arg->prompt);

  return 0;
}

int
ipcp_vjset(struct cmdargs const *arg)
{
  if (arg->argc != arg->argn+2)
    return -1;
  if (!strcasecmp(arg->argv[arg->argn], "slots")) {
    int slots;

    slots = atoi(arg->argv[arg->argn+1]);
    if (slots < 4 || slots > 16)
      return 1;
    arg->bundle->ncp.ipcp.cfg.vj.slots = slots;
    return 0;
  } else if (!strcasecmp(arg->argv[arg->argn], "slotcomp")) {
    if (!strcasecmp(arg->argv[arg->argn+1], "on"))
      arg->bundle->ncp.ipcp.cfg.vj.slotcomp = 1;
    else if (!strcasecmp(arg->argv[arg->argn+1], "off"))
      arg->bundle->ncp.ipcp.cfg.vj.slotcomp = 0;
    else
      return 2;
    return 0;
  }
  return -1;
}

void
ipcp_Init(struct ipcp *ipcp, struct bundle *bundle, struct link *l,
          const struct fsm_parent *parent)
{
  struct hostent *hp;
  char name[MAXHOSTNAMELEN];
  static const char *timer_names[] =
    {"IPCP restart", "IPCP openmode", "IPCP stopped"};

  fsm_Init(&ipcp->fsm, "IPCP", PROTO_IPCP, 1, IPCP_MAXCODE, LogIPCP,
           bundle, l, parent, &ipcp_Callbacks, timer_names);

  ipcp->route = NULL;
  ipcp->cfg.vj.slots = DEF_VJ_STATES;
  ipcp->cfg.vj.slotcomp = 1;
  memset(&ipcp->cfg.my_range, '\0', sizeof ipcp->cfg.my_range);
  if (gethostname(name, sizeof name) == 0) {
    hp = gethostbyname(name);
    if (hp && hp->h_addrtype == AF_INET)
      memcpy(&ipcp->cfg.my_range.ipaddr.s_addr, hp->h_addr, hp->h_length);
  }
  ipcp->cfg.netmask.s_addr = INADDR_ANY;
  memset(&ipcp->cfg.peer_range, '\0', sizeof ipcp->cfg.peer_range);
  iplist_setsrc(&ipcp->cfg.peer_list, "");
  ipcp->cfg.HaveTriggerAddress = 0;

  ipcp->cfg.ns.dns[0].s_addr = INADDR_ANY;
  ipcp->cfg.ns.dns[1].s_addr = INADDR_ANY;
  ipcp->cfg.ns.dns_neg = 0;
  ipcp->cfg.ns.nbns[0].s_addr = INADDR_ANY;
  ipcp->cfg.ns.nbns[1].s_addr = INADDR_ANY;

  ipcp->cfg.fsm.timeout = DEF_FSMRETRY;
  ipcp->cfg.fsm.maxreq = DEF_FSMTRIES;
  ipcp->cfg.fsm.maxtrm = DEF_FSMTRIES;
  ipcp->cfg.vj.neg = NEG_ENABLED|NEG_ACCEPTED;

  memset(&ipcp->vj, '\0', sizeof ipcp->vj);

  throughput_init(&ipcp->throughput, SAMPLE_PERIOD);
  memset(ipcp->Queue, '\0', sizeof ipcp->Queue);
  ipcp_Setup(ipcp, INADDR_NONE);
}

void
ipcp_SetLink(struct ipcp *ipcp, struct link *l)
{
  ipcp->fsm.link = l;
}

void
ipcp_Setup(struct ipcp *ipcp, u_int32_t mask)
{
  struct iface *iface = ipcp->fsm.bundle->iface;
  int pos, n;

  ipcp->fsm.open_mode = 0;
  ipcp->ifmask.s_addr = mask == INADDR_NONE ? ipcp->cfg.netmask.s_addr : mask;

  if (iplist_isvalid(&ipcp->cfg.peer_list)) {
    /* Try to give the peer a previously configured IP address */
    for (n = 0; n < iface->in_addrs; n++) {
      pos = iplist_ip2pos(&ipcp->cfg.peer_list, iface->in_addr[n].brd);
      if (pos != -1) {
        ipcp->cfg.peer_range.ipaddr =
          iplist_setcurpos(&ipcp->cfg.peer_list, pos);
        break;
      }
    }
    if (n == iface->in_addrs)
      /* Ok, so none of 'em fit.... pick a random one */
      ipcp->cfg.peer_range.ipaddr = iplist_setrandpos(&ipcp->cfg.peer_list);

    ipcp->cfg.peer_range.mask.s_addr = INADDR_BROADCAST;
    ipcp->cfg.peer_range.width = 32;
  }

  ipcp->heis1172 = 0;

  ipcp->peer_ip = ipcp->cfg.peer_range.ipaddr;
  ipcp->peer_compproto = 0;

  if (ipcp->cfg.HaveTriggerAddress) {
    /*
     * Some implementations of PPP require that we send a
     * *special* value as our address, even though the rfc specifies
     * full negotiation (e.g. "0.0.0.0" or Not "0.0.0.0").
     */
    ipcp->my_ip = ipcp->cfg.TriggerAddress;
    log_Printf(LogIPCP, "Using trigger address %s\n",
              inet_ntoa(ipcp->cfg.TriggerAddress));
  } else {
    /*
     * Otherwise, if we've used an IP number before and it's still within
     * the network specified on the ``set ifaddr'' line, we really
     * want to keep that IP number so that we can keep any existing
     * connections that are bound to that IP (assuming we're not
     * ``iface-alias''ing).
     */
    for (n = 0; n < iface->in_addrs; n++)
      if ((iface->in_addr[n].ifa.s_addr & ipcp->cfg.my_range.mask.s_addr) ==
          (ipcp->cfg.my_range.ipaddr.s_addr & ipcp->cfg.my_range.mask.s_addr)) {
        ipcp->my_ip = iface->in_addr[n].ifa;
        break;
      }
    if (n == iface->in_addrs)
      ipcp->my_ip = ipcp->cfg.my_range.ipaddr;
  }

  if (IsEnabled(ipcp->cfg.vj.neg)
#ifndef NORADIUS
      || (ipcp->fsm.bundle->radius.valid && ipcp->fsm.bundle->radius.vj)
#endif
     )
    ipcp->my_compproto = (PROTO_VJCOMP << 16) +
                         ((ipcp->cfg.vj.slots - 1) << 8) +
                         ipcp->cfg.vj.slotcomp;
  else
    ipcp->my_compproto = 0;
  sl_compress_init(&ipcp->vj.cslc, ipcp->cfg.vj.slots - 1);

  ipcp->peer_reject = 0;
  ipcp->my_reject = 0;
}

static int
ipcp_doproxyall(struct bundle *bundle,
                int (*proxyfun)(struct bundle *, struct in_addr, int), int s)
{
  int n, ret;
  struct sticky_route *rp;
  struct in_addr addr;
  struct ipcp *ipcp;

  ipcp = &bundle->ncp.ipcp;
  for (rp = ipcp->route; rp != NULL; rp = rp->next) {
    if (rp->mask.s_addr == INADDR_BROADCAST)
        continue;
    n = ntohl(INADDR_BROADCAST) - ntohl(rp->mask.s_addr) - 1;
    if (n > 0 && n <= 254 && rp->dst.s_addr != INADDR_ANY) {
      addr = rp->dst;
      while (n--) {
        addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
	log_Printf(LogDEBUG, "ipcp_doproxyall: %s\n", inet_ntoa(addr));
	ret = (*proxyfun)(bundle, addr, s);
	if (!ret)
	  return ret;
      }
    }
  }

  return 0;
}

static int
ipcp_SetIPaddress(struct bundle *bundle, struct in_addr myaddr,
                  struct in_addr hisaddr, int silent)
{
  struct in_addr mask, oaddr, none = { INADDR_ANY };

  mask = addr2mask(myaddr);

  if (bundle->ncp.ipcp.ifmask.s_addr != INADDR_ANY &&
      (bundle->ncp.ipcp.ifmask.s_addr & mask.s_addr) == mask.s_addr)
    mask.s_addr = bundle->ncp.ipcp.ifmask.s_addr;

  oaddr.s_addr = bundle->iface->in_addrs ?
                 bundle->iface->in_addr[0].ifa.s_addr : INADDR_ANY;
  if (!iface_inAdd(bundle->iface, myaddr, mask, hisaddr,
                 IFACE_ADD_FIRST|IFACE_FORCE_ADD))
    return -1;

  if (!Enabled(bundle, OPT_IFACEALIAS) && bundle->iface->in_addrs > 1
      && myaddr.s_addr != oaddr.s_addr)
    /* Nuke the old one */
    iface_inDelete(bundle->iface, oaddr);

  if (bundle->ncp.ipcp.cfg.sendpipe > 0 || bundle->ncp.ipcp.cfg.recvpipe > 0)
    bundle_SetRoute(bundle, RTM_CHANGE, hisaddr, myaddr, none, 0, 0);

  if (Enabled(bundle, OPT_SROUTES))
    route_Change(bundle, bundle->ncp.ipcp.route, myaddr, hisaddr);

#ifndef NORADIUS
  if (bundle->radius.valid)
    route_Change(bundle, bundle->radius.routes, myaddr, hisaddr);
#endif

  if (Enabled(bundle, OPT_PROXY) || Enabled(bundle, OPT_PROXYALL)) {
    int s = ID0socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
      log_Printf(LogERROR, "ipcp_SetIPaddress: socket(): %s\n",
                 strerror(errno));
    else {
      if (Enabled(bundle, OPT_PROXYALL))
        ipcp_doproxyall(bundle, arp_SetProxy, s);
      else if (Enabled(bundle, OPT_PROXY))
        arp_SetProxy(bundle, hisaddr, s);
      close(s);
    }
  }

  return 0;
}

static struct in_addr
ChooseHisAddr(struct bundle *bundle, struct in_addr gw)
{
  struct in_addr try;
  u_long f;

  for (f = 0; f < bundle->ncp.ipcp.cfg.peer_list.nItems; f++) {
    try = iplist_next(&bundle->ncp.ipcp.cfg.peer_list);
    log_Printf(LogDEBUG, "ChooseHisAddr: Check item %ld (%s)\n",
              f, inet_ntoa(try));
    if (ipcp_SetIPaddress(bundle, gw, try, 1) == 0) {
      log_Printf(LogIPCP, "Selected IP address %s\n", inet_ntoa(try));
      break;
    }
  }

  if (f == bundle->ncp.ipcp.cfg.peer_list.nItems) {
    log_Printf(LogDEBUG, "ChooseHisAddr: All addresses in use !\n");
    try.s_addr = INADDR_ANY;
  }

  return try;
}

static void
IpcpInitRestartCounter(struct fsm *fp, int what)
{
  /* Set fsm timer load */
  struct ipcp *ipcp = fsm2ipcp(fp);

  fp->FsmTimer.load = ipcp->cfg.fsm.timeout * SECTICKS;
  switch (what) {
    case FSM_REQ_TIMER:
      fp->restart = ipcp->cfg.fsm.maxreq;
      break;
    case FSM_TRM_TIMER:
      fp->restart = ipcp->cfg.fsm.maxtrm;
      break;
    default:
      fp->restart = 1;
      break;
  }
}

static void
IpcpSendConfigReq(struct fsm *fp)
{
  /* Send config REQ please */
  struct physical *p = link2physical(fp->link);
  struct ipcp *ipcp = fsm2ipcp(fp);
  u_char buff[24];
  struct lcp_opt *o;

  o = (struct lcp_opt *)buff;

  if ((p && !physical_IsSync(p)) || !REJECTED(ipcp, TY_IPADDR)) {
    memcpy(o->data, &ipcp->my_ip.s_addr, 4);
    INC_LCP_OPT(TY_IPADDR, 6, o);
  }

  if (ipcp->my_compproto && !REJECTED(ipcp, TY_COMPPROTO)) {
    if (ipcp->heis1172) {
      u_int16_t proto = PROTO_VJCOMP;

      ua_htons(&proto, o->data);
      INC_LCP_OPT(TY_COMPPROTO, 4, o);
    } else {
      struct compreq req;

      req.proto = htons(ipcp->my_compproto >> 16);
      req.slots = (ipcp->my_compproto >> 8) & 255;
      req.compcid = ipcp->my_compproto & 1;
      memcpy(o->data, &req, 4);
      INC_LCP_OPT(TY_COMPPROTO, 6, o);
    }
  }

  if (IsEnabled(ipcp->cfg.ns.dns_neg) &&
      !REJECTED(ipcp, TY_PRIMARY_DNS - TY_ADJUST_NS) &&
      !REJECTED(ipcp, TY_SECONDARY_DNS - TY_ADJUST_NS)) {
    struct in_addr dns[2];
    getdns(ipcp, dns);
    memcpy(o->data, &dns[0].s_addr, 4);
    INC_LCP_OPT(TY_PRIMARY_DNS, 6, o);
    memcpy(o->data, &dns[1].s_addr, 4);
    INC_LCP_OPT(TY_SECONDARY_DNS, 6, o);
  }

  fsm_Output(fp, CODE_CONFIGREQ, fp->reqid, buff, (u_char *)o - buff,
             MB_IPCPOUT);
}

static void
IpcpSentTerminateReq(struct fsm *fp)
{
  /* Term REQ just sent by FSM */
}

static void
IpcpSendTerminateAck(struct fsm *fp, u_char id)
{
  /* Send Term ACK please */
  fsm_Output(fp, CODE_TERMACK, id, NULL, 0, MB_IPCPOUT);
}

static void
IpcpLayerStart(struct fsm *fp)
{
  /* We're about to start up ! */
  struct ipcp *ipcp = fsm2ipcp(fp);

  log_Printf(LogIPCP, "%s: LayerStart.\n", fp->link->name);
  throughput_start(&ipcp->throughput, "IPCP throughput",
                   Enabled(fp->bundle, OPT_THROUGHPUT));
  fp->more.reqs = fp->more.naks = fp->more.rejs = ipcp->cfg.fsm.maxreq * 3;
}

static void
IpcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  struct ipcp *ipcp = fsm2ipcp(fp);

  log_Printf(LogIPCP, "%s: LayerFinish.\n", fp->link->name);
  throughput_stop(&ipcp->throughput);
  throughput_log(&ipcp->throughput, LogIPCP, NULL);
}

void
ipcp_CleanInterface(struct ipcp *ipcp)
{
  struct iface *iface = ipcp->fsm.bundle->iface;

  route_Clean(ipcp->fsm.bundle, ipcp->route);

  if (iface->in_addrs && (Enabled(ipcp->fsm.bundle, OPT_PROXY) ||
                          Enabled(ipcp->fsm.bundle, OPT_PROXYALL))) {
    int s = ID0socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
      log_Printf(LogERROR, "ipcp_CleanInterface: socket: %s\n",
                 strerror(errno));
    else {
      if (Enabled(ipcp->fsm.bundle, OPT_PROXYALL))
        ipcp_doproxyall(ipcp->fsm.bundle, arp_ClearProxy, s);
      else if (Enabled(ipcp->fsm.bundle, OPT_PROXY))
        arp_ClearProxy(ipcp->fsm.bundle, iface->in_addr[0].brd, s);
      close(s);
    }
  }

  iface_inClear(ipcp->fsm.bundle->iface, IFACE_CLEAR_ALL);
}

static void
IpcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  static int recursing;
  struct ipcp *ipcp = fsm2ipcp(fp);
  const char *s;

  if (!recursing++) {
    if (ipcp->fsm.bundle->iface->in_addrs)
      s = inet_ntoa(ipcp->fsm.bundle->iface->in_addr[0].ifa);
    else
      s = "Interface configuration error !";
    log_Printf(LogIPCP, "%s: LayerDown: %s\n", fp->link->name, s);

    /*
     * XXX this stuff should really live in the FSM.  Our config should
     * associate executable sections in files with events.
     */
    if (system_Select(fp->bundle, s, LINKDOWNFILE, NULL, NULL) < 0) {
      if (bundle_GetLabel(fp->bundle)) {
         if (system_Select(fp->bundle, bundle_GetLabel(fp->bundle),
                          LINKDOWNFILE, NULL, NULL) < 0)
         system_Select(fp->bundle, "MYADDR", LINKDOWNFILE, NULL, NULL);
      } else
        system_Select(fp->bundle, "MYADDR", LINKDOWNFILE, NULL, NULL);
    }

    ipcp_Setup(ipcp, INADDR_NONE);
  }
  recursing--;
}

int
ipcp_InterfaceUp(struct ipcp *ipcp)
{
  if (ipcp_SetIPaddress(ipcp->fsm.bundle, ipcp->my_ip, ipcp->peer_ip, 0) < 0) {
    log_Printf(LogERROR, "ipcp_InterfaceUp: unable to set ip address\n");
    return 0;
  }

#ifndef NONAT
  if (ipcp->fsm.bundle->NatEnabled)
    PacketAliasSetAddress(ipcp->my_ip);
#endif

  return 1;
}

static int
IpcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct ipcp *ipcp = fsm2ipcp(fp);
  char tbuff[16];

  log_Printf(LogIPCP, "%s: LayerUp.\n", fp->link->name);
  snprintf(tbuff, sizeof tbuff, "%s", inet_ntoa(ipcp->my_ip));
  log_Printf(LogIPCP, "myaddr %s hisaddr = %s\n",
             tbuff, inet_ntoa(ipcp->peer_ip));

  if (ipcp->peer_compproto >> 16 == PROTO_VJCOMP)
    sl_compress_init(&ipcp->vj.cslc, (ipcp->peer_compproto >> 8) & 255);

  if (!ipcp_InterfaceUp(ipcp))
    return 0;

  /*
   * XXX this stuff should really live in the FSM.  Our config should
   * associate executable sections in files with events.
   */
  if (system_Select(fp->bundle, tbuff, LINKUPFILE, NULL, NULL) < 0) {
    if (bundle_GetLabel(fp->bundle)) {
      if (system_Select(fp->bundle, bundle_GetLabel(fp->bundle),
                       LINKUPFILE, NULL, NULL) < 0)
        system_Select(fp->bundle, "MYADDR", LINKUPFILE, NULL, NULL);
    } else
      system_Select(fp->bundle, "MYADDR", LINKUPFILE, NULL, NULL);
  }

  fp->more.reqs = fp->more.naks = fp->more.rejs = ipcp->cfg.fsm.maxreq * 3;
  log_DisplayPrompts();

  return 1;
}

static int
AcceptableAddr(const struct in_range *prange, struct in_addr ipaddr)
{
  /* Is the given IP in the given range ? */
  return (prange->ipaddr.s_addr & prange->mask.s_addr) ==
    (ipaddr.s_addr & prange->mask.s_addr) && ipaddr.s_addr;
}

static void
IpcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type,
                 struct fsm_decode *dec)
{
  /* Deal with incoming PROTO_IPCP */
  struct iface *iface = fp->bundle->iface;
  struct ipcp *ipcp = fsm2ipcp(fp);
  int type, length, gotdns, gotdnsnak, n;
  u_int32_t compproto;
  struct compreq *pcomp;
  struct in_addr ipaddr, dstipaddr, have_ip, dns[2], dnsnak[2];
  char tbuff[100], tbuff2[100];

  gotdns = 0;
  gotdnsnak = 0;
  dnsnak[0].s_addr = dnsnak[1].s_addr = INADDR_ANY;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];

    if (length == 0) {
      log_Printf(LogIPCP, "%s: IPCP size zero\n", fp->link->name);
      break;
    }

    if (type < NCFTYPES)
      snprintf(tbuff, sizeof tbuff, " %s[%d] ", cftypes[type], length);
    else if (type > 128 && type < 128 + NCFTYPES128)
      snprintf(tbuff, sizeof tbuff, " %s[%d] ", cftypes128[type-128], length);
    else
      snprintf(tbuff, sizeof tbuff, " <%d>[%d] ", type, length);

    switch (type) {
    case TY_IPADDR:		/* RFC1332 */
      memcpy(&ipaddr.s_addr, cp + 2, 4);
      log_Printf(LogIPCP, "%s %s\n", tbuff, inet_ntoa(ipaddr));

      switch (mode_type) {
      case MODE_REQ:
        if (iplist_isvalid(&ipcp->cfg.peer_list)) {
          if (ipaddr.s_addr == INADDR_ANY ||
              iplist_ip2pos(&ipcp->cfg.peer_list, ipaddr) < 0 ||
              ipcp_SetIPaddress(fp->bundle, ipcp->cfg.my_range.ipaddr,
                                ipaddr, 1)) {
            log_Printf(LogIPCP, "%s: Address invalid or already in use\n",
                      inet_ntoa(ipaddr));
            /*
             * If we've already had a valid address configured for the peer,
             * try NAKing with that so that we don't have to upset things
             * too much.
             */
            for (n = 0; n < iface->in_addrs; n++)
              if (iplist_ip2pos(&ipcp->cfg.peer_list, iface->in_addr[n].brd)
                  >=0) {
                ipcp->peer_ip = iface->in_addr[n].brd;
                break;
              }

            if (n == iface->in_addrs)
              /* Just pick an IP number from our list */
              ipcp->peer_ip = ChooseHisAddr
                (fp->bundle, ipcp->cfg.my_range.ipaddr);

            if (ipcp->peer_ip.s_addr == INADDR_ANY) {
	      memcpy(dec->rejend, cp, length);
	      dec->rejend += length;
            } else {
	      memcpy(dec->nakend, cp, 2);
	      memcpy(dec->nakend + 2, &ipcp->peer_ip.s_addr, length - 2);
	      dec->nakend += length;
            }
	    break;
          }
	} else if (!AcceptableAddr(&ipcp->cfg.peer_range, ipaddr)) {
	  /*
	   * If destination address is not acceptable, NAK with what we
	   * want to use.
	   */
	  memcpy(dec->nakend, cp, 2);
          for (n = 0; n < iface->in_addrs; n++)
            if ((iface->in_addr[n].brd.s_addr &
                 ipcp->cfg.peer_range.mask.s_addr)
                == (ipcp->cfg.peer_range.ipaddr.s_addr &
                    ipcp->cfg.peer_range.mask.s_addr)) {
              /* We prefer the already-configured address */
	      memcpy(dec->nakend + 2, &iface->in_addr[n].brd.s_addr,
                     length - 2);
              break;
            }

          if (n == iface->in_addrs)
	    memcpy(dec->nakend + 2, &ipcp->peer_ip.s_addr, length - 2);

	  dec->nakend += length;
	  break;
	}
	ipcp->peer_ip = ipaddr;
	memcpy(dec->ackend, cp, length);
	dec->ackend += length;
	break;

      case MODE_NAK:
	if (AcceptableAddr(&ipcp->cfg.my_range, ipaddr)) {
	  /* Use address suggested by peer */
	  snprintf(tbuff2, sizeof tbuff2, "%s changing address: %s ", tbuff,
		   inet_ntoa(ipcp->my_ip));
	  log_Printf(LogIPCP, "%s --> %s\n", tbuff2, inet_ntoa(ipaddr));
	  ipcp->my_ip = ipaddr;
          bundle_AdjustFilters(fp->bundle, &ipcp->my_ip, NULL);
	} else {
	  log_Printf(log_IsKept(LogIPCP) ? LogIPCP : LogPHASE,
                    "%s: Unacceptable address!\n", inet_ntoa(ipaddr));
          fsm_Close(&ipcp->fsm);
	}
	break;

      case MODE_REJ:
	ipcp->peer_reject |= (1 << type);
	break;
      }
      break;

    case TY_COMPPROTO:
      pcomp = (struct compreq *)(cp + 2);
      compproto = (ntohs(pcomp->proto) << 16) + (pcomp->slots << 8) +
                  pcomp->compcid;
      log_Printf(LogIPCP, "%s %s\n", tbuff, vj2asc(compproto));

      switch (mode_type) {
      case MODE_REQ:
	if (!IsAccepted(ipcp->cfg.vj.neg)) {
	  memcpy(dec->rejend, cp, length);
	  dec->rejend += length;
	} else {
	  switch (length) {
	  case 4:		/* RFC1172 */
	    if (ntohs(pcomp->proto) == PROTO_VJCOMP) {
	      log_Printf(LogWARN, "Peer is speaking RFC1172 compression "
                         "protocol !\n");
	      ipcp->heis1172 = 1;
	      ipcp->peer_compproto = compproto;
	      memcpy(dec->ackend, cp, length);
	      dec->ackend += length;
	    } else {
	      memcpy(dec->nakend, cp, 2);
	      pcomp->proto = htons(PROTO_VJCOMP);
	      memcpy(dec->nakend+2, &pcomp, 2);
	      dec->nakend += length;
	    }
	    break;
	  case 6:		/* RFC1332 */
	    if (ntohs(pcomp->proto) == PROTO_VJCOMP) {
              if (pcomp->slots <= MAX_VJ_STATES
                  && pcomp->slots >= MIN_VJ_STATES) {
                /* Ok, we can do that */
	        ipcp->peer_compproto = compproto;
	        ipcp->heis1172 = 0;
	        memcpy(dec->ackend, cp, length);
	        dec->ackend += length;
	      } else {
                /* Get as close as we can to what he wants */
	        ipcp->heis1172 = 0;
	        memcpy(dec->nakend, cp, 2);
	        pcomp->slots = pcomp->slots < MIN_VJ_STATES ?
                               MIN_VJ_STATES : MAX_VJ_STATES;
	        memcpy(dec->nakend+2, &pcomp, sizeof pcomp);
	        dec->nakend += length;
              }
	    } else {
              /* What we really want */
	      memcpy(dec->nakend, cp, 2);
	      pcomp->proto = htons(PROTO_VJCOMP);
	      pcomp->slots = DEF_VJ_STATES;
	      pcomp->compcid = 1;
	      memcpy(dec->nakend+2, &pcomp, sizeof pcomp);
	      dec->nakend += length;
	    }
	    break;
	  default:
	    memcpy(dec->rejend, cp, length);
	    dec->rejend += length;
	    break;
	  }
	}
	break;

      case MODE_NAK:
	if (ntohs(pcomp->proto) == PROTO_VJCOMP) {
          if (pcomp->slots > MAX_VJ_STATES)
            pcomp->slots = MAX_VJ_STATES;
          else if (pcomp->slots < MIN_VJ_STATES)
            pcomp->slots = MIN_VJ_STATES;
          compproto = (ntohs(pcomp->proto) << 16) + (pcomp->slots << 8) +
                      pcomp->compcid;
        } else
          compproto = 0;
	log_Printf(LogIPCP, "%s changing compproto: %08x --> %08x\n",
		  tbuff, ipcp->my_compproto, compproto);
        ipcp->my_compproto = compproto;
	break;

      case MODE_REJ:
	ipcp->peer_reject |= (1 << type);
	break;
      }
      break;

    case TY_IPADDRS:		/* RFC1172 */
      memcpy(&ipaddr.s_addr, cp + 2, 4);
      memcpy(&dstipaddr.s_addr, cp + 6, 4);
      snprintf(tbuff2, sizeof tbuff2, "%s %s,", tbuff, inet_ntoa(ipaddr));
      log_Printf(LogIPCP, "%s %s\n", tbuff2, inet_ntoa(dstipaddr));

      switch (mode_type) {
      case MODE_REQ:
	memcpy(dec->rejend, cp, length);
	dec->rejend += length;
	break;

      case MODE_NAK:
      case MODE_REJ:
	break;
      }
      break;

    case TY_PRIMARY_DNS:	/* DNS negotiation (rfc1877) */
    case TY_SECONDARY_DNS:
      memcpy(&ipaddr.s_addr, cp + 2, 4);
      log_Printf(LogIPCP, "%s %s\n", tbuff, inet_ntoa(ipaddr));

      switch (mode_type) {
      case MODE_REQ:
        if (!IsAccepted(ipcp->cfg.ns.dns_neg)) {
          ipcp->my_reject |= (1 << (type - TY_ADJUST_NS));
	  memcpy(dec->rejend, cp, length);
	  dec->rejend += length;
	  break;
        }
        if (!gotdns) {
          dns[0] = ipcp->cfg.ns.dns[0];
          dns[1] = ipcp->cfg.ns.dns[1];
          if (dns[0].s_addr == INADDR_ANY && dns[1].s_addr == INADDR_ANY)
            getdns(ipcp, dns);
          gotdns = 1;
        }
        have_ip = dns[type == TY_PRIMARY_DNS ? 0 : 1];

	if (ipaddr.s_addr != have_ip.s_addr) {
	  /*
	   * The client has got the DNS stuff wrong (first request) so
	   * we'll tell 'em how it is
	   */
	  memcpy(dec->nakend, cp, 2);	/* copy first two (type/length) */
	  memcpy(dec->nakend + 2, &have_ip.s_addr, length - 2);
	  dec->nakend += length;
	} else {
	  /*
	   * Otherwise they have it right (this time) so we send a ack packet
	   * back confirming it... end of story
	   */
	  memcpy(dec->ackend, cp, length);
	  dec->ackend += length;
        }
	break;

      case MODE_NAK:		/* what does this mean?? */
        if (IsEnabled(ipcp->cfg.ns.dns_neg)) {
          gotdnsnak = 1;
          memcpy(&dnsnak[type == TY_PRIMARY_DNS ? 0 : 1].s_addr, cp + 2, 4);
	}
	break;

      case MODE_REJ:		/* Can't do much, stop asking */
        ipcp->peer_reject |= (1 << (type - TY_ADJUST_NS));
	break;
      }
      break;

    case TY_PRIMARY_NBNS:	/* M$ NetBIOS nameserver hack (rfc1877) */
    case TY_SECONDARY_NBNS:
      memcpy(&ipaddr.s_addr, cp + 2, 4);
      log_Printf(LogIPCP, "%s %s\n", tbuff, inet_ntoa(ipaddr));

      switch (mode_type) {
      case MODE_REQ:
	have_ip.s_addr =
          ipcp->cfg.ns.nbns[type == TY_PRIMARY_NBNS ? 0 : 1].s_addr;

        if (have_ip.s_addr == INADDR_ANY) {
	  log_Printf(LogIPCP, "NBNS REQ - rejected - nbns not set\n");
          ipcp->my_reject |= (1 << (type - TY_ADJUST_NS));
	  memcpy(dec->rejend, cp, length);
	  dec->rejend += length;
	  break;
        }

	if (ipaddr.s_addr != have_ip.s_addr) {
	  memcpy(dec->nakend, cp, 2);
	  memcpy(dec->nakend+2, &have_ip.s_addr, length);
	  dec->nakend += length;
	} else {
	  memcpy(dec->ackend, cp, length);
	  dec->ackend += length;
        }
	break;

      case MODE_NAK:
	log_Printf(LogIPCP, "MS NBNS req %d - NAK??\n", type);
	break;

      case MODE_REJ:
	log_Printf(LogIPCP, "MS NBNS req %d - REJ??\n", type);
	break;
      }
      break;

    default:
      if (mode_type != MODE_NOP) {
        ipcp->my_reject |= (1 << type);
        memcpy(dec->rejend, cp, length);
        dec->rejend += length;
      }
      break;
    }
    plen -= length;
    cp += length;
  }

  if (gotdnsnak)
    if (!setdns(ipcp, dnsnak)) {
      ipcp->peer_reject |= (1 << (TY_PRIMARY_DNS - TY_ADJUST_NS));
      ipcp->peer_reject |= (1 << (TY_SECONDARY_DNS - TY_ADJUST_NS));
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

extern struct mbuf *
ipcp_Input(struct bundle *bundle, struct link *l, struct mbuf *bp)
{
  /* Got PROTO_IPCP from link */
  mbuf_SetType(bp, MB_IPCPIN);
  if (bundle_Phase(bundle) == PHASE_NETWORK)
    fsm_Input(&bundle->ncp.ipcp.fsm, bp);
  else {
    if (bundle_Phase(bundle) < PHASE_NETWORK)
      log_Printf(LogIPCP, "%s: Error: Unexpected IPCP in phase %s (ignored)\n",
                 l->name, bundle_PhaseName(bundle));
    mbuf_Free(bp);
  }
  return NULL;
}

int
ipcp_UseHisIPaddr(struct bundle *bundle, struct in_addr hisaddr)
{
  struct ipcp *ipcp = &bundle->ncp.ipcp;

  memset(&ipcp->cfg.peer_range, '\0', sizeof ipcp->cfg.peer_range);
  iplist_reset(&ipcp->cfg.peer_list);
  ipcp->peer_ip = ipcp->cfg.peer_range.ipaddr = hisaddr;
  ipcp->cfg.peer_range.mask.s_addr = INADDR_BROADCAST;
  ipcp->cfg.peer_range.width = 32;

  if (ipcp_SetIPaddress(bundle, ipcp->cfg.my_range.ipaddr, hisaddr, 0) < 0)
    return 0;

  return 1;	/* Ok */
}

int
ipcp_UseHisaddr(struct bundle *bundle, const char *hisaddr, int setaddr)
{
  struct ipcp *ipcp = &bundle->ncp.ipcp;

  /* Use `hisaddr' for the peers address (set iface if `setaddr') */
  memset(&ipcp->cfg.peer_range, '\0', sizeof ipcp->cfg.peer_range);
  iplist_reset(&ipcp->cfg.peer_list);
  if (strpbrk(hisaddr, ",-")) {
    iplist_setsrc(&ipcp->cfg.peer_list, hisaddr);
    if (iplist_isvalid(&ipcp->cfg.peer_list)) {
      iplist_setrandpos(&ipcp->cfg.peer_list);
      ipcp->peer_ip = ChooseHisAddr(bundle, ipcp->my_ip);
      if (ipcp->peer_ip.s_addr == INADDR_ANY) {
        log_Printf(LogWARN, "%s: None available !\n", ipcp->cfg.peer_list.src);
        return 0;
      }
      ipcp->cfg.peer_range.ipaddr.s_addr = ipcp->peer_ip.s_addr;
      ipcp->cfg.peer_range.mask.s_addr = INADDR_BROADCAST;
      ipcp->cfg.peer_range.width = 32;
    } else {
      log_Printf(LogWARN, "%s: Invalid range !\n", hisaddr);
      return 0;
    }
  } else if (ParseAddr(ipcp, hisaddr, &ipcp->cfg.peer_range.ipaddr,
		       &ipcp->cfg.peer_range.mask,
                       &ipcp->cfg.peer_range.width) != 0) {
    ipcp->peer_ip.s_addr = ipcp->cfg.peer_range.ipaddr.s_addr;

    if (setaddr && ipcp_SetIPaddress(bundle, ipcp->cfg.my_range.ipaddr,
                                     ipcp->cfg.peer_range.ipaddr, 0) < 0)
      return 0;
  } else
    return 0;

  bundle_AdjustFilters(bundle, NULL, &ipcp->peer_ip);

  return 1;	/* Ok */
}

struct in_addr
addr2mask(struct in_addr addr)
{
  u_int32_t haddr = ntohl(addr.s_addr);

  haddr = IN_CLASSA(haddr) ? IN_CLASSA_NET :
          IN_CLASSB(haddr) ? IN_CLASSB_NET :
          IN_CLASSC_NET;
  addr.s_addr = htonl(haddr);

  return addr;
}

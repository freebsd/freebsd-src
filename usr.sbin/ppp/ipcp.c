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
 * $Id: ipcp.c,v 1.54 1998/06/12 17:45:10 brian Exp $
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
#include <net/if.h>
#include <sys/sockio.h>
#include <sys/un.h>

#include <fcntl.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <termios.h>
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
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "loadalias.h"
#include "vjcomp.h"
#include "lqr.h"
#include "hdlc.h"
#include "async.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#include "bundle.h"
#include "id.h"
#include "arp.h"
#include "systems.h"
#include "prompt.h"
#include "route.h"

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
static void IpcpInitRestartCounter(struct fsm *);
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
  }

  if (ipcp->route) {
    prompt_Printf(arg->prompt, "\n");
    route_ShowSticky(arg->prompt, ipcp->route);
  }

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  prompt_Printf(arg->prompt, " My Address:      %s/%d",
	        inet_ntoa(ipcp->cfg.my_range.ipaddr), ipcp->cfg.my_range.width);

  if (ipcp->cfg.HaveTriggerAddress)
    prompt_Printf(arg->prompt, " (trigger with %s)",
                  inet_ntoa(ipcp->cfg.TriggerAddress));
  prompt_Printf(arg->prompt, "\n VJ compression:  %s (%d slots %s slot "
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

  fsm_Init(&ipcp->fsm, "IPCP", PROTO_IPCP, 1, IPCP_MAXCODE, 10, LogIPCP,
           bundle, l, parent, &ipcp_Callbacks, timer_names);

  ipcp->route = NULL;
  ipcp->cfg.vj.slots = DEF_VJ_STATES;
  ipcp->cfg.vj.slotcomp = 1;
  memset(&ipcp->cfg.my_range, '\0', sizeof ipcp->cfg.my_range);
  if (gethostname(name, sizeof name) == 0) {
    hp = gethostbyname(name);
    if (hp && hp->h_addrtype == AF_INET) {
      memcpy(&ipcp->cfg.my_range.ipaddr.s_addr, hp->h_addr, hp->h_length);
      ipcp->cfg.peer_range.mask.s_addr = INADDR_BROADCAST;
      ipcp->cfg.peer_range.width = 32;
    }
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

  ipcp->cfg.fsmretry = DEF_FSMRETRY;
  ipcp->cfg.vj.neg = NEG_ENABLED|NEG_ACCEPTED;

  memset(&ipcp->vj, '\0', sizeof ipcp->vj);

  ipcp->my_ifip.s_addr = INADDR_ANY;
  ipcp->peer_ifip.s_addr = INADDR_ANY;

  throughput_init(&ipcp->throughput);
  ipcp_Setup(ipcp);
}

void
ipcp_SetLink(struct ipcp *ipcp, struct link *l)
{
  ipcp->fsm.link = l;
}

void
ipcp_Setup(struct ipcp *ipcp)
{
  int pos;

  ipcp->fsm.open_mode = 0;
  ipcp->fsm.maxconfig = 10;

  if (iplist_isvalid(&ipcp->cfg.peer_list)) {
    if (ipcp->my_ifip.s_addr != INADDR_ANY &&
        (pos = iplist_ip2pos(&ipcp->cfg.peer_list, ipcp->my_ifip)) != -1)
      ipcp->cfg.peer_range.ipaddr = iplist_setcurpos(&ipcp->cfg.peer_list, pos);
    else
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
  } else if ((ipcp->my_ifip.s_addr & ipcp->cfg.my_range.mask.s_addr) ==
             (ipcp->cfg.my_range.ipaddr.s_addr &
              ipcp->cfg.my_range.mask.s_addr))
    /*
     * Otherwise, if we've been assigned an IP number before, we really
     * want to keep the same IP number so that we can keep any existing
     * connections that are bound to that IP.
     */
    ipcp->my_ip = ipcp->my_ifip;
  else
    ipcp->my_ip = ipcp->cfg.my_range.ipaddr;

  if (IsEnabled(ipcp->cfg.vj.neg))
    ipcp->my_compproto = (PROTO_VJCOMP << 16) +
                         ((ipcp->cfg.vj.slots - 1) << 8) +
                         ipcp->cfg.vj.slotcomp;
  else
    ipcp->my_compproto = 0;
  sl_compress_init(&ipcp->vj.cslc, ipcp->cfg.vj.slots - 1);

  ipcp->peer_reject = 0;
  ipcp->my_reject = 0;

  throughput_stop(&ipcp->throughput);
  throughput_init(&ipcp->throughput);
}

static int
ipcp_SetIPaddress(struct bundle *bundle, struct in_addr myaddr,
                  struct in_addr hisaddr, int silent)
{
  struct sockaddr_in *sock_in;
  int s;
  u_long mask, addr;
  struct ifaliasreq ifra;

  /* If given addresses are alreay set, then ignore this request */
  if (bundle->ncp.ipcp.my_ifip.s_addr == myaddr.s_addr &&
      bundle->ncp.ipcp.peer_ifip.s_addr == hisaddr.s_addr)
    return 0;

  ipcp_CleanInterface(&bundle->ncp.ipcp);

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "SetIpDevice: socket(): %s\n", strerror(errno));
    return (-1);
  }

  memset(&ifra, '\0', sizeof ifra);
  strncpy(ifra.ifra_name, bundle->ifp.Name, sizeof ifra.ifra_name - 1);
  ifra.ifra_name[sizeof ifra.ifra_name - 1] = '\0';

  /* Set interface address */
  sock_in = (struct sockaddr_in *)&ifra.ifra_addr;
  sock_in->sin_family = AF_INET;
  sock_in->sin_addr = myaddr;
  sock_in->sin_len = sizeof *sock_in;

  /* Set destination address */
  sock_in = (struct sockaddr_in *)&ifra.ifra_broadaddr;
  sock_in->sin_family = AF_INET;
  sock_in->sin_addr = hisaddr;
  sock_in->sin_len = sizeof *sock_in;

  addr = ntohl(myaddr.s_addr);
  if (IN_CLASSA(addr))
    mask = IN_CLASSA_NET;
  else if (IN_CLASSB(addr))
    mask = IN_CLASSB_NET;
  else
    mask = IN_CLASSC_NET;

  /* if subnet mask is given, use it instead of class mask */
  if (bundle->ncp.ipcp.cfg.netmask.s_addr != INADDR_ANY &&
      (ntohl(bundle->ncp.ipcp.cfg.netmask.s_addr) & mask) == mask)
    mask = ntohl(bundle->ncp.ipcp.cfg.netmask.s_addr);

  sock_in = (struct sockaddr_in *)&ifra.ifra_mask;
  sock_in->sin_family = AF_INET;
  sock_in->sin_addr.s_addr = htonl(mask);
  sock_in->sin_len = sizeof *sock_in;

  if (ID0ioctl(s, SIOCAIFADDR, &ifra) < 0) {
    if (!silent)
      log_Printf(LogERROR, "SetIpDevice: ioctl(SIOCAIFADDR): %s\n",
		strerror(errno));
    close(s);
    return (-1);
  }

  if (Enabled(bundle, OPT_SROUTES))
    route_Change(bundle, bundle->ncp.ipcp.route, myaddr, hisaddr);

  bundle->ncp.ipcp.peer_ifip.s_addr = hisaddr.s_addr;
  bundle->ncp.ipcp.my_ifip.s_addr = myaddr.s_addr;

  if (Enabled(bundle, OPT_PROXY))
    arp_SetProxy(bundle, bundle->ncp.ipcp.peer_ifip, s);

  close(s);
  return (0);
}

static struct in_addr
ChooseHisAddr(struct bundle *bundle, const struct in_addr gw)
{
  struct in_addr try;
  int f;

  for (f = 0; f < bundle->ncp.ipcp.cfg.peer_list.nItems; f++) {
    try = iplist_next(&bundle->ncp.ipcp.cfg.peer_list);
    log_Printf(LogDEBUG, "ChooseHisAddr: Check item %d (%s)\n",
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
IpcpInitRestartCounter(struct fsm * fp)
{
  /* Set fsm timer load */
  struct ipcp *ipcp = fsm2ipcp(fp);

  fp->FsmTimer.load = ipcp->cfg.fsmretry * SECTICKS;
  fp->restart = 5;
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
    *(u_int32_t *)o->data = ipcp->my_ip.s_addr;
    INC_LCP_OPT(TY_IPADDR, 6, o);
  }

  if (ipcp->my_compproto && !REJECTED(ipcp, TY_COMPPROTO)) {
    if (ipcp->heis1172) {
      *(u_short *)o->data = htons(PROTO_VJCOMP);
      INC_LCP_OPT(TY_COMPPROTO, 4, o);
    } else {
      *(u_long *)o->data = htonl(ipcp->my_compproto);
      INC_LCP_OPT(TY_COMPPROTO, 6, o);
    }
  }

  if (IsEnabled(ipcp->cfg.ns.dns_neg) &&
      !REJECTED(ipcp, TY_PRIMARY_DNS - TY_ADJUST_NS) &&
      !REJECTED(ipcp, TY_SECONDARY_DNS - TY_ADJUST_NS)) {
    struct in_addr dns[2];
    getdns(ipcp, dns);
    *(u_int32_t *)o->data = dns[0].s_addr;
    INC_LCP_OPT(TY_PRIMARY_DNS, 6, o);
    *(u_int32_t *)o->data = dns[1].s_addr;
    INC_LCP_OPT(TY_SECONDARY_DNS, 6, o);
  }

  fsm_Output(fp, CODE_CONFIGREQ, fp->reqid, buff, (u_char *)o - buff);
}

static void
IpcpSentTerminateReq(struct fsm * fp)
{
  /* Term REQ just sent by FSM */
}

static void
IpcpSendTerminateAck(struct fsm *fp, u_char id)
{
  /* Send Term ACK please */
  fsm_Output(fp, CODE_TERMACK, id, NULL, 0);
}

static void
IpcpLayerStart(struct fsm * fp)
{
  /* We're about to start up ! */
  log_Printf(LogIPCP, "%s: IpcpLayerStart.\n", fp->link->name);

  /* This is where we should be setting up the interface in AUTO mode */
}

static void
IpcpLayerFinish(struct fsm *fp)
{
  /* We're now down */
  log_Printf(LogIPCP, "%s: IpcpLayerFinish.\n", fp->link->name);
}

void
ipcp_CleanInterface(struct ipcp *ipcp)
{
  struct ifaliasreq ifra;
  struct sockaddr_in *me, *peer;
  int s;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "ipcp_CleanInterface: socket: %s\n", strerror(errno));
    return;
  }

  route_Clean(ipcp->fsm.bundle, ipcp->route);

  if (Enabled(ipcp->fsm.bundle, OPT_PROXY))
    arp_ClearProxy(ipcp->fsm.bundle, ipcp->peer_ifip, s);

  if (ipcp->my_ifip.s_addr != INADDR_ANY ||
      ipcp->peer_ifip.s_addr != INADDR_ANY) {
    memset(&ifra, '\0', sizeof ifra);
    strncpy(ifra.ifra_name, ipcp->fsm.bundle->ifp.Name,
            sizeof ifra.ifra_name - 1);
    ifra.ifra_name[sizeof ifra.ifra_name - 1] = '\0';
    me = (struct sockaddr_in *)&ifra.ifra_addr;
    peer = (struct sockaddr_in *)&ifra.ifra_broadaddr;
    me->sin_family = peer->sin_family = AF_INET;
    me->sin_len = peer->sin_len = sizeof(struct sockaddr_in);
    me->sin_addr = ipcp->my_ifip;
    peer->sin_addr = ipcp->peer_ifip;
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0)
      log_Printf(LogERROR, "ipcp_CleanInterface: ioctl(SIOCDIFADDR): %s\n",
                strerror(errno));
    ipcp->my_ifip.s_addr = ipcp->peer_ifip.s_addr = INADDR_ANY;
  }

  close(s);
}

static void
IpcpLayerDown(struct fsm *fp)
{
  /* About to come down */
  struct ipcp *ipcp = fsm2ipcp(fp);
  const char *s;

  s = inet_ntoa(ipcp->peer_ifip);
  log_Printf(LogIPCP, "%s: IpcpLayerDown: %s\n", fp->link->name, s);

  throughput_stop(&ipcp->throughput);
  throughput_log(&ipcp->throughput, LogIPCP, NULL);
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

  if (!(ipcp->fsm.bundle->phys_type.all & PHYS_AUTO))
    ipcp_CleanInterface(ipcp);
}

int
ipcp_InterfaceUp(struct ipcp *ipcp)
{
  if (ipcp_SetIPaddress(ipcp->fsm.bundle, ipcp->my_ip, ipcp->peer_ip, 0) < 0) {
    log_Printf(LogERROR, "IpcpLayerUp: unable to set ip address\n");
    return 0;
  }

#ifndef NOALIAS
  if (alias_IsEnabled())
    (*PacketAlias.SetAddress)(ipcp->my_ip);
#endif

  return 1;
}

static int
IpcpLayerUp(struct fsm *fp)
{
  /* We're now up */
  struct ipcp *ipcp = fsm2ipcp(fp);
  char tbuff[100];

  log_Printf(LogIPCP, "%s: IpcpLayerUp.\n", fp->link->name);
  snprintf(tbuff, sizeof tbuff, "myaddr = %s ", inet_ntoa(ipcp->my_ip));
  log_Printf(LogIPCP, " %s hisaddr = %s\n", tbuff, inet_ntoa(ipcp->peer_ip));

  if (ipcp->peer_compproto >> 16 == PROTO_VJCOMP)
    sl_compress_init(&ipcp->vj.cslc, (ipcp->peer_compproto >> 8) & 255);

  if (!ipcp_InterfaceUp(ipcp))
    return 0;

  /*
   * XXX this stuff should really live in the FSM.  Our config should
   * associate executable sections in files with events.
   */
  if (system_Select(fp->bundle, inet_ntoa(ipcp->my_ifip), LINKUPFILE,
                    NULL, NULL) < 0) {
    if (bundle_GetLabel(fp->bundle)) {
      if (system_Select(fp->bundle, bundle_GetLabel(fp->bundle),
                       LINKUPFILE, NULL, NULL) < 0)
        system_Select(fp->bundle, "MYADDR", LINKUPFILE, NULL, NULL);
    } else
      system_Select(fp->bundle, "MYADDR", LINKUPFILE, NULL, NULL);
  }

  throughput_start(&ipcp->throughput, "IPCP throughput",
                   Enabled(fp->bundle, OPT_THROUGHPUT));
  log_DisplayPrompts();
  return 1;
}

static int
AcceptableAddr(struct in_range *prange, struct in_addr ipaddr)
{
  /* Is the given IP in the given range ? */
  return (prange->ipaddr.s_addr & prange->mask.s_addr) ==
    (ipaddr.s_addr & prange->mask.s_addr) && ipaddr.s_addr;
}

static void
IpcpDecodeConfig(struct fsm *fp, u_char * cp, int plen, int mode_type,
                 struct fsm_decode *dec)
{
  /* Deal with incoming PROTO_IPCP */
  struct ipcp *ipcp = fsm2ipcp(fp);
  int type, length;
  u_int32_t compproto;
  struct compreq *pcomp;
  struct in_addr ipaddr, dstipaddr, have_ip, dns[2], dnsnak[2];
  char tbuff[100], tbuff2[100];
  int gotdns, gotdnsnak;

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
      ipaddr.s_addr = *(u_int32_t *)(cp + 2);
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
            if (iplist_ip2pos(&ipcp->cfg.peer_list, ipcp->peer_ifip) >= 0)
              /*
               * If we've already got a valid address configured for the peer
               * (in AUTO mode), try NAKing with that so that we don't
               * have to upset things too much.
               */
              ipcp->peer_ip = ipcp->peer_ifip;
            else
              /* Just pick an IP number from our list */
              ipcp->peer_ip = ChooseHisAddr
                (fp->bundle, ipcp->cfg.my_range.ipaddr);

            if (ipcp->peer_ip.s_addr == INADDR_ANY) {
	      memcpy(dec->rejend, cp, length);
	      dec->rejend += length;
            } else {
	      memcpy(dec->nakend, cp, 2);
	      memcpy(dec->nakend+2, &ipcp->peer_ip.s_addr, length - 2);
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
          if ((ipcp->peer_ifip.s_addr & ipcp->cfg.peer_range.mask.s_addr) ==
             (ipcp->cfg.peer_range.ipaddr.s_addr &
              ipcp->cfg.peer_range.mask.s_addr))
            /* We prefer the already-configured address */
	    memcpy(dec->nakend+2, &ipcp->peer_ifip.s_addr, length - 2);
          else
	    memcpy(dec->nakend+2, &ipcp->peer_ip.s_addr, length - 2);
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
      compproto = htonl(*(u_int32_t *)(cp + 2));
      log_Printf(LogIPCP, "%s %s\n", tbuff, vj2asc(compproto));

      switch (mode_type) {
      case MODE_REQ:
	if (!IsAccepted(ipcp->cfg.vj.neg)) {
	  memcpy(dec->rejend, cp, length);
	  dec->rejend += length;
	} else {
	  pcomp = (struct compreq *) (cp + 2);
	  switch (length) {
	  case 4:		/* RFC1172 */
	    if (ntohs(pcomp->proto) == PROTO_VJCOMP) {
	      log_Printf(LogWARN, "Peer is speaking RFC1172 compression protocol !\n");
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
	    if (ntohs(pcomp->proto) == PROTO_VJCOMP
		&& pcomp->slots <= MAX_VJ_STATES
                && pcomp->slots >= MIN_VJ_STATES) {
	      ipcp->peer_compproto = compproto;
	      ipcp->heis1172 = 0;
	      memcpy(dec->ackend, cp, length);
	      dec->ackend += length;
	    } else {
	      memcpy(dec->nakend, cp, 2);
	      pcomp->proto = htons(PROTO_VJCOMP);
	      pcomp->slots = DEF_VJ_STATES;
	      pcomp->compcid = 0;
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
      ipaddr.s_addr = *(u_int32_t *)(cp + 2);
      dstipaddr.s_addr = *(u_int32_t *)(cp + 6);
      snprintf(tbuff2, sizeof tbuff2, "%s %s,", tbuff, inet_ntoa(ipaddr));
      log_Printf(LogIPCP, "%s %s\n", tbuff2, inet_ntoa(dstipaddr));

      switch (mode_type) {
      case MODE_REQ:
	ipcp->peer_ip = ipaddr;
	ipcp->my_ip = dstipaddr;
	memcpy(dec->ackend, cp, length);
	dec->ackend += length;
	break;
      case MODE_NAK:
        snprintf(tbuff2, sizeof tbuff2, "%s changing address: %s", tbuff,
		 inet_ntoa(ipcp->my_ip));
	log_Printf(LogIPCP, "%s --> %s\n", tbuff2, inet_ntoa(ipaddr));
	ipcp->my_ip = ipaddr;
	ipcp->peer_ip = dstipaddr;
	break;
      case MODE_REJ:
	ipcp->peer_reject |= (1 << type);
	break;
      }
      break;

    case TY_PRIMARY_DNS:	/* DNS negotiation (rfc1877) */
    case TY_SECONDARY_DNS:
      ipaddr.s_addr = *(u_int32_t *)(cp + 2);
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
          dnsnak[type == TY_PRIMARY_DNS ? 0 : 1].s_addr =
            *(u_int32_t *)(cp + 2);
	}
	break;
      case MODE_REJ:		/* Can't do much, stop asking */
        ipcp->peer_reject |= (1 << (type - TY_ADJUST_NS));
	break;
      }
      break;

    case TY_PRIMARY_NBNS:	/* M$ NetBIOS nameserver hack (rfc1877) */
    case TY_SECONDARY_NBNS:
      ipaddr.s_addr = *(u_int32_t *)(cp + 2);
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

void
ipcp_Input(struct ipcp *ipcp, struct bundle *bundle, struct mbuf *bp)
{
  /* Got PROTO_IPCP from link */
  if (bundle_Phase(bundle) == PHASE_NETWORK)
    fsm_Input(&ipcp->fsm, bp);
  else {
    if (bundle_Phase(bundle) < PHASE_NETWORK)
      log_Printf(LogIPCP, "%s: Error: Unexpected IPCP in phase %s (ignored)\n",
                 ipcp->fsm.link->name, bundle_PhaseName(bundle));
    mbuf_Free(bp);
  }
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
        return(0);
      }
      ipcp->cfg.peer_range.ipaddr.s_addr = ipcp->peer_ip.s_addr;
      ipcp->cfg.peer_range.mask.s_addr = INADDR_BROADCAST;
      ipcp->cfg.peer_range.width = 32;
    } else {
      log_Printf(LogWARN, "%s: Invalid range !\n", hisaddr);
      return 0;
    }
  } else if (ParseAddr(ipcp, 1, &hisaddr, &ipcp->cfg.peer_range.ipaddr,
		       &ipcp->cfg.peer_range.mask,
                       &ipcp->cfg.peer_range.width) != 0) {
    ipcp->peer_ip.s_addr = ipcp->cfg.peer_range.ipaddr.s_addr;

    if (setaddr && ipcp_SetIPaddress(bundle, ipcp->cfg.my_range.ipaddr,
                                     ipcp->cfg.peer_range.ipaddr, 0) < 0) {
      ipcp->cfg.my_range.ipaddr.s_addr = INADDR_ANY;
      ipcp->cfg.peer_range.ipaddr.s_addr = INADDR_ANY;
      return 0;
    }
  } else
    return 0;

  return 1;
}

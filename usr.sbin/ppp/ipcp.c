/*-
 * Copyright (c) 1996 - 2001 Brian Somers <brian@Awfulhak.org>
 *          based on work by Toshiharu OHNO <tony-o@iij.ad.jp>
 *                           Internet Initiative Japan, Inc (IIJ)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#ifndef NONAT
#ifdef LOCALNAT
#include "alias.h"
#else
#include <alias.h>
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
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
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

static u_short default_urgent_tcp_ports[] = {
  21,	/* ftp */
  22,	/* ssh */
  23,	/* telnet */
  513,	/* login */
  514,	/* shell */
  543,	/* klogin */
  544	/* kshell */
};

static u_short default_urgent_udp_ports[] = { };

#define NDEFTCPPORTS \
  (sizeof default_urgent_tcp_ports / sizeof default_urgent_tcp_ports[0])
#define NDEFUDPPORTS \
  (sizeof default_urgent_udp_ports / sizeof default_urgent_udp_ports[0])

int
ipcp_IsUrgentPort(struct port_range *range, u_short src, u_short dst)
{
  int f;

  for (f = 0; f < range->nports; f++)
    if (range->port[f] == src || range->port[f] == dst)
      return 1;

  return 0;
}

void
ipcp_AddUrgentPort(struct port_range *range, u_short port)
{
  u_short *newport;
  int p;

  if (range->nports == range->maxports) {
    range->maxports += 10;
    newport = (u_short *)realloc(range->port,
                                 range->maxports * sizeof(u_short));
    if (newport == NULL) {
      log_Printf(LogERROR, "ipcp_AddUrgentPort: realloc: %s\n",
                 strerror(errno));
      range->maxports -= 10;
      return;
    }
    range->port = newport;
  }

  for (p = 0; p < range->nports; p++)
    if (range->port[p] == port) {
      log_Printf(LogWARN, "%u: Port already set to urgent\n", port);
      break;
    } else if (range->port[p] > port) {
      memmove(range->port + p + 1, range->port + p,
              (range->nports - p) * sizeof(u_short));
      range->port[p] = port;
      range->nports++;
      break;
    }

  if (p == range->nports)
    range->port[range->nports++] = port;
}

void
ipcp_RemoveUrgentPort(struct port_range *range, u_short port)
{
  int p;

  for (p = 0; p < range->nports; p++)
    if (range->port[p] == port) {
      if (p != range->nports - 1)
        memmove(range->port + p, range->port + p + 1,
                (range->nports - p - 1) * sizeof(u_short));
      range->nports--;
      return;
    }

  if (p == range->nports)
    log_Printf(LogWARN, "%u: Port not set to urgent\n", port);
}

void
ipcp_ClearUrgentPorts(struct port_range *range)
{
  range->nports = 0;
}

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

static const char *
protoname(int proto)
{
  static struct {
    int id;
    const char *txt;
  } cftypes[] = {
    /* Check out the latest ``Assigned numbers'' rfc (rfc1700.txt) */
    { 1, "IPADDRS" },		/* IP-Addresses */	/* deprecated */
    { 2, "COMPPROTO" },		/* IP-Compression-Protocol */
    { 3, "IPADDR" },		/* IP-Address */
    { 129, "PRIDNS" },		/* 129: Primary DNS Server Address */
    { 130, "PRINBNS" },		/* 130: Primary NBNS Server Address */
    { 131, "SECDNS" },		/* 131: Secondary DNS Server Address */
    { 132, "SECNBNS" }		/* 132: Secondary NBNS Server Address */
  };
  int f;

  for (f = 0; f < sizeof cftypes / sizeof *cftypes; f++)
    if (cftypes[f].id == proto)
      return cftypes[f].txt;

  return NumStr(proto, NULL, 0);
}

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

void
ipcp_LoadDNS(struct ipcp *ipcp)
{
  int fd;

  ipcp->ns.dns[0].s_addr = ipcp->ns.dns[1].s_addr = INADDR_NONE;

  if (ipcp->ns.resolv != NULL) {
    free(ipcp->ns.resolv);
    ipcp->ns.resolv = NULL;
  }
  if (ipcp->ns.resolv_nons != NULL) {
    free(ipcp->ns.resolv_nons);
    ipcp->ns.resolv_nons = NULL;
  }
  ipcp->ns.resolver = 0;

  if ((fd = open(_PATH_RESCONF, O_RDONLY)) != -1) {
    struct stat st;

    if (fstat(fd, &st) == 0) {
      ssize_t got;

      if ((ipcp->ns.resolv_nons = (char *)malloc(st.st_size + 1)) == NULL)
        log_Printf(LogERROR, "Failed to malloc %lu for %s: %s\n",
                   (unsigned long)st.st_size, _PATH_RESCONF, strerror(errno));
      else if ((ipcp->ns.resolv = (char *)malloc(st.st_size + 1)) == NULL) {
        log_Printf(LogERROR, "Failed(2) to malloc %lu for %s: %s\n",
                   (unsigned long)st.st_size, _PATH_RESCONF, strerror(errno));
        free(ipcp->ns.resolv_nons);
        ipcp->ns.resolv_nons = NULL;
      } else if ((got = read(fd, ipcp->ns.resolv, st.st_size)) != st.st_size) {
        if (got == -1)
          log_Printf(LogERROR, "Failed to read %s: %s\n",
                     _PATH_RESCONF, strerror(errno));
        else
          log_Printf(LogERROR, "Failed to read %s, got %lu not %lu\n",
                     _PATH_RESCONF, (unsigned long)got,
                     (unsigned long)st.st_size);
        free(ipcp->ns.resolv_nons);
        ipcp->ns.resolv_nons = NULL;
        free(ipcp->ns.resolv);
        ipcp->ns.resolv = NULL;
      } else {
        char *cp, *cp_nons, *ncp, ch;
        int n;

        ipcp->ns.resolv[st.st_size] = '\0';
        ipcp->ns.resolver = 1;

        cp_nons = ipcp->ns.resolv_nons;
        cp = ipcp->ns.resolv;
        n = 0;

        while ((ncp = strstr(cp, "nameserver")) != NULL) {
          if (ncp != cp) {
            memcpy(cp_nons, cp, ncp - cp);
            cp_nons += ncp - cp;
          }
          if ((ncp != cp && ncp[-1] != '\n') || !issep(ncp[10])) {
            memcpy(cp_nons, ncp, 9);
            cp_nons += 9;
            cp = ncp + 9;	/* Can't match "nameserver" at cp... */
            continue;
          }

          for (cp = ncp + 11; issep(*cp); cp++)	/* Skip whitespace */
            ;

          for (ncp = cp; isip(*ncp); ncp++)		/* Jump over IP */
            ;

          ch = *ncp;
          *ncp = '\0';
          if (n < 2 && inet_aton(cp, ipcp->ns.dns + n))
            n++;
          *ncp = ch;

          if ((cp = strchr(ncp, '\n')) == NULL)	/* Point at next line */
            cp = ncp + strlen(ncp);
          else
            cp++;
        }
        strcpy(cp_nons, cp);	/* Copy the end - including the NUL */
        cp_nons += strlen(cp_nons) - 1;
        while (cp_nons >= ipcp->ns.resolv_nons && *cp_nons == '\n')
          *cp_nons-- = '\0';
        if (n == 2 && ipcp->ns.dns[0].s_addr == INADDR_ANY) {
          ipcp->ns.dns[0].s_addr = ipcp->ns.dns[1].s_addr;
          ipcp->ns.dns[1].s_addr = INADDR_ANY;
        }
        bundle_AdjustDNS(ipcp->fsm.bundle, ipcp->ns.dns);
      }
    } else
      log_Printf(LogERROR, "Failed to stat opened %s: %s\n",
                 _PATH_RESCONF, strerror(errno));

    close(fd);
  }
}

int
ipcp_WriteDNS(struct ipcp *ipcp)
{
  const char *paddr;
  mode_t mask;
  FILE *fp;

  if (ipcp->ns.dns[0].s_addr == INADDR_ANY &&
      ipcp->ns.dns[1].s_addr == INADDR_ANY) {
    log_Printf(LogIPCP, "%s not modified: All nameservers NAKd\n",
              _PATH_RESCONF);
    return 0;
  }

  if (ipcp->ns.dns[0].s_addr == INADDR_ANY) {
    ipcp->ns.dns[0].s_addr = ipcp->ns.dns[1].s_addr;
    ipcp->ns.dns[1].s_addr = INADDR_ANY;
  }

  mask = umask(022);
  if ((fp = ID0fopen(_PATH_RESCONF, "w")) != NULL) {
    umask(mask);
    if (ipcp->ns.resolv_nons)
      fputs(ipcp->ns.resolv_nons, fp);
    paddr = inet_ntoa(ipcp->ns.dns[0]);
    log_Printf(LogIPCP, "Primary nameserver set to %s\n", paddr);
    fprintf(fp, "\nnameserver %s\n", paddr);
    if (ipcp->ns.dns[1].s_addr != INADDR_ANY &&
        ipcp->ns.dns[1].s_addr != INADDR_NONE &&
        ipcp->ns.dns[1].s_addr != ipcp->ns.dns[0].s_addr) {
      paddr = inet_ntoa(ipcp->ns.dns[1]);
      log_Printf(LogIPCP, "Secondary nameserver set to %s\n", paddr);
      fprintf(fp, "nameserver %s\n", paddr);
    }
    if (fclose(fp) == EOF) {
      log_Printf(LogERROR, "write(): Failed updating %s: %s\n", _PATH_RESCONF,
                 strerror(errno));
      return 0;
    }
  } else
    umask(mask);

  return 1;
}

void
ipcp_RestoreDNS(struct ipcp *ipcp)
{
  if (ipcp->ns.resolver) {
    ssize_t got;
    size_t len;
    int fd;

    if ((fd = ID0open(_PATH_RESCONF, O_WRONLY|O_TRUNC, 0644)) != -1) {
      len = strlen(ipcp->ns.resolv);
      if ((got = write(fd, ipcp->ns.resolv, len)) != len) {
        if (got == -1)
          log_Printf(LogERROR, "Failed rewriting %s: write: %s\n",
                     _PATH_RESCONF, strerror(errno));
        else
          log_Printf(LogERROR, "Failed rewriting %s: wrote %lu of %lu\n",
                     _PATH_RESCONF, (unsigned long)got, (unsigned long)len);
      }
      close(fd);
    } else
      log_Printf(LogERROR, "Failed rewriting %s: open: %s\n", _PATH_RESCONF,
                 strerror(errno));
  } else if (remove(_PATH_RESCONF) == -1)
    log_Printf(LogERROR, "Failed removing %s: %s\n", _PATH_RESCONF,
               strerror(errno));
  
}

int
ipcp_Show(struct cmdargs const *arg)
{
  struct ipcp *ipcp = &arg->bundle->ncp.ipcp;
  int p;

  prompt_Printf(arg->prompt, "%s [%s]\n", ipcp->fsm.name,
                State2Nam(ipcp->fsm.state));
  if (ipcp->fsm.state == ST_OPENED) {
    prompt_Printf(arg->prompt, " His side:        %s, %s\n",
	          inet_ntoa(ipcp->peer_ip), vj2asc(ipcp->peer_compproto));
    prompt_Printf(arg->prompt, " My side:         %s, %s\n",
	          inet_ntoa(ipcp->my_ip), vj2asc(ipcp->my_compproto));
    prompt_Printf(arg->prompt, " Queued packets:  %lu\n",
                  (unsigned long)ip_QueueLen(ipcp));
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

  prompt_Printf(arg->prompt, " DNS:             %s",
                ipcp->cfg.ns.dns[0].s_addr == INADDR_NONE ?
                "none" : inet_ntoa(ipcp->cfg.ns.dns[0]));
  if (ipcp->cfg.ns.dns[1].s_addr != INADDR_NONE)
    prompt_Printf(arg->prompt, ", %s", inet_ntoa(ipcp->cfg.ns.dns[1]));
  prompt_Printf(arg->prompt, ", %s\n",
                command_ShowNegval(ipcp->cfg.ns.dns_neg));
  prompt_Printf(arg->prompt, " Resolver DNS:    %s",
                ipcp->ns.dns[0].s_addr == INADDR_NONE ?
                "none" : inet_ntoa(ipcp->ns.dns[0]));
  if (ipcp->ns.dns[1].s_addr != INADDR_NONE &&
      ipcp->ns.dns[1].s_addr != ipcp->ns.dns[0].s_addr)
    prompt_Printf(arg->prompt, ", %s", inet_ntoa(ipcp->ns.dns[1]));
  prompt_Printf(arg->prompt, "\n NetBIOS NS:      %s, ",
	        inet_ntoa(ipcp->cfg.ns.nbns[0]));
  prompt_Printf(arg->prompt, "%s\n", inet_ntoa(ipcp->cfg.ns.nbns[1]));

  prompt_Printf(arg->prompt, " Urgent ports\n");
  prompt_Printf(arg->prompt, "          TCP:    ");
  if (ipcp->cfg.urgent.tcp.nports == 0)
    prompt_Printf(arg->prompt, "none");
  else
    for (p = 0; p < ipcp->cfg.urgent.tcp.nports; p++) {
      if (p)
        prompt_Printf(arg->prompt, ", ");
      prompt_Printf(arg->prompt, "%u", ipcp->cfg.urgent.tcp.port[p]);
    }
  prompt_Printf(arg->prompt, "\n          UDP:    ");
  if (ipcp->cfg.urgent.udp.nports == 0)
    prompt_Printf(arg->prompt, "none");
  else
    for (p = 0; p < ipcp->cfg.urgent.udp.nports; p++) {
      if (p)
        prompt_Printf(arg->prompt, ", ");
      prompt_Printf(arg->prompt, "%u", ipcp->cfg.urgent.udp.port[p]);
    }
  prompt_Printf(arg->prompt, "\n          TOS:    %s\n\n",
                ipcp->cfg.urgent.tos ? "yes" : "no");

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
  static const char * const timer_names[] =
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

  ipcp->cfg.ns.dns[0].s_addr = INADDR_NONE;
  ipcp->cfg.ns.dns[1].s_addr = INADDR_NONE;
  ipcp->cfg.ns.dns_neg = 0;
  ipcp->cfg.ns.nbns[0].s_addr = INADDR_ANY;
  ipcp->cfg.ns.nbns[1].s_addr = INADDR_ANY;

  ipcp->cfg.urgent.tcp.nports = ipcp->cfg.urgent.tcp.maxports = NDEFTCPPORTS;
  ipcp->cfg.urgent.tcp.port = (u_short *)malloc(NDEFTCPPORTS * sizeof(u_short));
  memcpy(ipcp->cfg.urgent.tcp.port, default_urgent_tcp_ports,
         NDEFTCPPORTS * sizeof(u_short));
  ipcp->cfg.urgent.tos = 1;

  ipcp->cfg.urgent.udp.nports = ipcp->cfg.urgent.udp.maxports = NDEFUDPPORTS;
  ipcp->cfg.urgent.udp.port = (u_short *)malloc(NDEFUDPPORTS * sizeof(u_short));
  memcpy(ipcp->cfg.urgent.udp.port, default_urgent_udp_ports,
         NDEFUDPPORTS * sizeof(u_short));

  ipcp->cfg.fsm.timeout = DEF_FSMRETRY;
  ipcp->cfg.fsm.maxreq = DEF_FSMTRIES;
  ipcp->cfg.fsm.maxtrm = DEF_FSMTRIES;
  ipcp->cfg.vj.neg = NEG_ENABLED|NEG_ACCEPTED;

  memset(&ipcp->vj, '\0', sizeof ipcp->vj);

  ipcp->ns.resolv = NULL;
  ipcp->ns.resolv_nons = NULL;
  ipcp->ns.writable = 1;
  ipcp_LoadDNS(ipcp);

  throughput_init(&ipcp->throughput, SAMPLE_PERIOD);
  memset(ipcp->Queue, '\0', sizeof ipcp->Queue);
  ipcp_Setup(ipcp, INADDR_NONE);
}

void
ipcp_Destroy(struct ipcp *ipcp)
{
  if (ipcp->cfg.urgent.tcp.maxports) {
    ipcp->cfg.urgent.tcp.nports = ipcp->cfg.urgent.tcp.maxports = 0;
    free(ipcp->cfg.urgent.tcp.port);
    ipcp->cfg.urgent.tcp.port = NULL;
  }
  if (ipcp->cfg.urgent.udp.maxports) {
    ipcp->cfg.urgent.udp.nports = ipcp->cfg.urgent.udp.maxports = 0;
    free(ipcp->cfg.urgent.udp.port);
    ipcp->cfg.urgent.udp.port = NULL;
  }
  if (ipcp->ns.resolv != NULL) {
    free(ipcp->ns.resolv);
    ipcp->ns.resolv = NULL;
  }
  if (ipcp->ns.resolv_nons != NULL) {
    free(ipcp->ns.resolv_nons);
    ipcp->ns.resolv_nons = NULL;
  }
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
  ipcp->peer_req = 0;
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

  /* Copy startup values into ipcp->dns? */
  if (ipcp->cfg.ns.dns[0].s_addr != INADDR_NONE)
    memcpy(ipcp->dns, ipcp->cfg.ns.dns, sizeof ipcp->dns);
  else if (ipcp->ns.dns[0].s_addr != INADDR_NONE)
    memcpy(ipcp->dns, ipcp->ns.dns, sizeof ipcp->dns);
  else
    ipcp->dns[0].s_addr = ipcp->dns[1].s_addr = INADDR_ANY;

  if (ipcp->dns[1].s_addr == INADDR_NONE)
    ipcp->dns[1] = ipcp->dns[0];
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
  struct in_addr mask, oaddr;

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
    rt_Update(bundle, hisaddr, myaddr);

  if (Enabled(bundle, OPT_SROUTES))
    route_Change(bundle, bundle->ncp.ipcp.route, myaddr, hisaddr,
                 bundle->ncp.ipcp.ns.dns);

#ifndef NORADIUS
  if (bundle->radius.valid)
    route_Change(bundle, bundle->radius.routes, myaddr, hisaddr,
                 bundle->ncp.ipcp.ns.dns);
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
      !REJECTED(ipcp, TY_PRIMARY_DNS - TY_ADJUST_NS)) {
    memcpy(o->data, &ipcp->dns[0].s_addr, 4);
    INC_LCP_OPT(TY_PRIMARY_DNS, 6, o);
  }

  if (IsEnabled(ipcp->cfg.ns.dns_neg) &&
      !REJECTED(ipcp, TY_SECONDARY_DNS - TY_ADJUST_NS)) {
    memcpy(o->data, &ipcp->dns[1].s_addr, 4);
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
  ipcp->peer_req = 0;
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

#ifndef NORADIUS
    radius_Account(&fp->bundle->radius, &fp->bundle->radacct,
                   fp->bundle->links, RAD_STOP, &ipcp->peer_ip, &ipcp->ifmask,
                   &ipcp->throughput);
#endif

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

  if (!iface_SetFlags(ipcp->fsm.bundle->iface->name, IFF_UP)) {
    log_Printf(LogERROR, "ipcp_InterfaceUp: Can't set the IFF_UP flag on %s\n",
               ipcp->fsm.bundle->iface->name);
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

#ifndef NORADIUS
  radius_Account(&fp->bundle->radius, &fp->bundle->radacct, fp->bundle->links, 
                 RAD_START, &ipcp->peer_ip, &ipcp->ifmask, &ipcp->throughput);
#endif

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
ipcp_ValidateReq(struct ipcp *ipcp, struct in_addr ip, struct fsm_decode *dec)
{
  struct bundle *bundle = ipcp->fsm.bundle;
  struct iface *iface = bundle->iface;
  int n;

  if (iplist_isvalid(&ipcp->cfg.peer_list)) {
    if (ip.s_addr == INADDR_ANY ||
        iplist_ip2pos(&ipcp->cfg.peer_list, ip) < 0 ||
        ipcp_SetIPaddress(bundle, ipcp->cfg.my_range.ipaddr, ip, 1)) {
      log_Printf(LogIPCP, "%s: Address invalid or already in use\n",
                 inet_ntoa(ip));
      /*
       * If we've already had a valid address configured for the peer,
       * try NAKing with that so that we don't have to upset things
       * too much.
       */
      for (n = 0; n < iface->in_addrs; n++)
        if (iplist_ip2pos(&ipcp->cfg.peer_list, iface->in_addr[n].brd) >= 0) {
          ipcp->peer_ip = iface->in_addr[n].brd;
          break;
        }

      if (n == iface->in_addrs)
        /* Just pick an IP number from our list */
        ipcp->peer_ip = ChooseHisAddr(bundle, ipcp->cfg.my_range.ipaddr);

      if (ipcp->peer_ip.s_addr == INADDR_ANY) {
        *dec->rejend++ = TY_IPADDR;
        *dec->rejend++ = 6;
        memcpy(dec->rejend, &ip.s_addr, 4);
        dec->rejend += 4;
      } else {
        *dec->nakend++ = TY_IPADDR;
        *dec->nakend++ = 6;
        memcpy(dec->nakend, &ipcp->peer_ip.s_addr, 4);
        dec->nakend += 4;
      }
      return;
    }
  } else if (!AcceptableAddr(&ipcp->cfg.peer_range, ip)) {
    /*
     * If the destination address is not acceptable, NAK with what we
     * want to use.
     */
    *dec->nakend++ = TY_IPADDR;
    *dec->nakend++ = 6;
    for (n = 0; n < iface->in_addrs; n++)
      if ((iface->in_addr[n].brd.s_addr & ipcp->cfg.peer_range.mask.s_addr)
          == (ipcp->cfg.peer_range.ipaddr.s_addr &
              ipcp->cfg.peer_range.mask.s_addr)) {
        /* We prefer the already-configured address */
        memcpy(dec->nakend, &iface->in_addr[n].brd.s_addr, 4);
        break;
      }

    if (n == iface->in_addrs)
      memcpy(dec->nakend, &ipcp->peer_ip.s_addr, 4);

    dec->nakend += 4;
    return;
  }

  ipcp->peer_ip = ip;
  *dec->ackend++ = TY_IPADDR;
  *dec->ackend++ = 6;
  memcpy(dec->ackend, &ip.s_addr, 4);
  dec->ackend += 4;
}

static void
IpcpDecodeConfig(struct fsm *fp, u_char *cp, int plen, int mode_type,
                 struct fsm_decode *dec)
{
  /* Deal with incoming PROTO_IPCP */
  struct ipcp *ipcp = fsm2ipcp(fp);
  int type, length, gotdnsnak;
  u_int32_t compproto;
  struct compreq *pcomp;
  struct in_addr ipaddr, dstipaddr, have_ip;
  char tbuff[100], tbuff2[100];

  gotdnsnak = 0;

  while (plen >= sizeof(struct fsmconfig)) {
    type = *cp;
    length = cp[1];

    if (length == 0) {
      log_Printf(LogIPCP, "%s: IPCP size zero\n", fp->link->name);
      break;
    }

    snprintf(tbuff, sizeof tbuff, " %s[%d] ", protoname(type), length);

    switch (type) {
    case TY_IPADDR:		/* RFC1332 */
      memcpy(&ipaddr.s_addr, cp + 2, 4);
      log_Printf(LogIPCP, "%s %s\n", tbuff, inet_ntoa(ipaddr));

      switch (mode_type) {
      case MODE_REQ:
        ipcp->peer_req = 1;
        ipcp_ValidateReq(ipcp, ipaddr, dec);
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
        have_ip = ipcp->dns[type == TY_PRIMARY_DNS ? 0 : 1];

        if (type == TY_PRIMARY_DNS && ipaddr.s_addr != have_ip.s_addr &&
            ipaddr.s_addr == ipcp->dns[1].s_addr) {
          /* Swap 'em 'round */
          ipcp->dns[0] = ipcp->dns[1];
          ipcp->dns[1] = have_ip;
          have_ip = ipcp->dns[0];
        }

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

      case MODE_NAK:
        if (IsEnabled(ipcp->cfg.ns.dns_neg)) {
          gotdnsnak = 1;
          memcpy(&ipcp->dns[type == TY_PRIMARY_DNS ? 0 : 1].s_addr, cp + 2, 4);
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

  if (gotdnsnak) {
    memcpy(ipcp->ns.dns, ipcp->dns, sizeof ipcp->ns.dns);
    if (ipcp->ns.writable) {
      log_Printf(LogDEBUG, "Updating resolver\n");
      if (!ipcp_WriteDNS(ipcp)) {
        ipcp->peer_reject |= (1 << (TY_PRIMARY_DNS - TY_ADJUST_NS));
        ipcp->peer_reject |= (1 << (TY_SECONDARY_DNS - TY_ADJUST_NS));
      } else
        bundle_AdjustDNS(fp->bundle, ipcp->dns);
    } else {
      log_Printf(LogDEBUG, "Not updating resolver (readonly)\n");
      bundle_AdjustDNS(fp->bundle, ipcp->dns);
    }
  }

  if (mode_type != MODE_NOP) {
    if (mode_type == MODE_REQ && !ipcp->peer_req) {
      if (dec->rejend == dec->rej && dec->nakend == dec->nak) {
        /*
         * Pretend the peer has requested an IP.
         * We do this to ensure that we only send one NAK if the only
         * reason for the NAK is because the peer isn't sending a
         * TY_IPADDR REQ.  This stops us from repeatedly trying to tell
         * the peer that we have to have an IP address on their end.
         */
        ipcp->peer_req = 1;
      }
      ipaddr.s_addr = INADDR_ANY;
      ipcp_ValidateReq(ipcp, ipaddr, dec);
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
ipcp_Input(struct bundle *bundle, struct link *l, struct mbuf *bp)
{
  /* Got PROTO_IPCP from link */
  m_settype(bp, MB_IPCPIN);
  if (bundle_Phase(bundle) == PHASE_NETWORK)
    fsm_Input(&bundle->ncp.ipcp.fsm, bp);
  else {
    if (bundle_Phase(bundle) < PHASE_NETWORK)
      log_Printf(LogIPCP, "%s: Error: Unexpected IPCP in phase %s (ignored)\n",
                 l->name, bundle_PhaseName(bundle));
    m_freem(bp);
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

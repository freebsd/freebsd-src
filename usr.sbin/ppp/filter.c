/*
 *		PPP Filter command Interface
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
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD: src/usr.sbin/ppp/filter.c,v 1.39.2.3 2000/08/19 09:30:03 brian Exp $
 *
 *	TODO: Should send ICMP error message when we discard packets.
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <termios.h>

#include "layer.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "iplist.h"
#include "timer.h"
#include "throughput.h"
#include "lqr.h"
#include "hdlc.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "prompt.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"

static int filter_Nam2Proto(int, char const *const *);
static int filter_Nam2Op(const char *);

static const u_int32_t netmasks[33] = {
  0x00000000,
  0x80000000, 0xC0000000, 0xE0000000, 0xF0000000,
  0xF8000000, 0xFC000000, 0xFE000000, 0xFF000000,
  0xFF800000, 0xFFC00000, 0xFFE00000, 0xFFF00000,
  0xFFF80000, 0xFFFC0000, 0xFFFE0000, 0xFFFF0000,
  0xFFFF8000, 0xFFFFC000, 0xFFFFE000, 0xFFFFF000,
  0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00, 0xFFFFFF00,
  0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0, 0xFFFFFFF0,
  0xFFFFFFF8, 0xFFFFFFFC, 0xFFFFFFFE, 0xFFFFFFFF,
};

struct in_addr
bits2mask(int bits)
{
  struct in_addr result;

  result.s_addr = htonl(netmasks[bits]);
  return result;
}

int
ParseAddr(struct ipcp *ipcp, const char *data,
	  struct in_addr *paddr, struct in_addr *pmask, int *pwidth)
{
  int bits, len;
  char *wp;
  const char *cp;

  if (pmask)
    pmask->s_addr = INADDR_BROADCAST;	/* Assume 255.255.255.255 as default */

  cp = pmask || pwidth ? strchr(data, '/') : NULL;
  len = cp ? cp - data : strlen(data);

  if (ipcp && strncasecmp(data, "HISADDR", len) == 0)
    *paddr = ipcp->peer_ip;
  else if (ipcp && strncasecmp(data, "MYADDR", len) == 0)
    *paddr = ipcp->my_ip;
  else if (ipcp && strncasecmp(data, "DNS0", len) == 0)
    *paddr = ipcp->ns.dns[0];
  else if (ipcp && strncasecmp(data, "DNS1", len) == 0)
    *paddr = ipcp->ns.dns[1];
  else {
    char *s;

    s = (char *)alloca(len + 1);
    strncpy(s, data, len);
    s[len] = '\0';
    *paddr = GetIpAddr(s);
    if (paddr->s_addr == INADDR_NONE) {
      log_Printf(LogWARN, "ParseAddr: %s: Bad address\n", s);
      return 0;
    }
  }
  if (cp && *++cp) {
    bits = strtol(cp, &wp, 0);
    if (cp == wp || bits < 0 || bits > 32) {
      log_Printf(LogWARN, "ParseAddr: bad mask width.\n");
      return 0;
    }
  } else if (paddr->s_addr == INADDR_ANY)
    /* An IP of 0.0.0.0 without a width is anything */
    bits = 0;
  else
    /* If a valid IP is given without a width, assume 32 bits */
    bits = 32;

  if (pwidth)
    *pwidth = bits;

  if (pmask) {
    if (paddr->s_addr == INADDR_ANY)
      pmask->s_addr = INADDR_ANY;
    else
      *pmask = bits2mask(bits);
  }

  return 1;
}

static int
ParsePort(const char *service, int proto)
{
  const char *protocol_name;
  char *cp;
  struct servent *servent;
  int port;

  switch (proto) {
  case P_UDP:
    protocol_name = "udp";
    break;
  case P_TCP:
    protocol_name = "tcp";
    break;
  default:
    protocol_name = 0;
  }

  servent = getservbyname(service, protocol_name);
  if (servent != 0)
    return ntohs(servent->s_port);

  port = strtol(service, &cp, 0);
  if (cp == service) {
    log_Printf(LogWARN, "ParsePort: %s is not a port name or number.\n",
	      service);
    return 0;
  }
  return port;
}

/*
 *	ICMP Syntax:	src eq icmp_message_type
 */
static int
ParseIcmp(int argc, char const *const *argv, struct filterent *tgt)
{
  int type;
  char *cp;

  switch (argc) {
  case 0:
    /* permit/deny all ICMP types */
    tgt->f_srcop = OP_NONE;
    break;

  case 3:
    if (!strcmp(*argv, "src") && !strcmp(argv[1], "eq")) {
      type = strtol(argv[2], &cp, 0);
      if (cp == argv[2]) {
	log_Printf(LogWARN, "ParseIcmp: type is expected.\n");
	return 0;
      }
      tgt->f_srcop = OP_EQ;
      tgt->f_srcport = type;
    }
    break;

  default:
    log_Printf(LogWARN, "ParseIcmp: bad icmp syntax.\n");
    return 0;
  }
  return 1;
}

/*
 *	UDP Syntax: [src op port] [dst op port]
 */
static int
ParseUdpOrTcp(int argc, char const *const *argv, int proto,
              struct filterent *tgt)
{
  tgt->f_srcop = tgt->f_dstop = OP_NONE;
  tgt->f_estab = tgt->f_syn = tgt->f_finrst = 0;

  if (argc >= 3 && !strcmp(*argv, "src")) {
    tgt->f_srcop = filter_Nam2Op(argv[1]);
    if (tgt->f_srcop == OP_NONE) {
      log_Printf(LogWARN, "ParseUdpOrTcp: bad operation\n");
      return 0;
    }
    tgt->f_srcport = ParsePort(argv[2], proto);
    if (tgt->f_srcport == 0)
      return 0;
    argc -= 3;
    argv += 3;
  }

  if (argc >= 3 && !strcmp(argv[0], "dst")) {
    tgt->f_dstop = filter_Nam2Op(argv[1]);
    if (tgt->f_dstop == OP_NONE) {
      log_Printf(LogWARN, "ParseUdpOrTcp: bad operation\n");
      return 0;
    }
    tgt->f_dstport = ParsePort(argv[2], proto);
    if (tgt->f_dstport == 0)
      return 0;
    argc -= 3;
    argv += 3;
  }

  if (proto == P_TCP) {
    for (; argc > 0; argc--, argv++)
      if (!strcmp(*argv, "estab"))
        tgt->f_estab = 1;
      else if (!strcmp(*argv, "syn"))
        tgt->f_syn = 1;
      else if (!strcmp(*argv, "finrst"))
        tgt->f_finrst = 1;
      else
        break;
  }

  if (argc > 0) {
    log_Printf(LogWARN, "ParseUdpOrTcp: bad src/dst port syntax: %s\n", *argv);
    return 0;
  }

  return 1;
}

static int
ParseIgmp(int argc, char const * const *argv, struct filterent *tgt)
{
  /*
   * Filter currently is a catch-all. Requests are either permitted or
   * dropped.
   */
  if (argc != 0) {
    log_Printf(LogWARN, "ParseIgmp: Too many parameters\n");
    return 0;
  } else
    tgt->f_srcop = OP_NONE;

  return 1;
}

#ifdef P_GRE
static int
ParseGRE(int argc, char const * const *argv, struct filterent *tgt)
{
  /*
   * Filter currently is a catch-all. Requests are either permitted or
   * dropped.
   */
  if (argc != 0) {
    log_Printf(LogWARN, "ParseGRE: Too many parameters\n");
    return 0;
  } else
    tgt->f_srcop = OP_NONE;

  return 1;
}
#endif

#ifdef P_OSPF
static int
ParseOspf(int argc, char const * const *argv, struct filterent *tgt)
{
  /*
   * Filter currently is a catch-all. Requests are either permitted or
   * dropped.
   */
  if (argc != 0) {
    log_Printf(LogWARN, "ParseOspf: Too many parameters\n");
    return 0;
  } else
    tgt->f_srcop = OP_NONE;

  return 1;
}
#endif

static unsigned
addrtype(const char *addr)
{
  if (!strncasecmp(addr, "MYADDR", 6) && (addr[6] == '\0' || addr[6] == '/'))
    return T_MYADDR;
  if (!strncasecmp(addr, "HISADDR", 7) && (addr[7] == '\0' || addr[7] == '/'))
    return T_HISADDR;
  if (!strncasecmp(addr, "DNS0", 4) && (addr[4] == '\0' || addr[4] == '/'))
    return T_DNS0;
  if (!strncasecmp(addr, "DNS1", 4) && (addr[4] == '\0' || addr[4] == '/'))
    return T_DNS1;

  return T_ADDR;
}

static const char *
addrstr(struct in_addr addr, unsigned type)
{
  switch (type) {
    case T_MYADDR:
      return "MYADDR";
    case T_HISADDR:
      return "HISADDR";
    case T_DNS0:
      return "DNS0";
    case T_DNS1:
      return "DNS1";
  }
  return inet_ntoa(addr);
}

static const char *
maskstr(int bits)
{
  static char str[4];

  if (bits == 32)
    *str = '\0';
  else
    snprintf(str, sizeof str, "/%d", bits);

  return str;
}

static int
Parse(struct ipcp *ipcp, int argc, char const *const *argv,
      struct filterent *ofp)
{
  int action, proto;
  int val, ruleno;
  char *wp;
  struct filterent filterdata;

  ruleno = strtol(*argv, &wp, 0);
  if (*argv == wp || ruleno >= MAXFILTERS) {
    log_Printf(LogWARN, "Parse: invalid filter number.\n");
    return 0;
  }
  if (ruleno < 0) {
    for (ruleno = 0; ruleno < MAXFILTERS; ruleno++) {
      ofp->f_action = A_NONE;
      ofp++;
    }
    log_Printf(LogWARN, "Parse: filter cleared.\n");
    return 1;
  }
  ofp += ruleno;

  if (--argc == 0) {
    log_Printf(LogWARN, "Parse: missing action.\n");
    return 0;
  }
  argv++;

  proto = P_NONE;
  memset(&filterdata, '\0', sizeof filterdata);

  val = strtol(*argv, &wp, 0);
  if (!*wp && val >= 0 && val < MAXFILTERS) {
    if (val <= ruleno) {
      log_Printf(LogWARN, "Parse: Can only jump forward from rule %d\n",
                 ruleno);
      return 0;
    }
    action = val;
  } else if (!strcmp(*argv, "permit")) {
    action = A_PERMIT;
  } else if (!strcmp(*argv, "deny")) {
    action = A_DENY;
  } else if (!strcmp(*argv, "clear")) {
    ofp->f_action = A_NONE;
    return 1;
  } else {
    log_Printf(LogWARN, "Parse: bad action: %s\n", *argv);
    return 0;
  }
  filterdata.f_action = action;

  argc--;
  argv++;

  if (argc && argv[0][0] == '!' && !argv[0][1]) {
    filterdata.f_invert = 1;
    argc--;
    argv++;
  }

  proto = filter_Nam2Proto(argc, argv);
  if (proto == P_NONE) {
    if (!argc)
      log_Printf(LogWARN, "Parse: address/mask is expected.\n");
    else if (ParseAddr(ipcp, *argv, &filterdata.f_src.ipaddr,
                       &filterdata.f_src.mask, &filterdata.f_src.width)) {
      filterdata.f_srctype = addrtype(*argv);
      argc--;
      argv++;
      proto = filter_Nam2Proto(argc, argv);
      if (!argc)
        log_Printf(LogWARN, "Parse: address/mask is expected.\n");
      else if (proto == P_NONE) {
	if (ParseAddr(ipcp, *argv, &filterdata.f_dst.ipaddr,
		      &filterdata.f_dst.mask, &filterdata.f_dst.width)) {
          filterdata.f_dsttype = addrtype(*argv);
	  argc--;
	  argv++;
	} else
          filterdata.f_dsttype = T_ADDR;
        if (argc) {
	  proto = filter_Nam2Proto(argc, argv);
	  if (proto == P_NONE) {
            log_Printf(LogWARN, "Parse: %s: Invalid protocol\n", *argv);
            return 0;
          } else {
	    argc--;
	    argv++;
	  }
	}
      } else {
	argc--;
	argv++;
      }
    } else {
      log_Printf(LogWARN, "Parse: Address/protocol expected.\n");
      return 0;
    }
  } else {
    argc--;
    argv++;
  }

  if (argc >= 2 && strcmp(argv[argc - 2], "timeout") == 0) {
    filterdata.timeout = strtoul(argv[argc - 1], NULL, 10);
    argc -= 2;
  }

  val = 1;
  filterdata.f_proto = proto;

  switch (proto) {
  case P_TCP:
    val = ParseUdpOrTcp(argc, argv, P_TCP, &filterdata);
    break;
  case P_UDP:
    val = ParseUdpOrTcp(argc, argv, P_UDP, &filterdata);
    break;
  case P_ICMP:
    val = ParseIcmp(argc, argv, &filterdata);
    break;
  case P_IGMP:
    val = ParseIgmp(argc, argv, &filterdata);
    break;
#ifdef P_OSPF
  case P_OSPF:
    val = ParseOspf(argc, argv, &filterdata);
    break;
#endif
#ifdef P_GRE
  case P_GRE:
    val = ParseGRE(argc, argv, &filterdata);
    break;
#endif
  }

  log_Printf(LogDEBUG, "Parse: Src: %s\n", inet_ntoa(filterdata.f_src.ipaddr));
  log_Printf(LogDEBUG, "Parse: Src mask: %s\n", inet_ntoa(filterdata.f_src.mask));
  log_Printf(LogDEBUG, "Parse: Dst: %s\n", inet_ntoa(filterdata.f_dst.ipaddr));
  log_Printf(LogDEBUG, "Parse: Dst mask: %s\n", inet_ntoa(filterdata.f_dst.mask));
  log_Printf(LogDEBUG, "Parse: Proto = %d\n", proto);

  log_Printf(LogDEBUG, "Parse: src:  %s (%d)\n",
            filter_Op2Nam(filterdata.f_srcop), filterdata.f_srcport);
  log_Printf(LogDEBUG, "Parse: dst:  %s (%d)\n",
            filter_Op2Nam(filterdata.f_dstop), filterdata.f_dstport);
  log_Printf(LogDEBUG, "Parse: estab: %u\n", filterdata.f_estab);
  log_Printf(LogDEBUG, "Parse: syn: %u\n", filterdata.f_syn);
  log_Printf(LogDEBUG, "Parse: finrst: %u\n", filterdata.f_finrst);

  if (val)
    *ofp = filterdata;

  return val;
}

int
filter_Set(struct cmdargs const *arg)
{
  struct filter *filter;

  if (arg->argc < arg->argn+2)
    return -1;

  if (!strcmp(arg->argv[arg->argn], "in"))
    filter = &arg->bundle->filter.in;
  else if (!strcmp(arg->argv[arg->argn], "out"))
    filter = &arg->bundle->filter.out;
  else if (!strcmp(arg->argv[arg->argn], "dial"))
    filter = &arg->bundle->filter.dial;
  else if (!strcmp(arg->argv[arg->argn], "alive"))
    filter = &arg->bundle->filter.alive;
  else {
    log_Printf(LogWARN, "filter_Set: %s: Invalid filter name.\n",
              arg->argv[arg->argn]);
    return -1;
  }

  Parse(&arg->bundle->ncp.ipcp, arg->argc - arg->argn - 1,
        arg->argv + arg->argn + 1, filter->rule);
  return 0;
}

const char *
filter_Action2Nam(int act)
{
  static const char * const actname[] = { "  none ", "permit ", "  deny " };
  static char	buf[8];

  if (act >= 0 && act < MAXFILTERS) {
    snprintf(buf, sizeof buf, "%6d ", act);
    return buf;
  } else if (act >= A_NONE && act < A_NONE + sizeof(actname)/sizeof(char *))
    return actname[act - A_NONE];
  else
    return "?????? ";
}

static void
doShowFilter(struct filterent *fp, struct prompt *prompt)
{
  int n;

  for (n = 0; n < MAXFILTERS; n++, fp++) {
    if (fp->f_action != A_NONE) {
      prompt_Printf(prompt, "  %2d %s", n, filter_Action2Nam(fp->f_action));
      prompt_Printf(prompt, "%c ", fp->f_invert ? '!' : ' ');
      prompt_Printf(prompt, "%s%s ", addrstr(fp->f_src.ipaddr, fp->f_srctype),
                    maskstr(fp->f_src.width));
      prompt_Printf(prompt, "%s%s ", addrstr(fp->f_dst.ipaddr, fp->f_dsttype),
                    maskstr(fp->f_dst.width));
      if (fp->f_proto) {
	prompt_Printf(prompt, "%s", filter_Proto2Nam(fp->f_proto));

	if (fp->f_srcop)
	  prompt_Printf(prompt, " src %s %d", filter_Op2Nam(fp->f_srcop),
		  fp->f_srcport);
	if (fp->f_dstop)
	  prompt_Printf(prompt, " dst %s %d", filter_Op2Nam(fp->f_dstop),
		  fp->f_dstport);
	if (fp->f_estab)
	  prompt_Printf(prompt, " estab");
	if (fp->f_syn)
	  prompt_Printf(prompt, " syn");
	if (fp->f_finrst)
	  prompt_Printf(prompt, " finrst");
      }
      if (fp->timeout != 0)
	  prompt_Printf(prompt, " timeout %u", fp->timeout);
      prompt_Printf(prompt, "\n");
    }
  }
}

int
filter_Show(struct cmdargs const *arg)
{
  if (arg->argc > arg->argn+1)
    return -1;

  if (arg->argc == arg->argn+1) {
    struct filter *filter;

    if (!strcmp(arg->argv[arg->argn], "in"))
      filter = &arg->bundle->filter.in;
    else if (!strcmp(arg->argv[arg->argn], "out"))
      filter = &arg->bundle->filter.out;
    else if (!strcmp(arg->argv[arg->argn], "dial"))
      filter = &arg->bundle->filter.dial;
    else if (!strcmp(arg->argv[arg->argn], "alive"))
      filter = &arg->bundle->filter.alive;
    else
      return -1;
    doShowFilter(filter->rule, arg->prompt);
  } else {
    struct filter *filter[4];
    int f;

    filter[0] = &arg->bundle->filter.in;
    filter[1] = &arg->bundle->filter.out;
    filter[2] = &arg->bundle->filter.dial;
    filter[3] = &arg->bundle->filter.alive;
    for (f = 0; f < 4; f++) {
      if (f)
        prompt_Printf(arg->prompt, "\n");
      prompt_Printf(arg->prompt, "%s:\n", filter[f]->name);
      doShowFilter(filter[f]->rule, arg->prompt);
    }
  }

  return 0;
}

static const char * const protoname[] = {
  "none", "tcp", "udp", "icmp", "ospf", "igmp", "gre"
};

const char *
filter_Proto2Nam(int proto)
{
  if (proto >= sizeof protoname / sizeof protoname[0])
    return "unknown";
  return protoname[proto];
}

static int
filter_Nam2Proto(int argc, char const *const *argv)
{
  int proto;

  if (argc == 0)
    proto = 0;
  else
    for (proto = sizeof protoname / sizeof protoname[0] - 1; proto; proto--)
      if (!strcasecmp(*argv, protoname[proto]))
        break;

  return proto;
}

static const char * const opname[] = {"none", "eq", "gt", "lt"};

const char *
filter_Op2Nam(int op)
{
  if (op >= sizeof opname / sizeof opname[0])
    return "unknown";
  return opname[op];

}

static int
filter_Nam2Op(const char *cp)
{
  int op;

  for (op = sizeof opname / sizeof opname[0] - 1; op; op--)
    if (!strcasecmp(cp, opname[op]))
      break;

  return op;
}

void
filter_AdjustAddr(struct filter *filter, struct in_addr *my_ip,
                  struct in_addr *peer_ip, struct in_addr dns[2])
{
  struct filterent *fp;
  int n;

  for (fp = filter->rule, n = 0; n < MAXFILTERS; fp++, n++)
    if (fp->f_action != A_NONE) {
      if (my_ip) {
        if (fp->f_srctype == T_MYADDR)
          fp->f_src.ipaddr = *my_ip;
        if (fp->f_dsttype == T_MYADDR)
          fp->f_dst.ipaddr = *my_ip;
      }
      if (peer_ip) {
        if (fp->f_srctype == T_HISADDR)
          fp->f_src.ipaddr = *peer_ip;
        if (fp->f_dsttype == T_HISADDR)
          fp->f_dst.ipaddr = *peer_ip;
      }
      if (dns) {
        if (fp->f_srctype == T_DNS0)
          fp->f_src.ipaddr = dns[0];
        if (fp->f_dsttype == T_DNS0)
          fp->f_dst.ipaddr = dns[0];
        if (fp->f_srctype == T_DNS1)
          fp->f_src.ipaddr = dns[1];
        if (fp->f_dsttype == T_DNS1)
          fp->f_dst.ipaddr = dns[1];
      }
    }
}

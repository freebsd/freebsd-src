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
 * $Id: filter.c,v 1.23 1998/05/21 21:45:13 brian Exp $
 *
 *	TODO: Shoud send ICMP error message when we discard packets.
 */

#include <sys/types.h>
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
#include "bundle.h"

static int filter_Nam2Proto(int, char const *const *);
static int filter_Nam2Op(const char *);

static const u_long netmasks[33] = {
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

int
ParseAddr(struct ipcp *ipcp, int argc, char const *const *argv,
	  struct in_addr *paddr, struct in_addr *pmask, int *pwidth)
{
  int bits, len;
  char *wp;
  const char *cp;

  if (argc < 1) {
    log_Printf(LogWARN, "ParseAddr: address/mask is expected.\n");
    return (0);
  }

  if (pmask)
    pmask->s_addr = INADDR_BROADCAST;	/* Assume 255.255.255.255 as default */

  cp = pmask || pwidth ? strchr(*argv, '/') : NULL;
  len = cp ? cp - *argv : strlen(*argv);

  if (strncasecmp(*argv, "HISADDR", len) == 0)
    *paddr = ipcp->peer_ip;
  else if (strncasecmp(*argv, "MYADDR", len) == 0)
    *paddr = ipcp->my_ip;
  else if (len > 15)
    log_Printf(LogWARN, "ParseAddr: %s: Bad address\n", *argv);
  else {
    char s[16];
    strncpy(s, *argv, len);
    s[len] = '\0';
    if (inet_aton(s, paddr) == 0) {
      log_Printf(LogWARN, "ParseAddr: %s: Bad address\n", s);
      return (0);
    }
  }
  if (cp && *++cp) {
    bits = strtol(cp, &wp, 0);
    if (cp == wp || bits < 0 || bits > 32) {
      log_Printf(LogWARN, "ParseAddr: bad mask width.\n");
      return (0);
    }
  } else if (paddr->s_addr == INADDR_ANY)
    /* An IP of 0.0.0.0 without a width is anything */
    bits = 0;
  else
    /* If a valid IP is given without a width, assume 32 bits */
    bits = 32;

  if (pwidth)
    *pwidth = bits;

  if (pmask)
    pmask->s_addr = htonl(netmasks[bits]);

  return (1);
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
    return (ntohs(servent->s_port));

  port = strtol(service, &cp, 0);
  if (cp == service) {
    log_Printf(LogWARN, "ParsePort: %s is not a port name or number.\n",
	      service);
    return (0);
  }
  return (port);
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
    tgt->opt.srcop = OP_NONE;
    break;

  case 3:
    if (!strcmp(*argv, "src") && !strcmp(argv[1], "eq")) {
      type = strtol(argv[2], &cp, 0);
      if (cp == argv[2]) {
	log_Printf(LogWARN, "ParseIcmp: type is expected.\n");
	return (0);
      }
      tgt->opt.srcop = OP_EQ;
      tgt->opt.srcport = type;
    }
    break;

  default:
    log_Printf(LogWARN, "ParseIcmp: bad icmp syntax.\n");
    return (0);
  }
  return (1);
}

/*
 *	UDP Syntax: [src op port] [dst op port]
 */
static int
ParseUdpOrTcp(int argc, char const *const *argv, int proto,
              struct filterent *tgt)
{
  tgt->opt.srcop = tgt->opt.dstop = OP_NONE;
  tgt->opt.estab = tgt->opt.syn = tgt->opt.finrst = 0;

  if (argc >= 3 && !strcmp(*argv, "src")) {
    tgt->opt.srcop = filter_Nam2Op(argv[1]);
    if (tgt->opt.srcop == OP_NONE) {
      log_Printf(LogWARN, "ParseUdpOrTcp: bad operation\n");
      return (0);
    }
    tgt->opt.srcport = ParsePort(argv[2], proto);
    if (tgt->opt.srcport == 0)
      return (0);
    argc -= 3;
    argv += 3;
  }

  if (argc >= 3 && !strcmp(argv[0], "dst")) {
    tgt->opt.dstop = filter_Nam2Op(argv[1]);
    if (tgt->opt.dstop == OP_NONE) {
      log_Printf(LogWARN, "ParseUdpOrTcp: bad operation\n");
      return (0);
    }
    tgt->opt.dstport = ParsePort(argv[2], proto);
    if (tgt->opt.dstport == 0)
      return (0);
    argc -= 3;
    argv += 3;
  }

  if (proto == P_TCP) {
    for (; argc > 0; argc--, argv++)
      if (!strcmp(*argv, "estab"))
        tgt->opt.estab = 1;
      else if (!strcmp(*argv, "syn"))
        tgt->opt.syn = 1;
      else if (!strcmp(*argv, "finrst"))
        tgt->opt.finrst = 1;
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
Parse(struct ipcp *ipcp, int argc, char const *const *argv,
      struct filterent *ofp)
{
  int action, proto;
  int val;
  char *wp;
  struct filterent filterdata;

  val = strtol(*argv, &wp, 0);
  if (*argv == wp || val > MAXFILTERS) {
    log_Printf(LogWARN, "Parse: invalid filter number.\n");
    return (0);
  }
  if (val < 0) {
    for (val = 0; val < MAXFILTERS; val++) {
      ofp->action = A_NONE;
      ofp++;
    }
    log_Printf(LogWARN, "Parse: filter cleared.\n");
    return (1);
  }
  ofp += val;

  if (--argc == 0) {
    log_Printf(LogWARN, "Parse: missing action.\n");
    return (0);
  }
  argv++;

  proto = P_NONE;
  memset(&filterdata, '\0', sizeof filterdata);

  if (!strcmp(*argv, "permit")) {
    action = A_PERMIT;
  } else if (!strcmp(*argv, "deny")) {
    action = A_DENY;
  } else if (!strcmp(*argv, "clear")) {
    ofp->action = A_NONE;
    return (1);
  } else {
    log_Printf(LogWARN, "Parse: bad action: %s\n", *argv);
    return (0);
  }
  filterdata.action = action;

  argc--;
  argv++;

  if (filterdata.action == A_DENY) {
    if (!strcmp(*argv, "host")) {
      filterdata.action |= A_UHOST;
      argc--;
      argv++;
    } else if (!strcmp(*argv, "port")) {
      filterdata.action |= A_UPORT;
      argc--;
      argv++;
    }
  }
  proto = filter_Nam2Proto(argc, argv);
  if (proto == P_NONE) {
    if (ParseAddr(ipcp, argc, argv, &filterdata.saddr, &filterdata.smask,
                  &filterdata.swidth)) {
      argc--;
      argv++;
      proto = filter_Nam2Proto(argc, argv);
      if (proto == P_NONE) {
	if (ParseAddr(ipcp, argc, argv, &filterdata.daddr, &filterdata.dmask,
                      &filterdata.dwidth)) {
	  argc--;
	  argv++;
	}
	proto = filter_Nam2Proto(argc, argv);
	if (proto != P_NONE) {
	  argc--;
	  argv++;
	}
      } else {
	argc--;
	argv++;
      }
    } else {
      log_Printf(LogWARN, "Parse: Address/protocol expected.\n");
      return (0);
    }
  } else {
    argc--;
    argv++;
  }

  val = 1;
  filterdata.proto = proto;

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
  }

  log_Printf(LogDEBUG, "Parse: Src: %s\n", inet_ntoa(filterdata.saddr));
  log_Printf(LogDEBUG, "Parse: Src mask: %s\n", inet_ntoa(filterdata.smask));
  log_Printf(LogDEBUG, "Parse: Dst: %s\n", inet_ntoa(filterdata.daddr));
  log_Printf(LogDEBUG, "Parse: Dst mask: %s\n", inet_ntoa(filterdata.dmask));
  log_Printf(LogDEBUG, "Parse: Proto = %d\n", proto);

  log_Printf(LogDEBUG, "Parse: src:  %s (%d)\n",
            filter_Op2Nam(filterdata.opt.srcop), filterdata.opt.srcport);
  log_Printf(LogDEBUG, "Parse: dst:  %s (%d)\n",
            filter_Op2Nam(filterdata.opt.dstop), filterdata.opt.dstport);
  log_Printf(LogDEBUG, "Parse: estab: %u\n", filterdata.opt.estab);
  log_Printf(LogDEBUG, "Parse: syn: %u\n", filterdata.opt.syn);
  log_Printf(LogDEBUG, "Parse: finrst: %u\n", filterdata.opt.finrst);

  if (val)
    *ofp = filterdata;
  return (val);
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
  static const char *actname[] = { "none   ", "permit ", "deny   " };
  return actname[act & (A_PERMIT|A_DENY)];
}

static void
doShowFilter(struct filterent *fp, struct prompt *prompt)
{
  int n;

  for (n = 0; n < MAXFILTERS; n++, fp++) {
    if (fp->action != A_NONE) {
      prompt_Printf(prompt, "  %2d %s", n, filter_Action2Nam(fp->action));
      if (fp->action & A_UHOST)
        prompt_Printf(prompt, "host ");
      else if (fp->action & A_UPORT)
        prompt_Printf(prompt, "port ");
      else
        prompt_Printf(prompt, "     ");
      prompt_Printf(prompt, "%s/%d ", inet_ntoa(fp->saddr), fp->swidth);
      prompt_Printf(prompt, "%s/%d ", inet_ntoa(fp->daddr), fp->dwidth);
      if (fp->proto) {
	prompt_Printf(prompt, "%s", filter_Proto2Nam(fp->proto));

	if (fp->opt.srcop)
	  prompt_Printf(prompt, " src %s %d", filter_Op2Nam(fp->opt.srcop),
		  fp->opt.srcport);
	if (fp->opt.dstop)
	  prompt_Printf(prompt, " dst %s %d", filter_Op2Nam(fp->opt.dstop),
		  fp->opt.dstport);
	if (fp->opt.estab)
	  prompt_Printf(prompt, " estab");
	if (fp->opt.syn)
	  prompt_Printf(prompt, " syn");
	if (fp->opt.finrst)
	  prompt_Printf(prompt, " finrst");
      }
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

static const char *protoname[] = { "none", "tcp", "udp", "icmp" };

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

static const char *opname[] = {"none", "eq", "gt", "unknown", "lt"};

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

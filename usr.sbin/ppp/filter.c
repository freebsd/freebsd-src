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
 * $Id: filter.c,v 1.9 1997/05/10 01:22:08 brian Exp $
 *
 *	TODO: Shoud send ICMP error message when we discard packets.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "filter.h"
#include "loadalias.h"
#include "vars.h"

static struct filterent filterdata;

static u_long netmasks[33] = {
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
ParseAddr(argc, argv, paddr, pmask, pwidth)
int argc;
char **argv;
struct in_addr *paddr;
struct in_addr *pmask;
int *pwidth;
{
  u_long addr;
  int bits;
  char *cp, *wp;

  if (argc < 1) {
    LogPrintf(LogWARN, "ParseAddr: address/mask is expected.\n");
    return(0);
  }

  pmask->s_addr = 0xffffffff;	/* Assume 255.255.255.255 as default */
  cp = index(*argv, '/');
  if (cp) *cp++ = '\0';
  addr = inet_addr(*argv);
  paddr->s_addr = addr;
  if (cp && *cp) {
    bits = strtol(cp, &wp, 0);
    if (cp == wp || bits < 0 || bits > 32) {
      LogPrintf(LogWARN, "ParseAddr: bad mask width.\n");
      return(0);
    }
  } else {
    /* if width is not given, assume whole 32 bits are meaningfull */
    bits = 32;
  }

  *pwidth = bits;
  pmask->s_addr = htonl(netmasks[bits]);

  return(1);
}

static int
ParseProto(argc, argv)
int argc;
char **argv;
{
  int proto;

  if (argc < 1)
    return(P_NONE);

  if (STREQ(*argv, "tcp"))
    proto = P_TCP;
  else if (STREQ(*argv, "udp"))
    proto = P_UDP;
  else if (STREQ(*argv, "icmp"))
    proto = P_ICMP;
  else
    proto = P_NONE;
  return(proto);
}

static int
ParsePort(service, proto)
char *service;
int proto;
{
  char *protocol_name, *cp;
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

  servent = getservbyname (service, protocol_name);
  if (servent != 0)
    return(ntohs(servent->s_port));

  port = strtol(service, &cp, 0);
  if (cp == service) {
    LogPrintf(LogWARN, "ParsePort: %s is not a port name or number.\n",
              service);
    return(0);
  }
  return(port);
}

/*
 *	ICMP Syntax:	src eq icmp_message_type
 */
static int
ParseIcmp(argc, argv)
int argc;
char **argv;
{
  int type;
  char *cp;

  switch (argc) {
  case 0:
    /* permit/deny all ICMP types */
    filterdata.opt.srcop = OP_NONE;
    break;
  default:
    LogPrintf(LogWARN, "ParseIcmp: bad icmp syntax.\n");
    return(0);
  case 3:
    if (STREQ(*argv, "src") && STREQ(argv[1], "eq")) {
      type = strtol(argv[2], &cp, 0);
      if (cp == argv[2]) {
	LogPrintf(LogWARN, "ParseIcmp: type is expected.\n");
	return(0);
      }
      filterdata.opt.srcop = OP_EQ;
      filterdata.opt.srcport = type;
    }
    break;
  }
  return(1);
}

static int
ParseOp(cp)
char *cp;
{
  int op = OP_NONE;

  if (STREQ(cp, "eq"))
    op = OP_EQ;
  else if (STREQ(cp, "gt"))
    op = OP_GT;
  else if (STREQ(cp, "lt"))
    op = OP_LT;
  return(op);
}

/*
 *	UDP Syntax: [src op port] [dst op port]
 */
static int
ParseUdpOrTcp(argc, argv, proto)
int argc;
char **argv;
int proto;
{

  if (argc == 0) {
    /* permit/deny all tcp traffic */
    filterdata.opt.srcop = filterdata.opt.dstop = A_NONE;
    return(1);
  }
  if (argc < 3) {
    LogPrintf(LogWARN, "ParseUdpOrTcp: bad udp/tcp syntax.\n");
    return(0);
  }
  if (argc >= 3 && STREQ(*argv, "src")) {
    filterdata.opt.srcop = ParseOp(argv[1]);
    if (filterdata.opt.srcop == OP_NONE) {
      LogPrintf(LogWARN, "ParseUdpOrTcp: bad operation\n");
      return(0);
    }
    filterdata.opt.srcport = ParsePort(argv[2], proto);
    if (filterdata.opt.srcport == 0)
      return(0);
    argc -= 3; argv += 3;
    if (argc == 0)
      return(1);
  }
  if (argc >= 3 && STREQ(argv[0], "dst")) {
    filterdata.opt.dstop = ParseOp(argv[1]);
    if (filterdata.opt.dstop == OP_NONE) {
      LogPrintf(LogWARN, "ParseUdpOrTcp: bad operation\n");
      return(0);
    }
    filterdata.opt.dstport = ParsePort(argv[2], proto);
    if (filterdata.opt.dstport == 0)
      return(0);
    argc -= 3; argv += 3;
    if (argc == 0)
      return(1);
  }
  if (argc == 1) {
    if (STREQ(*argv, "estab")) {
      filterdata.opt.estab = 1;
      return(1);
    }
    LogPrintf(LogWARN, "ParseUdpOrTcp: estab is expected: %s\n", *argv);
    return(0);
  }
  if (argc > 0)
    LogPrintf(LogWARN, "ParseUdpOrTcp: bad src/dst port syntax: %s\n", *argv);
  return(0);
}

char *opname[] = { "none", "eq", "gt", "lt" };

static int
Parse(argc, argv, ofp)
int argc;
char **argv;
struct filterent *ofp;
{
  int action, proto;
  int val;
  char *wp;
  struct filterent *fp = &filterdata;

  val = strtol(*argv, &wp, 0);
  if (*argv == wp || val > MAXFILTERS) {
    LogPrintf(LogWARN, "Parse: invalid filter number.\n");
    return(0);
  }
  if (val < 0) {
    for (val = 0; val < MAXFILTERS; val++) {
      ofp->action = A_NONE;
      ofp++;
    }
    LogPrintf(LogWARN, "Parse: filter cleared.\n");
    return(1);
  }
  ofp += val;

  if (--argc == 0) {
    LogPrintf(LogWARN, "Parse: missing action.\n");
    return(0);
  }
  argv++;

  proto = P_NONE;
  bzero(&filterdata, sizeof(filterdata));

  if (STREQ(*argv, "permit")) {
    action = A_PERMIT;
  } else if (STREQ(*argv, "deny")) {
    action = A_DENY;
  } else if (STREQ(*argv, "clear")) {
    ofp->action = A_NONE;
    return(1);
  } else {
    LogPrintf(LogWARN, "Parse: bad action: %s\n", *argv);
    return(0);
  }
  fp->action = action;

  argc--; argv++;

  if (ofp->action == A_DENY) {
    if (STREQ(*argv, "host")) {
      fp->action |= A_UHOST;
      argc--; argv++;
    } else if (STREQ(*argv, "port")) {
      fp->action |= A_UPORT;
      argc--; argv++;
    }
  }

  proto = ParseProto(argc, argv);
  if (proto == P_NONE) {
    if (ParseAddr(argc, argv, &fp->saddr, &fp->smask, &fp->swidth)) {
      argc--; argv++;
      proto = ParseProto(argc, argv);
      if (proto == P_NONE) {
	if (ParseAddr(argc, argv, &fp->daddr, &fp->dmask, &fp->dwidth)) {
	  argc--; argv++;
	}
	proto = ParseProto(argc, argv);
	if (proto) {
	  argc--; argv++;
	}
      }
    } else {
      LogPrintf(LogWARN, "Parse: Address/protocol expected.\n");
      return(0);
    }
  } else {
    argc--; argv++;
  }

  val = 1;
  fp->proto = proto;

  switch (proto) {
  case P_TCP:
    val = ParseUdpOrTcp(argc, argv, P_TCP);
    break;
  case P_UDP:
    val = ParseUdpOrTcp(argc, argv, P_UDP);
    break;
  case P_ICMP:
    val = ParseIcmp(argc, argv);
    break;
  }

  LogPrintf(LogDEBUG, "Parse: Src: %s", inet_ntoa(fp->saddr));
  LogPrintf(LogDEBUG, "Parse: Src mask: %s ", inet_ntoa(fp->smask));
  LogPrintf(LogDEBUG, "Parse: Dst: %s", inet_ntoa(fp->daddr));
  LogPrintf(LogDEBUG, "Parse: Dst mask: %s\n", inet_ntoa(fp->dmask));
  LogPrintf(LogDEBUG, "Parse: Proto = %d\n", proto);

  LogPrintf(LogDEBUG, "Parse: src:  %s (%d)\n", opname[fp->opt.srcop],
	    fp->opt.srcport);
  LogPrintf(LogDEBUG, "Parse: dst:  %s (%d)\n", opname[fp->opt.dstop],
	    fp->opt.dstport);
  LogPrintf(LogDEBUG, "Parse: estab: %d\n", fp->opt.estab);

  if (val)
    *ofp = *fp;
  return(val);
}

int
SetIfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0) {
    (void) Parse(argc, argv, ifilters);
    return 0;
  }

  return -1;
}

int
SetOfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0) {
    (void) Parse(argc, argv, ofilters);
    return 0;
  }

  return -1;
}

int
SetDfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0) {
    (void) Parse(argc, argv, dfilters);
    return 0;
  }

  return -1;
}

int
SetAfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc > 0) {
    (void) Parse(argc, argv, afilters);
    return 0;
  }

  return -1;
}

static char *protoname[] = {
  "none", "tcp", "udp", "icmp",
};

static char *actname[] = {
  "none   ", "permit ", "deny   ",
};

static void
ShowFilter(fp)
struct filterent *fp;
{
  int n;

  if (!VarTerm)
    return;

  for (n = 0; n < MAXFILTERS; n++, fp++) {
    if (fp->action != A_NONE) {
      fprintf(VarTerm, "%2d %s", n, actname[fp->action]);
      fprintf(VarTerm, "%s/%d ", inet_ntoa(fp->saddr), fp->swidth);
      fprintf(VarTerm, "%s/%d ", inet_ntoa(fp->daddr), fp->dwidth);
      if (fp->proto) {
	fprintf(VarTerm, "%s", protoname[fp->proto]);

	if (fp->opt.srcop)
	  fprintf(VarTerm, " src %s %d", opname[fp->opt.srcop],
                  fp->opt.srcport);
	if (fp->opt.dstop)
	  fprintf(VarTerm, " dst %s %d", opname[fp->opt.dstop],
                  fp->opt.dstport);
	if (fp->opt.estab)
	  fprintf(VarTerm, " estab");

      }
      fprintf(VarTerm, "\n");
    }
  }
}

int
ShowIfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  ShowFilter(ifilters);
  return 0;
}

int
ShowOfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  ShowFilter(ofilters);
  return 0;
}

int
ShowDfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  ShowFilter(dfilters);
  return 0;
}

int
ShowAfilter(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  ShowFilter(afilters);
  return 0;
}

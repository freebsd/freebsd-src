/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.c,v 1.22 1999/03/25 23:36:23 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#ifdef __OpenBSD__
#include "alias.h"
#else
#include <alias.h>
#endif
#include "defs.h"
#include "command.h"
#include "log.h"
#include "alias_cmd.h"
#include "descriptor.h"
#include "prompt.h"
#include "timer.h"
#include "fsm.h"
#include "slcompress.h"
#include "throughput.h"
#include "iplist.h"
#include "mbuf.h"
#include "lqr.h"
#include "hdlc.h"
#include "ipcp.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "filter.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"


static int StrToAddr(const char *, struct in_addr *);
static int StrToPortRange(const char *, u_short *, u_short *, const char *);
static int StrToAddrAndPort(const char *, struct in_addr *, u_short *,
                            u_short *, const char *);


int
alias_RedirectPort(struct cmdargs const *arg)
{
  if (!arg->bundle->AliasEnabled) {
    prompt_Printf(arg->prompt, "Alias not enabled\n");
    return 1;
  } else if (arg->argc == arg->argn + 3) {
    char proto_constant;
    const char *proto;
    u_short hlocalport;
    u_short llocalport;
    u_short haliasport;
    u_short laliasport;
    u_short port;
    int error;
    struct in_addr local_addr;
    struct in_addr null_addr;
    struct alias_link *link;

    proto = arg->argv[arg->argn];
    if (strcmp(proto, "tcp") == 0) {
      proto_constant = IPPROTO_TCP;
    } else if (strcmp(proto, "udp") == 0) {
      proto_constant = IPPROTO_UDP;
    } else {
      prompt_Printf(arg->prompt, "port redirect: protocol must be"
                    " tcp or udp\n");
      return -1;
    }

    error = StrToAddrAndPort(arg->argv[arg->argn+1], &local_addr, &llocalport,
                             &hlocalport, proto);
    if (error) {
      prompt_Printf(arg->prompt, "alias port: error reading localaddr:port\n");
      return -1;
    }
    error = StrToPortRange(arg->argv[arg->argn+2], &laliasport, &haliasport,
                           proto);
    if (error) {
      prompt_Printf(arg->prompt, "alias port: error reading alias port\n");
      return -1;
    }
    null_addr.s_addr = INADDR_ANY;

    if (llocalport > hlocalport) {
      port = llocalport;
      llocalport = hlocalport;
      hlocalport = port;
    }

    if (laliasport > haliasport) {
      port = laliasport;
      laliasport = haliasport;
      haliasport = port;
    }

    if (haliasport - laliasport != hlocalport - llocalport) {
      prompt_Printf(arg->prompt, "alias port: Port ranges must be equal\n");
      return -1;
    }

    for (port = laliasport; port <= haliasport; port++) {
      link = PacketAliasRedirectPort(local_addr,
                                     htons(llocalport + (port - laliasport)),
				     null_addr, 0, null_addr, htons(port),
				     proto_constant);

      if (link == NULL) {
        prompt_Printf(arg->prompt, "alias port: %d: error %d\n", port, error);
        return 1;
      }
    }
  } else
    return -1;

  return 0;
}


int
alias_RedirectAddr(struct cmdargs const *arg)
{
  if (!arg->bundle->AliasEnabled) {
    prompt_Printf(arg->prompt, "alias not enabled\n");
    return 1;
  } else if (arg->argc == arg->argn+2) {
    int error;
    struct in_addr local_addr;
    struct in_addr alias_addr;
    struct alias_link *link;

    error = StrToAddr(arg->argv[arg->argn], &local_addr);
    if (error) {
      prompt_Printf(arg->prompt, "address redirect: invalid local address\n");
      return 1;
    }
    error = StrToAddr(arg->argv[arg->argn+1], &alias_addr);
    if (error) {
      prompt_Printf(arg->prompt, "address redirect: invalid alias address\n");
      prompt_Printf(arg->prompt, "Usage: alias %s %s\n", arg->cmd->name,
                    arg->cmd->syntax);
      return 1;
    }
    link = PacketAliasRedirectAddr(local_addr, alias_addr);
    if (link == NULL) {
      prompt_Printf(arg->prompt, "address redirect: packet aliasing"
                    " engine error\n");
      prompt_Printf(arg->prompt, "Usage: alias %s %s\n", arg->cmd->name,
                    arg->cmd->syntax);
    }
  } else
    return -1;

  return 0;
}


static int
StrToAddr(const char *str, struct in_addr *addr)
{
  struct hostent *hp;

  if (inet_aton(str, addr))
    return 0;

  hp = gethostbyname(str);
  if (!hp) {
    log_Printf(LogWARN, "StrToAddr: Unknown host %s.\n", str);
    return -1;
  }
  *addr = *((struct in_addr *) hp->h_addr);
  return 0;
}


static int
StrToPort(const char *str, u_short *port, const char *proto)
{
  struct servent *sp;
  char *end;

  *port = strtol(str, &end, 10);
  if (*end != '\0') {
    sp = getservbyname(str, proto);
    if (sp == NULL) {
      log_Printf(LogWARN, "StrToAddr: Unknown port or service %s/%s.\n",
	        str, proto);
      return -1;
    }
    *port = ntohs(sp->s_port);
  }

  return 0;
}

static int
StrToPortRange(const char *str, u_short *low, u_short *high, const char *proto)
{
  char *minus;
  int res;

  minus = strchr(str, '-');
  if (minus)
    *minus = '\0';		/* Cheat the const-ness ! */

  res = StrToPort(str, low, proto);

  if (minus)
    *minus = '-';		/* Cheat the const-ness ! */

  if (res == 0) {
    if (minus)
      res = StrToPort(minus + 1, high, proto);
    else
      *high = *low;
  }

  return res;
}

static int
StrToAddrAndPort(const char *str, struct in_addr *addr, u_short *low,
                 u_short *high, const char *proto)
{
  char *colon;
  int res;

  colon = strchr(str, ':');
  if (!colon) {
    log_Printf(LogWARN, "StrToAddrAndPort: %s is missing port number.\n", str);
    return -1;
  }

  *colon = '\0';		/* Cheat the const-ness ! */
  res = StrToAddr(str, addr);
  *colon = ':';			/* Cheat the const-ness ! */
  if (res != 0)
    return -1;

  return StrToPortRange(colon + 1, low, high, proto);
}

int
alias_ProxyRule(struct cmdargs const *arg)
{
  char cmd[LINE_LEN];
  int f, pos;
  size_t len;

  if (arg->argn >= arg->argc)
    return -1;

  for (f = arg->argn, pos = 0; f < arg->argc; f++) {
    len = strlen(arg->argv[f]);
    if (sizeof cmd - pos < len + (f ? 1 : 0))
      break;
    if (f)
      cmd[pos++] = ' ';
    strcpy(cmd + pos, arg->argv[f]);
    pos += len;
  }

  return PacketAliasProxyRule(cmd);
}

int
alias_Pptp(struct cmdargs const *arg)
{
  struct in_addr addr;

  if (arg->argc == arg->argn) {
    addr.s_addr = INADDR_NONE;
    PacketAliasPptp(addr);
    return 0;
  }

  if (arg->argc != arg->argn + 1)
    return -1;

  addr = GetIpAddr(arg->argv[arg->argn]);
  if (addr.s_addr == INADDR_NONE) {
    log_Printf(LogWARN, "%s: invalid address\n", arg->argv[arg->argn]);
    return 1;
  }

  PacketAliasPptp(addr);
  return 0;
}

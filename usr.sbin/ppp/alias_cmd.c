/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.c,v 1.11 1997/12/24 10:28:37 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "vars.h"
#include "alias_cmd.h"


static int StrToAddr(const char *, struct in_addr *);
static int StrToPort(const char *, u_short *, const char *);
static int StrToAddrAndPort(const char *, struct in_addr *, u_short *, const char *);


int
AliasRedirectPort(struct cmdargs const *arg)
{
  if (!(mode & MODE_ALIAS)) {
    if (VarTerm)
      fprintf(VarTerm, "Alias not enabled\n");
    return 1;
  } else if (arg->argc == 3) {
    char proto_constant;
    const char *proto;
    u_short local_port;
    u_short alias_port;
    int error;
    struct in_addr local_addr;
    struct in_addr null_addr;
    struct alias_link *link;

    proto = arg->argv[0];
    if (strcmp(proto, "tcp") == 0) {
      proto_constant = IPPROTO_TCP;
    } else if (strcmp(proto, "udp") == 0) {
      proto_constant = IPPROTO_UDP;
    } else {
      if (VarTerm) {
	fprintf(VarTerm, "port redirect: protocol must be tcp or udp\n");
	fprintf(VarTerm, "Usage: alias %s %s\n", arg->cmd->name,
		arg->cmd->syntax);
      }
      return 1;
    }

    error = StrToAddrAndPort(arg->argv[1], &local_addr, &local_port, proto);
    if (error) {
      if (VarTerm) {
	fprintf(VarTerm, "port redirect: error reading local addr:port\n");
	fprintf(VarTerm, "Usage: alias %s %s\n", arg->cmd->name, arg->cmd->syntax);
      }
      return 1;
    }
    error = StrToPort(arg->argv[2], &alias_port, proto);
    if (error) {
      if (VarTerm) {
	fprintf(VarTerm, "port redirect: error reading alias port\n");
	fprintf(VarTerm, "Usage: alias %s %s\n", arg->cmd->name, arg->cmd->syntax);
      }
      return 1;
    }
    null_addr.s_addr = 0;

    link = VarPacketAliasRedirectPort(local_addr, local_port,
				      null_addr, 0,
				      null_addr, alias_port,
				      proto_constant);

    if (link == NULL && VarTerm)
      fprintf(VarTerm, "port redirect: error returned by packed"
	      " aliasing engine (code=%d)\n", error);
  } else
    return -1;

  return 0;
}


int
AliasRedirectAddr(struct cmdargs const *arg)
{
  if (!(mode & MODE_ALIAS)) {
    if (VarTerm)
      fprintf(VarTerm, "alias not enabled\n");
    return 1;
  } else if (arg->argc == 2) {
    int error;
    struct in_addr local_addr;
    struct in_addr alias_addr;
    struct alias_link *link;

    error = StrToAddr(arg->argv[0], &local_addr);
    if (error) {
      if (VarTerm)
	fprintf(VarTerm, "address redirect: invalid local address\n");
      return 1;
    }
    error = StrToAddr(arg->argv[1], &alias_addr);
    if (error) {
      if (VarTerm) {
	fprintf(VarTerm, "address redirect: invalid alias address\n");
	fprintf(VarTerm, "Usage: alias %s %s\n", arg->cmd->name, arg->cmd->syntax);
      }
      return 1;
    }
    link = VarPacketAliasRedirectAddr(local_addr, alias_addr);
    if (link == NULL && VarTerm) {
      fprintf(VarTerm, "address redirect: packet aliasing engine error\n");
      fprintf(VarTerm, "Usage: alias %s %s\n", arg->cmd->name, arg->cmd->syntax);
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
    LogPrintf(LogWARN, "StrToAddr: Unknown host %s.\n", str);
    return -1;
  }
  *addr = *((struct in_addr *) hp->h_addr);
  return 0;
}


static int
StrToPort(const char *str, u_short *port, const char *proto)
{
  int iport;
  struct servent *sp;
  char *end;

  iport = strtol(str, &end, 10);
  if (end != str) {
    *port = htons(iport);
    return 0;
  }
  sp = getservbyname(str, proto);
  if (!sp) {
    LogPrintf(LogWARN, "StrToAddr: Unknown port or service %s/%s.\n",
	      str, proto);
    return -1;
  }
  *port = sp->s_port;
  return 0;
}


static int
StrToAddrAndPort(const char *str, struct in_addr *addr, u_short *port, const char *proto)
{
  char *colon;
  int res;

  colon = strchr(str, ':');
  if (!colon) {
    LogPrintf(LogWARN, "StrToAddrAndPort: %s is missing port number.\n", str);
    return -1;
  }

  *colon = '\0';		/* Cheat the const-ness ! */
  res = StrToAddr(str, addr);
  *colon = ':';			/* Cheat the const-ness ! */
  if (res != 0)
    return -1;

  return StrToPort(colon+1, port, proto);
}

/*-
 * The code in this file was written by Eivind Eklund <perhaps@yes.no>,
 * who places it in the public domain without restriction.
 *
 *	$Id: alias_cmd.c,v 1.12.2.8 1998/05/01 19:23:43 brian Exp $
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "command.h"
#include "log.h"
#include "loadalias.h"
#include "alias_cmd.h"
#include "descriptor.h"
#include "prompt.h"


static int StrToAddr(const char *, struct in_addr *);
static int StrToPort(const char *, u_short *, const char *);
static int StrToAddrAndPort(const char *, struct in_addr *, u_short *, const char *);


int
alias_RedirectPort(struct cmdargs const *arg)
{
  if (!alias_IsEnabled()) {
    prompt_Printf(arg->prompt, "Alias not enabled\n");
    return 1;
  } else if (arg->argc == arg->argn+3) {
    char proto_constant;
    const char *proto;
    u_short local_port;
    u_short alias_port;
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
      prompt_Printf(arg->prompt, "Usage: alias %s %s\n", arg->cmd->name,
		    arg->cmd->syntax);
      return 1;
    }

    error = StrToAddrAndPort(arg->argv[arg->argn+1], &local_addr, &local_port,
                             proto);
    if (error) {
      prompt_Printf(arg->prompt, "port redirect: error reading"
                    " local addr:port\n");
      prompt_Printf(arg->prompt, "Usage: alias %s %s\n", arg->cmd->name,
                    arg->cmd->syntax);
      return 1;
    }
    error = StrToPort(arg->argv[arg->argn+2], &alias_port, proto);
    if (error) {
      prompt_Printf(arg->prompt, "port redirect: error reading alias port\n");
      prompt_Printf(arg->prompt, "Usage: alias %s %s\n", arg->cmd->name,
                    arg->cmd->syntax);
      return 1;
    }
    null_addr.s_addr = INADDR_ANY;

    link = (*PacketAlias.RedirectPort)(local_addr, local_port,
				      null_addr, 0,
				      null_addr, alias_port,
				      proto_constant);

    if (link == NULL)
      prompt_Printf(arg->prompt, "port redirect: error returned by packed"
	      " aliasing engine (code=%d)\n", error);
  } else
    return -1;

  return 0;
}


int
alias_RedirectAddr(struct cmdargs const *arg)
{
  if (!alias_IsEnabled()) {
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
    link = (*PacketAlias.RedirectAddr)(local_addr, alias_addr);
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
    log_Printf(LogWARN, "StrToAddr: Unknown port or service %s/%s.\n",
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
    log_Printf(LogWARN, "StrToAddrAndPort: %s is missing port number.\n", str);
    return -1;
  }

  *colon = '\0';		/* Cheat the const-ness ! */
  res = StrToAddr(str, addr);
  *colon = ':';			/* Cheat the const-ness ! */
  if (res != 0)
    return -1;

  return StrToPort(colon+1, port, proto);
}

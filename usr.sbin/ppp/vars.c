/*
 *		PPP configuration variables
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
 * $Id: vars.c,v 1.45.2.21 1998/04/03 19:24:36 brian Exp $
 *
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "termios.h"
#include "loadalias.h"
#include "vars.h"
#include "auth.h"
#include "lcp.h"
#include "async.h"
#include "throughput.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "prompt.h"

char VarVersion[] = "PPP Version 2.0-beta";
char VarLocalVersion[] = "$Date: 1998/04/03 19:24:36 $";

/*
 * Order of conf option is important. See vars.h.
 */
struct confdesc pppConfs[NCONFS] = {
  {"acfcomp", CONF_ENABLE, CONF_ACCEPT},
  {"chap", CONF_DISABLE, CONF_ACCEPT},
  {"deflate", CONF_ENABLE, CONF_ACCEPT},
  {"lqr", CONF_DISABLE, CONF_ACCEPT},
  {"pap", CONF_DISABLE, CONF_ACCEPT},
  {"pppd-deflate", CONF_DISABLE, CONF_DENY},
  {"pred1", CONF_ENABLE, CONF_ACCEPT},
  {"protocomp", CONF_ENABLE, CONF_ACCEPT},
  {"vjcomp", CONF_ENABLE, CONF_ACCEPT},
  {"msext", CONF_DISABLE, CONF_NONE},
  {"passwdauth", CONF_DISABLE, CONF_NONE},
  {"proxy", CONF_DISABLE, CONF_NONE},
  {"throughput", CONF_DISABLE, CONF_NONE},
  {"utmp", CONF_ENABLE, CONF_NONE},
  {"idcheck", CONF_ENABLE, CONF_NONE},
  {"loopback", CONF_ENABLE, CONF_NONE}
};

struct pppvars pppVars = {
  LOCAL_NO_AUTH
};

int
DisplayCommand(struct cmdargs const *arg)
{
  int f;

  prompt_Printf(&prompt, "Current configuration option settings..\n\n");
  prompt_Printf(&prompt, "Name\t\tMy Side\t\tHis Side\n");
  prompt_Printf(&prompt, "----------------------------------------\n");
  for (f = 0; f < NCONFS; f++)
    prompt_Printf(&prompt, "%-10s\t%s\t\t%s\n", pppConfs[f].name,
	          (pppConfs[f].myside == CONF_ENABLE) ? "enable" :
                   (pppConfs[f].myside == CONF_DISABLE ? "disable" : "N/A"),
	          (pppConfs[f].hisside == CONF_ACCEPT) ? "accept" :
                   (pppConfs[f].hisside == CONF_DENY ? "deny" : "N/A"));

  return 0;
}

static int
ConfigCommand(struct cmdargs const *arg, int mine, int val)
{
  int f;
  int err;
  int narg = 0;

  if (arg->argc < 1)
    return -1;

  err = 0;
  do {
    for (f = 0; f < NCONFS; f++)
      if (strcasecmp(pppConfs[f].name, arg->argv[narg]) == 0) {
	if (mine) {
          if (pppConfs[f].myside == CONF_NONE) {
            LogPrintf(LogWARN, "Config: %s cannot be enabled or disabled\n",
                      pppConfs[f].name);
            err++;
          } else
	    pppConfs[f].myside = val;
	} else {
          if (pppConfs[f].hisside == CONF_NONE) {
            LogPrintf(LogWARN, "Config: %s cannot be accepted or denied\n",
                      pppConfs[f].name);
            err++;
          } else
	    pppConfs[f].hisside = val;
        }
	break;
      }
    if (f == NCONFS) {
      LogPrintf(LogWARN, "Config: %s: No such key word\n", arg->argv[narg]);
      err++;
    }
  } while (++narg < arg->argc);

  return err;
}

int
EnableCommand(struct cmdargs const *arg)
{
  return ConfigCommand(arg, 1, CONF_ENABLE);
}

int
DisableCommand(struct cmdargs const *arg)
{
  return ConfigCommand(arg, 1, CONF_DISABLE);
}

int
AcceptCommand(struct cmdargs const *arg)
{
  return ConfigCommand(arg, 0, CONF_ACCEPT);
}

int
DenyCommand(struct cmdargs const *arg)
{
  return ConfigCommand(arg, 0, CONF_DENY);
}

int
LocalAuthCommand(struct cmdargs const *arg)
{
  const char *pass;
  if (arg->argc == 0)
    pass = "";
  else if (arg->argc > 1)
    return -1;
  else
    pass = *arg->argv;

  if (VarHaveLocalAuthKey)
    VarLocalAuth = strcmp(VarLocalAuthKey, pass) ? LOCAL_NO_AUTH : LOCAL_AUTH;
  else
    switch (LocalAuthValidate(SECRETFILE, VarShortHost, pass)) {
    case INVALID:
      VarLocalAuth = LOCAL_NO_AUTH;
      break;
    case VALID:
      VarLocalAuth = LOCAL_AUTH;
      break;
    case NOT_FOUND:
      VarLocalAuth = LOCAL_AUTH;
      LogPrintf(LogWARN, "WARNING: No Entry for this system\n");
      break;
    default:
      VarLocalAuth = LOCAL_NO_AUTH;
      LogPrintf(LogERROR, "LocalAuthCommand: Ooops?\n");
      return 1;
    }
  return 0;
}

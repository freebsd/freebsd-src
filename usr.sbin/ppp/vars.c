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
 * $Id: vars.c,v 1.40 1997/12/13 02:37:33 brian Exp $
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
#include "hdlc.h"
#include "termios.h"
#include "loadalias.h"
#include "vars.h"
#include "auth.h"

char VarVersion[] = "PPP Version 1.6";
char VarLocalVersion[] = "$Date: 1997/12/13 02:37:33 $";
int Utmp = 0;
int ipInOctets = 0;
int ipOutOctets = 0;
int ipKeepAlive = 0;
int ipConnectSecs = 0;
int ipIdleSecs = 0;
int reconnectState = RECON_UNKNOWN;
int reconnectCount = 0;

/*
 * Order of conf option is important. See vars.h.
 */
struct confdesc pppConfs[] = {
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
  {NULL},
};

struct pppvars pppVars = {
  DEF_MRU, DEF_MTU, 0, MODEM_SPEED, CS8, MODEM_CTSRTS, 180, 30, 3,
  RECONNECT_TIMER, RECONNECT_TRIES, REDIAL_PERIOD,
  NEXT_REDIAL_PERIOD, 1, 1, MODEM_DEV, "", BASE_MODEM_DEV,
  OPEN_ACTIVE, LOCAL_NO_AUTH, 0
};

int
DisplayCommand(struct cmdargs const *arg)
{
  struct confdesc *vp;

  if (!VarTerm)
    return 1;

  fprintf(VarTerm, "Current configuration option settings..\n\n");
  fprintf(VarTerm, "Name\t\tMy Side\t\tHis Side\n");
  fprintf(VarTerm, "----------------------------------------\n");
  for (vp = pppConfs; vp->name; vp++)
    fprintf(VarTerm, "%-10s\t%s\t\t%s\n", vp->name,
	    (vp->myside == CONF_ENABLE) ? "enable" :
             (vp->myside == CONF_DISABLE ? "disable" : "N/A"),
	    (vp->hisside == CONF_ACCEPT) ? "accept" :
             (vp->hisside == CONF_DENY ? "deny" : "N/A"));

  return 0;
}

static int
ConfigCommand(struct cmdargs const *arg, int mine, int val)
{
  struct confdesc *vp;
  int err;
  int narg = 0;

  if (arg->argc < 1)
    return -1;

  err = 0;
  do {
    for (vp = pppConfs; vp->name; vp++)
      if (strcasecmp(vp->name, arg->argv[narg]) == 0) {
	if (mine) {
          if (vp->myside == CONF_NONE) {
            LogPrintf(LogWARN, "Config: %s cannot be enabled or disabled\n",
                      vp->name);
            err++;
          } else
	    vp->myside = val;
	} else {
          if (vp->hisside == CONF_NONE) {
            LogPrintf(LogWARN, "Config: %s cannot be accepted or denied\n",
                      vp->name);
            err++;
          } else
	    vp->hisside = val;
        }
	break;
      }
    if (!vp->name) {
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

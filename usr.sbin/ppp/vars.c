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
 * $Id: vars.c,v 1.24 1997/08/21 17:20:00 brian Exp $
 *
 */
#include "fsm.h"
#include "command.h"
#include "hdlc.h"
#include "termios.h"
#include "loadalias.h"
#include "vars.h"
#include "auth.h"
#include "defs.h"

char VarVersion[] = "PPP Version 1.1";
char VarLocalVersion[] = "$Date: 1997/08/21 17:20:00 $";

/*
 * Order of conf option is important. See vars.h.
 */
struct confdesc pppConfs[] = {
  {"vjcomp", CONF_ENABLE, CONF_ACCEPT},
  {"lqr", CONF_ENABLE, CONF_ACCEPT},
  {"chap", CONF_DISABLE, CONF_ACCEPT},
  {"pap", CONF_DISABLE, CONF_ACCEPT},
  {"acfcomp", CONF_ENABLE, CONF_ACCEPT},
  {"protocomp", CONF_ENABLE, CONF_ACCEPT},
  {"pred1", CONF_ENABLE, CONF_ACCEPT},
  {"proxy", CONF_DISABLE, CONF_DENY},
  {"msext", CONF_DISABLE, CONF_ACCEPT},
  {"passwdauth", CONF_DISABLE, CONF_DENY},
  {NULL},
};

struct pppvars pppVars = {
  DEF_MRU, DEF_MTU, 0, MODEM_SPEED, CS8, MODEM_CTSRTS, 180, 30, 3,
  RECONNECT_TIMER, RECONNECT_TRIES, REDIAL_PERIOD,
  NEXT_REDIAL_PERIOD, 1, 1, MODEM_DEV, BASE_MODEM_DEV,
  OPEN_ACTIVE, LOCAL_NO_AUTH, 0
};

int
DisplayCommand()
{
  struct confdesc *vp;

  if (!VarTerm)
    return 1;

  fprintf(VarTerm, "Current configuration option settings..\n\n");
  fprintf(VarTerm, "Name\t\tMy Side\t\tHis Side\n");
  fprintf(VarTerm, "----------------------------------------\n");
  for (vp = pppConfs; vp->name; vp++)
    fprintf(VarTerm, "%-10s\t%s\t\t%s\n", vp->name,
	    (vp->myside == CONF_ENABLE) ? "enable" : "disable",
	    (vp->hisside == CONF_ACCEPT) ? "accept" : "deny");

  return 0;
}

static int
ConfigCommand(struct cmdtab * list, int argc, char **argv, int mine, int val)
{
  struct confdesc *vp;
  int err;

  if (argc < 1)
    return -1;

  err = 0;
  do {
    for (vp = pppConfs; vp->name; vp++)
      if (strcasecmp(vp->name, *argv) == 0) {
	if (mine)
	  vp->myside = val;
	else
	  vp->hisside = val;
	break;
      }
    if (!vp->name) {
      LogPrintf(LogWARN, "Config: %s: No such key word\n", *argv);
      err++;
    }
    argc--;
    argv++;
  } while (argc > 0);

  return err;
}

int
EnableCommand(struct cmdtab * list, int argc, char **argv)
{
  return ConfigCommand(list, argc, argv, 1, CONF_ENABLE);
}

int
DisableCommand(struct cmdtab * list, int argc, char **argv)
{
  return ConfigCommand(list, argc, argv, 1, CONF_DISABLE);
}

int
AcceptCommand(struct cmdtab * list, int argc, char **argv)
{
  return ConfigCommand(list, argc, argv, 0, CONF_ACCEPT);
}

int
DenyCommand(struct cmdtab * list, int argc, char **argv)
{
  return ConfigCommand(list, argc, argv, 0, CONF_DENY);
}

int
LocalAuthCommand(struct cmdtab * list, int argc, char **argv)
{
  if (argc != 1)
    return -1;

  switch (LocalAuthValidate(SECRETFILE, VarShortHost, *argv)) {
  case INVALID:
    pppVars.lauth = LOCAL_NO_AUTH;
    break;
  case VALID:
    pppVars.lauth = LOCAL_AUTH;
    break;
  case NOT_FOUND:
    pppVars.lauth = LOCAL_AUTH;
    LogPrintf(LogWARN, "WARING: No Entry for this system\n");
    break;
  default:
    pppVars.lauth = LOCAL_NO_AUTH;
    LogPrintf(LogERROR, "LocalAuthCommand: Ooops?\n");
    return 1;
  }
  return 0;
}

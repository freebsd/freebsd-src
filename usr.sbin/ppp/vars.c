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
 * $Id: vars.c,v 1.45.2.26 1998/04/07 00:54:24 brian Exp $
 *
 */
#include <sys/types.h>

#include <stdio.h>
#include <string.h>

#include "command.h"
#include "log.h"
#include "termios.h"
#include "vars.h"
#include "descriptor.h"
#include "prompt.h"

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

  {"idcheck", CONF_ENABLE, CONF_NONE},
  {"loopback", CONF_ENABLE, CONF_NONE},
  {"msext", CONF_DISABLE, CONF_NONE},
  {"passwdauth", CONF_DISABLE, CONF_NONE},
  {"proxy", CONF_DISABLE, CONF_NONE},
  {"throughput", CONF_DISABLE, CONF_NONE},
  {"utmp", CONF_ENABLE, CONF_NONE}
};

int
DisplayCommand(struct cmdargs const *arg)
{
  int f;

  prompt_Printf(arg->prompt, "Current configuration option settings..\n\n");
  prompt_Printf(arg->prompt, "Name\t\tMy Side\t\tHis Side\n");
  prompt_Printf(arg->prompt, "----------------------------------------\n");
  for (f = 0; f < NCONFS; f++)
    prompt_Printf(arg->prompt, "%-10s\t%s\t\t%s\n", pppConfs[f].name,
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
  int narg = arg->argn;

  if (arg->argc < narg+1)
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

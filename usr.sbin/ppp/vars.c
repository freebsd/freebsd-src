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
 * $Id: vars.c,v 1.13 1997/04/14 23:48:19 brian Exp $
 *
 */
#include "fsm.h"
#include "command.h"
#include "hdlc.h"
#include "termios.h"
#include "vars.h"
#include "auth.h"
#include "defs.h"

char VarVersion[] = "Version 0.94";
char VarLocalVersion[] = "$Date: 1997/04/14 23:48:19 $";

/*
 * Order of conf option is important. See vars.h.
 */
struct confdesc pppConfs[] = {
  { "vjcomp",    CONF_ENABLE,  CONF_ACCEPT },
  { "lqr",       CONF_ENABLE,  CONF_ACCEPT },
  { "chap",      CONF_DISABLE, CONF_ACCEPT },
  { "pap",       CONF_DISABLE, CONF_ACCEPT },
  { "acfcomp",   CONF_ENABLE,  CONF_ACCEPT },
  { "protocomp", CONF_ENABLE,  CONF_ACCEPT },
  { "pred1",	 CONF_ENABLE,  CONF_ACCEPT },
  { "proxy",	 CONF_DISABLE, CONF_DENY   },
  { "msext",     CONF_DISABLE, CONF_ACCEPT },
  { "passwdauth",CONF_DISABLE,  CONF_DENY   },
  { NULL },
};

struct pppvars pppVars = {
  DEF_MRU, 0, MODEM_SPEED, CS8, MODEM_CTSRTS, 180, 30, 3,
  RECONNECT_TIMER, RECONNECT_TRIES, REDIAL_PERIOD,
  NEXT_REDIAL_PERIOD, 1, MODEM_DEV, OPEN_PASSIVE, LOCAL_NO_AUTH,
};

int
DisplayCommand()
{
  struct confdesc *vp;

  printf("Current configuration option settings..\n\n");
  printf("Name\t\tMy Side\t\tHis Side\n");
  printf("----------------------------------------\n");
  for (vp = pppConfs; vp->name; vp++)
    printf("%-10s\t%s\t\t%s\n", vp->name,
	(vp->myside == CONF_ENABLE)? "enable" : "disable",
	(vp->hisside == CONF_ACCEPT)? "accept" : "deny");
  return(1);
}

int
DisableCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct confdesc *vp;
  int    found  = FALSE;

  if (argc < 1) {
    printf("disable what?\n");
    return(1);
  }
  do {
    for (vp = pppConfs; vp->name; vp++) {
      if (strcasecmp(vp->name, *argv) == 0) {
	vp->myside = CONF_DISABLE;
        found  = TRUE;
      }
    }
    if ( found == FALSE )
       printf("%s - No such key word\n", *argv );
    argc--; argv++;
  } while (argc > 0);
  return(1);
}

int
EnableCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct confdesc *vp;
  int    found  = FALSE;

  if (argc < 1) {
    printf("enable what?\n");
    return(1);
  }
  do {
    for (vp = pppConfs; vp->name; vp++) {
      if (strcasecmp(vp->name, *argv) == 0) {
	vp->myside = CONF_ENABLE;
        found  = TRUE;
      }
    }
    if ( found == FALSE )
       printf("%s - No such key word\n", *argv );
    argc--; argv++;
  } while (argc > 0);
  return(1);
}

int
AcceptCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct confdesc *vp;
  int    found  = FALSE;

  if (argc < 1) {
    printf("accept what?\n");
    return(1);
  }
  do {
    for (vp = pppConfs; vp->name; vp++) {
      if (strcasecmp(vp->name, *argv) == 0) {
	vp->hisside = CONF_ACCEPT;
        found  = TRUE;
      }
    }
    if ( found == FALSE )
       printf("%s - No such key word\n", *argv );
    argc--; argv++;
  } while (argc > 0);
  return(1);
}

int
DenyCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  struct confdesc *vp;
  int    found  = FALSE;

  if (argc < 1) {
    printf("enable what?\n");
    return(1);
  }
  do {
    for (vp = pppConfs; vp->name; vp++) {
      if (strcasecmp(vp->name, *argv) == 0) {
	vp->hisside = CONF_DENY;
        found  = TRUE;
      }
    }
    if ( found == FALSE )
       printf("%s - No such key word\n", *argv );
    argc--; argv++;
  } while (argc > 0);
  return(1);
}

int
LocalAuthCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  if (argc < 1) {
    printf("Please Enter passwd for manipulating.\n");
    return(1);
  }

  switch ( LocalAuthValidate( SECRETFILE, VarShortHost, *argv ) ) {
	case INVALID:
		pppVars.lauth = LOCAL_NO_AUTH;
		break;
	case VALID:
		pppVars.lauth = LOCAL_AUTH;
		break;
	case NOT_FOUND:
		pppVars.lauth = LOCAL_AUTH;
		printf("WARING: No Entry for this system\n");
		break;
	default:
		pppVars.lauth = LOCAL_NO_AUTH;
		printf("Ooops?\n");
		break;
  }
  return(1);
}

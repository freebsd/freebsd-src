/*
 *	          System configuration routines
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
 * $Id: systems.c,v 1.20 1997/11/09 13:18:18 brian Exp $
 *
 *  TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "loadalias.h"
#include "command.h"
#include "ipcp.h"
#include "pathnames.h"
#include "vars.h"
#include "server.h"
#include "systems.h"

FILE *
OpenSecret(char *file)
{
  FILE *fp;
  char line[100];

  snprintf(line, sizeof line, "%s/%s", _PATH_PPP, file);
  fp = ID0fopen(line, "r");
  if (fp == NULL)
    LogPrintf(LogWARN, "OpenSecret: Can't open %s.\n", line);
  return (fp);
}

void
CloseSecret(FILE * fp)
{
  fclose(fp);
}

int
SelectSystem(char *name, char *file)
{
  FILE *fp;
  char *cp, *wp;
  int n, len;
  u_char olauth;
  char line[LINE_LEN];
  char filename[200];
  int linenum;

  snprintf(filename, sizeof filename, "%s/%s", _PATH_PPP, file);
  fp = ID0fopen(filename, "r");
  if (fp == NULL) {
    LogPrintf(LogDEBUG, "SelectSystem: Can't open %s.\n", filename);
    return (-1);
  }
  LogPrintf(LogDEBUG, "SelectSystem: Checking %s (%s).\n", name, filename);

  linenum = 0;
  while (fgets(line, sizeof(line), fp)) {
    linenum++;
    cp = line;
    switch (*cp) {
    case '#':			/* comment */
      break;
    case ' ':
    case '\t':
      break;
    default:
      wp = strpbrk(cp, ":\n");
      if (wp == NULL) {
	LogPrintf(LogWARN, "Bad rule in %s (line %d) - missing colon.\n",
		  filename, linenum);
	ServerClose();
	exit(1);
      }
      *wp = '\0';
      if (strcmp(cp, name) == 0) {
	while (fgets(line, sizeof(line), fp)) {
	  cp = line;
	  if (*cp == ' ' || *cp == '\t') {
	    n = strspn(cp, " \t");
	    cp += n;
            len = strlen(cp);
            if (!len)
              continue;
            if (cp[len-1] == '\n')
              cp[--len] = '\0';
            if (!len)
              continue;
	    LogPrintf(LogCOMMAND, "%s: %s\n", name, cp);
	    olauth = VarLocalAuth;
	    if (VarLocalAuth == LOCAL_NO_AUTH)
	      VarLocalAuth = LOCAL_AUTH;
	    DecodeCommand(cp, len, 0);
	    VarLocalAuth = olauth;
	  } else if (*cp == '#') {
	    continue;
	  } else
	    break;
	}
	fclose(fp);
	return (0);
      }
      break;
    }
  }
  fclose(fp);
  return -1;
}

int
LoadCommand(struct cmdtab const * list, int argc, char **argv)
{
  char *name;

  if (argc > 0)
    name = *argv;
  else
    name = "default";

  if (SelectSystem(name, CONFFILE) < 0) {
    LogPrintf(LogWARN, "%s: not found.\n", name);
    return -1;
  }
  return 0;
}

int
SaveCommand(struct cmdtab const *list, int argc, char **argv)
{
  LogPrintf(LogWARN, "save command is not implemented (yet).\n");
  return 1;
}

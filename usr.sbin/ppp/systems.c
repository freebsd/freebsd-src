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
 * $Id: systems.c,v 1.28 1997/11/22 03:37:51 brian Exp $
 *
 *  TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "loadalias.h"
#include "ipcp.h"
#include "pathnames.h"
#include "vars.h"
#include "server.h"
#include "chat.h"
#include "systems.h"

#define issep(ch) ((ch) == ' ' || (ch) == '\t')

FILE *
OpenSecret(const char *file)
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

/* Move string from ``from'' to ``to'', interpreting ``~'' and $.... */
static void
InterpretArg(char *from, char *to)
{
  const char *env;
  char *ptr, *startto, *endto;
  int len;

  startto = to;
  endto = to + LINE_LEN - 1;

  while(issep(*from))
    from++;
  if (*from == '~') {
    ptr = strchr(++from, '/');
    len = ptr ? ptr - from : strlen(from);
    if (len == 0) {
      if ((env = getenv("HOME")) == NULL)
        env = _PATH_PPP;
      strncpy(to, env, endto - to);
    } else {
      struct passwd *pwd;

      strncpy(to, from, len);
      to[len] = '\0';
      pwd = getpwnam(to);
      if (pwd)
        strncpy(to, pwd->pw_dir, endto-to);
      else
        strncpy(to, _PATH_PPP, endto - to);
      endpwent();
    }
    *endto = '\0';
    to += strlen(to);
    from += len;
  }

  while (to < endto && *from != '\0') {
    if (*from == '$') {
      if (from[1] == '$') {
        *to = '\0';	/* For an empty var name below */
        from += 2;
      } else if (from[1] == '{') {
        ptr = strchr(from+2, '}');
        if (ptr) {
          len = ptr - from - 2;
          if (endto - to < len )
            len = endto - to;
          if (len) {
            strncpy(to, from+2, len);
            to[len] = '\0';
            from = ptr+1;
          } else {
            *to++ = *from++;
            continue;
          }
        } else {
          *to++ = *from++;
          continue;
        }
      } else {
        ptr = to;
        for (from++; (isalnum(*from) || *from == '_') && ptr < endto; from++)
          *ptr++ = *from;
        *ptr = '\0';
      }
      if (*to == '\0')
        *to++ = '$';
      else if ((env = getenv(to)) != NULL) {
        strncpy(to, env, endto - to);
        *endto = '\0';
        to += strlen(to);
      }
    } else
      *to++ = *from++;
  }
  while (to > startto) {
    to--;
    if (!issep(*to)) {
      to++;
      break;
    }
  }
  *to = '\0';
}

#define CTRL_UNKNOWN (0)
#define CTRL_INCLUDE (1)

static int
DecodeCtrlCommand(char *line, char *arg)
{
  if (!strncasecmp(line, "include", 7) && issep(line[7])) {
    InterpretArg(line+8, arg);
    return CTRL_INCLUDE;
  }
  return CTRL_UNKNOWN;
}

static int userok;

int
AllowUsers(struct cmdargs const *arg)
{
  int f;
  char *user;

  userok = 0;
  user = getlogin();
  if (user && *user)
    for (f = 0; f < arg->argc; f++)
      if (!strcmp("*", arg->argv[f]) || !strcmp(user, arg->argv[f])) {
        userok = 1;
        break;
      }

  return 0;
}

static struct {
  int mode;
  const char *name;
} modes[] = {
  { MODE_INTER, "interactive" },
  { MODE_AUTO, "auto" },
  { MODE_DIRECT, "direct" },
  { MODE_DEDICATED, "dedicated" },
  { MODE_DDIAL, "ddial" },
  { MODE_BACKGROUND, "background" },
  { ~0, "*" },
  { 0, 0 }
};

static int modeok;

int
AllowModes(struct cmdargs const *arg)
{
  int f;
  int m;
  int allowed;

  allowed = 0;
  for (f = 0; f < arg->argc; f++) {
    for (m = 0; modes[m].mode; m++)
      if (!strcasecmp(modes[m].name, arg->argv[f])) {
        allowed |= modes[m].mode;
        break;
      }
    if (modes[m].mode == 0)
      LogPrintf(LogWARN, "%s: Invalid mode\n", arg->argv[f]);
  }

  modeok = (mode | allowed) == allowed ? 1 : 0;
  return 0;
}

static int
ReadSystem(const char *name, const char *file, int doexec)
{
  FILE *fp;
  char *cp, *wp;
  int n, len;
  u_char olauth;
  char line[LINE_LEN];
  char filename[200];
  int linenum;
  int argc;
  char **argv;
  int allowcmd;

  if (*file == '/')
    snprintf(filename, sizeof filename, "%s", file);
  else
    snprintf(filename, sizeof filename, "%s/%s", _PATH_PPP, file);
  fp = ID0fopen(filename, "r");
  if (fp == NULL) {
    LogPrintf(LogDEBUG, "ReadSystem: Can't open %s.\n", filename);
    return (-1);
  }
  LogPrintf(LogDEBUG, "ReadSystem: Checking %s (%s).\n", name, filename);

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
      if (*cp == '!') {
        char arg[LINE_LEN];
        switch (DecodeCtrlCommand(cp+1, arg)) {
        case CTRL_INCLUDE:
          LogPrintf(LogCOMMAND, "%s: Including \"%s\"\n", filename, arg);
          n = ReadSystem(name, arg, doexec);
          LogPrintf(LogCOMMAND, "%s: Done include of \"%s\"\n", filename, arg);
          if (!n)
            return 0;	/* got it */
          break;
        default:
          LogPrintf(LogWARN, "%s: %s: Invalid command\n", filename, cp);
          break;
        }
      } else if (strcmp(cp, name) == 0) {
	while (fgets(line, sizeof(line), fp)) {
	  cp = line;
          if (issep(*cp)) {
	    n = strspn(cp, " \t");
	    cp += n;
            len = strlen(cp);
            if (!len)
              continue;
            if (cp[len-1] == '\n')
              cp[--len] = '\0';
            if (!len)
              continue;
            InterpretCommand(cp, len, &argc, &argv);
            allowcmd = argc > 0 && !strcasecmp(*argv, "allow");
            if ((!doexec && allowcmd) || (doexec && !allowcmd)) {
	      olauth = VarLocalAuth;
	      if (VarLocalAuth == LOCAL_NO_AUTH)
	        VarLocalAuth = LOCAL_AUTH;
	      RunCommand(argc, (char const *const *)argv, name);
	      VarLocalAuth = olauth;
	    }
	  } else if (*cp == '#' || *cp == '\n' || *cp == '\0') {
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
ValidSystem(const char *name)
{
  if (ID0realuid() == 0)
    return userok = modeok = 1;
  userok = 0;
  modeok = 1;
  ReadSystem("default", CONFFILE, 0);
  if (name != NULL)
    ReadSystem(name, CONFFILE, 0);
  return userok && modeok;
}

int
SelectSystem(const char *name, const char *file)
{
  userok = modeok = 1;
  return ReadSystem(name, file, 1);
}

int
LoadCommand(struct cmdargs const *arg)
{
  const char *name;

  if (arg->argc > 0)
    name = *arg->argv;
  else
    name = "default";

  if (!ValidSystem(name)) {
    LogPrintf(LogERROR, "%s: Label not allowed\n", name);
    return 1;
  } else if (SelectSystem(name, CONFFILE) < 0) {
    LogPrintf(LogWARN, "%s: not found.\n", name);
    return -1;
  } else
    SetLabel(arg->argc ? name : NULL);
  return 0;
}

int
SaveCommand(struct cmdargs const *arg)
{
  LogPrintf(LogWARN, "save command is not implemented (yet).\n");
  return 1;
}

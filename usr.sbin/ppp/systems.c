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
 * $Id: systems.c,v 1.10 1997/05/10 01:22:19 brian Exp $
 *
 *  TODO:
 */
#include "fsm.h"
#include "loadalias.h"
#include "vars.h"
#include "ipcp.h"
#include "pathnames.h"
#include "vars.h"

extern void DecodeCommand();

static int uid, gid;
static int euid, egid;
static int usermode;

int
OrigUid()
{
    return uid;
}

void
GetUid()
{
  uid = getuid();
  gid = getgid();
  euid = geteuid();
  egid = getegid();
  usermode = 0;
}

static void
SetUserId()
{
  if (!usermode) {
    if (setreuid(euid, uid) == -1) {
      logprintf("unable to setreuid!\n");
      exit(1);
    }
    if (setregid(egid, gid) == -1) {
      logprintf("unable to setregid!\n");
      exit(1);
    }
    usermode = 1;
  }
}

static void
SetPppId()
{
  if (usermode) {
    if (setreuid(uid, euid) == -1) {
      logprintf("unable to setreuid!\n");
      exit(1);
    }
    if (setregid(gid, egid) == -1) {
      logprintf("unable to setregid!\n");
      exit(1);
    }
    usermode = 0;
  }
}

FILE *
OpenSecret(file)
char *file;
{
  FILE *fp;
  char *cp;
  char line[100];

  fp = NULL;
  cp = getenv("HOME");
  if (cp) {
    SetUserId();
    snprintf(line, sizeof line, "%s/.%s", cp, file);
    fp = fopen(line, "r");
  }
  if (fp == NULL) {
    SetPppId();
    snprintf(line, sizeof line, "%s/%s", _PATH_PPP, file);
    fp = fopen(line, "r");
  }
  if (fp == NULL) {
    fprintf(stderr, "can't open %s.\n", line);
    SetPppId();
    return(NULL);
  }
  return(fp);
}

void
CloseSecret(fp)
FILE *fp;
{
  fclose(fp);
  SetPppId();
}

int
SelectSystem(name, file)
char *name;
char *file;
{
  FILE *fp;
  char *cp, *wp;
  int n;
  int val = -1;
  u_char  olauth;
  char line[200];
  char filename[200];
  int linenum;

  fp = NULL;
  cp = getenv("HOME");
  if (cp) {
    SetUserId();
    snprintf(filename, sizeof filename, "%s/.%s", cp, file);
    fp = fopen(filename, "r");
  }
  if (fp == NULL) {
    SetPppId();		/* fix from pdp@ark.jr3uom.iijnet.or.jp */
    snprintf(filename, sizeof filename, "%s/%s", _PATH_PPP, file);
    fp = fopen(filename, "r");
  }
  if (fp == NULL) {
#ifdef DEBUG
    fprintf(stderr, "can't open %s.\n", filename);
#endif
    SetPppId();
    return(-1);
  }
#ifdef DEBUG
  fprintf(stderr, "checking %s (%s).\n", name, filename);
#endif

  linenum = 0;
  while (fgets(line, sizeof(line), fp)) {
    linenum++;
    cp = line;
    switch (*cp) {
    case '#':		/* comment */
      break;
    case ' ':
    case '\t':
      break;
    default:
      wp = strpbrk(cp, ":\n");
      if (wp == NULL) {
	fprintf(stderr, "Bad rule in %s (line %d) - missing colon.\n",
		filename, linenum);
	exit(1);
      }
      *wp = '\0';
      if (strcmp(cp, name) == 0) {
	while (fgets(line, sizeof(line), fp)) {
	  cp = line;
	  if (*cp == ' ' || *cp == '\t') {
	    n = strspn(cp, " \t");
	    cp += n;
#ifdef DEBUG
	    fprintf(stderr, "%s", cp);
#endif
	    SetPppId();
            olauth = VarLocalAuth;
	    VarLocalAuth = LOCAL_AUTH;
	    DecodeCommand(cp, strlen(cp), 0);
            VarLocalAuth = olauth;
	    SetUserId();
	  } else if (*cp == '#') {
	    continue;
	  } else
	    break;
	}
	fclose(fp);
	SetPppId();
	return(0);
      }
      break;
    }
  }
  fclose(fp);
  SetPppId();
  return(val);
}

int
LoadCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  char *name;

  if (argc > 0)
    name = *argv;
  else
    name = "default";

  if (SelectSystem(name, CONFFILE) < 0) {
    printf("%s: not found.\n", name);
    return(-1);
  }
  return(1);
}

int
SaveCommand(list, argc, argv)
struct cmdtab *list;
int argc;
char **argv;
{
  printf("save command is not implemented (yet).\n");
  return(1);
}

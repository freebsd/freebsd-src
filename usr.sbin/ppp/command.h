/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: command.h,v 1.12.2.9 1998/04/07 00:53:34 brian Exp $
 *
 *	TODO:
 */

struct cmdtab;
struct bundle;
struct datalink;
struct prompt;

struct cmdargs {
  struct cmdtab const *cmdtab;		/* The entire command table */
  struct cmdtab const *cmd;		/* This command entry */
  int argc;				/* Number of arguments (excluding cmd */
  int argn;				/* Argument to start processing from */
  char const *const *argv;		/* Arguments */
  struct bundle *bundle;		/* Our bundle */
  struct datalink *cx;			/* Our context */
  struct prompt *prompt;		/* Who executed us */
};

struct cmdtab {
  const char *name;
  const char *alias;
  int (*func) (struct cmdargs const *);
  u_char lauth;
  const char *helpmes;
  const char *syntax;
  const void *args;
};

#define	VAR_AUTHKEY	0
#define	VAR_DIAL	1
#define	VAR_LOGIN	2
#define	VAR_AUTHNAME	3
#define	VAR_WINSIZE	4
#define	VAR_DEVICE	5
#define	VAR_ACCMAP	6
#define	VAR_MRU		7
#define	VAR_MTU		8
#define	VAR_OPENMODE	9
#define	VAR_PHONE	10
#define	VAR_HANGUP	11
#define	VAR_ENC		12
#define	VAR_IDLETIMEOUT	13
#define	VAR_LQRPERIOD	14
#define	VAR_LCPRETRY	15
#define	VAR_CHAPRETRY	16
#define	VAR_PAPRETRY	17
#define	VAR_CCPRETRY	18
#define	VAR_IPCPRETRY	19

extern int IsInteractive(struct prompt *);
extern void InterpretCommand(char *, int, int *, char ***);
extern void RunCommand(struct bundle *, int, char const *const *,
                       struct prompt *, const char *);
extern void DecodeCommand(struct bundle *, char *, int, struct prompt *,
                          const char *);
extern struct link *ChooseLink(struct cmdargs const *);

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
 * $Id: command.h,v 1.12.2.5 1998/03/17 22:29:07 brian Exp $
 *
 *	TODO:
 */

struct cmdtab;

struct cmdargs {
  struct cmdtab const *cmdtab;		/* The entire command table */
  struct cmdtab const *cmd;		/* This command entry */
  int argc;
  char const *const *argv;
  struct bundle *bundle;
  struct datalink *cx;
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
#define	VAR_PHONE	7
#define	VAR_HANGUP	8
#ifdef HAVE_DES
#define	VAR_ENC		9
#endif

extern struct in_addr ifnetmask;
extern int aft_cmd;

extern int IsInteractive(int);
extern void InterpretCommand(char *, int, int *, char ***);
extern void RunCommand(struct bundle *, int, char const *const *, const char *);
extern void DecodeCommand(struct bundle *, char *, int, const char *);
extern struct link *ChooseLink(struct cmdargs const *);

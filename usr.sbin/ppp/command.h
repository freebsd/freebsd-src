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
 * $Id: command.h,v 1.2.6.3 1997/09/22 00:50:36 brian Exp $
 *
 *	TODO:
 */

struct cmdtab;

struct cmdargs {
  struct cmdtab const *cmd;
  int argc;
  char const *const *argv;
  const void *data;
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
#define	VAR_DEVICE	4
#define	VAR_ACCMAP	5
#define	VAR_PHONE	6
#define	VAR_HANGUP	7
#ifdef HAVE_DES
#define	VAR_ENC		8
#endif

extern struct in_addr ifnetmask;
extern int aft_cmd;

extern int SetVariable(struct cmdargs const *);
extern void Prompt(void);
extern int IsInteractive(int);
extern void InterpretCommand(char *, int, int *, char ***);
extern void RunCommand(int, char const *const *, const char *label);
extern void DecodeCommand(char *, int, const char *label);

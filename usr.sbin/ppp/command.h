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
 * $FreeBSD: src/usr.sbin/ppp/command.h,v 1.20.2.1 2000/03/21 10:23:04 brian Exp $
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

#define NEG_ACCEPTED (1)
#define NEG_ENABLED (2)
#define IsAccepted(x) ((x) & NEG_ACCEPTED)
#define IsEnabled(x) ((x) & NEG_ENABLED)

extern const char Version[];

extern void command_Expand(char **, int, char const *const *, struct bundle *,
                           int, pid_t);
extern int command_Expand_Interpret(char *, int, char *vector[MAXARGS], int);
extern int command_Interpret(char *, int, char *vector[MAXARGS]);
extern void command_Run(struct bundle *, int, char const *const *,
                        struct prompt *, const char *, struct datalink *);
extern int command_Decode(struct bundle *, char *, int, struct prompt *,
                           const char *);
extern struct link *command_ChooseLink(struct cmdargs const *);
extern const char *command_ShowNegval(unsigned);


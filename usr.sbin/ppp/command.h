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
 * $Id: command.h,v 1.5 1997/08/25 00:29:09 brian Exp $
 *
 *	TODO:
 */

struct cmdtab {
  char *name;
  char *alias;
  int (*func) ();
  u_char lauth;
  char *helpmes;
  char *syntax;
  void *args;
};

#define	VAR_AUTHKEY	0
#define	VAR_DIAL	1
#define	VAR_LOGIN	2
#define	VAR_AUTHNAME	3
#define	VAR_DEVICE	4
#define	VAR_ACCMAP	5
#define	VAR_PHONE	6
#define	VAR_HANGUP	7
#define	VAR_ENC		8

extern int SetVariable(struct cmdtab const *, int, char **, int var_param);

/*
 *			User Process PPP
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
 * $Id: systems.h,v 1.10.2.1 1998/02/02 19:32:15 brian Exp $
 *
 */

struct prompt;

extern int SelectSystem(struct bundle *bundle, const char *, const char *,
                        struct prompt *);
extern int ValidSystem(const char *, struct prompt *);
extern FILE *OpenSecret(const char *);
extern void CloseSecret(FILE *);
extern int AllowUsers(struct cmdargs const *);
extern int AllowModes(struct cmdargs const *);
extern int LoadCommand(struct cmdargs const *);
extern int SaveCommand(struct cmdargs const *);

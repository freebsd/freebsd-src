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
 * $Id: systems.h,v 1.11 1998/05/21 21:48:36 brian Exp $
 *
 */

struct prompt;
struct datalink;
struct bundle;
struct cmdargs;

extern int system_Select(struct bundle *bundle, const char *, const char *,
                        struct prompt *, struct datalink *);
extern int system_IsValid(const char *, struct prompt *, int);
extern FILE *OpenSecret(const char *);
extern void CloseSecret(FILE *);
extern int AllowUsers(struct cmdargs const *);
extern int AllowModes(struct cmdargs const *);
extern int LoadCommand(struct cmdargs const *);
extern int SaveCommand(struct cmdargs const *);

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
 * $Id: route.h,v 1.10.2.6 1998/05/05 23:30:13 brian Exp $
 *
 */

struct bundle;
struct cmdargs;

#define ROUTE_STATIC		0
#define ROUTE_DSTMYADDR		1
#define ROUTE_DSTHISADDR	2
#define ROUTE_DSTANY		3
#define ROUTE_GWHISADDR		4	/* May be ORd with DST_MYADDR */

struct sticky_route {
  int type;				/* ROUTE_* value (not _STATIC) */
  struct sticky_route *next;		/* next in list */

  struct in_addr dst;
  struct in_addr mask;
  struct in_addr gw;
};

extern int GetIfIndex(char *);
extern int route_Show(struct cmdargs const *);
extern void route_IfDelete(struct bundle *, int);
extern const char *Index2Nam(int);
extern void route_Change(struct bundle *, struct sticky_route *,
                         struct in_addr, struct in_addr);
extern void route_Add(struct sticky_route **, int, struct in_addr,
                      struct in_addr, struct in_addr);
extern void route_Delete(struct sticky_route **, int, struct in_addr);
extern void route_DeleteAll(struct sticky_route **);
extern void route_Clean(struct bundle *, struct sticky_route *);
extern void route_ShowSticky(struct prompt *, struct sticky_route *);

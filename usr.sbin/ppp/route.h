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
 * $FreeBSD$
 *
 */

struct bundle;
struct cmdargs;
struct rt_msghdr;
struct sockaddr;

#define ROUTE_STATIC		0x00
#define ROUTE_DSTMYADDR		0x01
#define ROUTE_DSTHISADDR	0x02
#define ROUTE_DSTDNS0		0x04
#define ROUTE_DSTDNS1		0x08
#define ROUTE_DSTANY		0x0f
#define ROUTE_GWHISADDR		0x10	/* May be ORd with DST_* */

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
extern void route_UpdateMTU(struct bundle *);
extern const char *Index2Nam(int);
extern void route_Change(struct bundle *, struct sticky_route *,
                         struct in_addr, struct in_addr, struct in_addr[2]);
extern void route_Add(struct sticky_route **, int, struct in_addr,
                      struct in_addr, struct in_addr);
extern void route_Delete(struct sticky_route **, int, struct in_addr);
extern void route_DeleteAll(struct sticky_route **);
extern void route_Clean(struct bundle *, struct sticky_route *);
extern void route_ShowSticky(struct prompt *, struct sticky_route *,
                             const char *, int);
extern void route_ParseHdr(struct rt_msghdr *, struct sockaddr *[RTAX_MAX]);
extern int rt_Set(struct bundle *, int, struct in_addr,
                           struct in_addr, struct in_addr, int, int);
extern void rt_Update(struct bundle *, struct in_addr, struct in_addr);

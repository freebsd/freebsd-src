/*-
 * Copyright (c) 1996 - 2001 Brian Somers <brian@Awfulhak.org>
 *          based on work by Toshiharu OHNO <tony-o@iij.ad.jp>
 *                           Internet Initiative Japan, Inc (IIJ)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
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

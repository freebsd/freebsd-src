/*
 *	      PPP Memory handling module
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
#include "defs.h"

struct memmap {
  struct mbuf *queue;
  int    count;
} MemMap[MB_MAX+2];

static int totalalloced;

int
plength(bp)
struct mbuf *bp;
{
  int len;

  for (len = 0; bp; bp = bp->next)
    len += bp->cnt;
  return(len);
}

struct mbuf *
mballoc(cnt, type)
int cnt;
int type;
{
  u_char *p;
  struct mbuf *bp;

  if (type > MB_MAX)
    logprintf("bad type %d\n", type);
  bp = (struct mbuf *)malloc(sizeof(struct mbuf));
  bzero(bp, sizeof(struct mbuf));
  p = (u_char *)malloc(cnt);
  MemMap[type].count += cnt;
  totalalloced += cnt;
  bp->base = p;
  bp->size = bp->cnt = cnt;
  bp->type = type;
  return(bp);
}

struct mbuf *
mbfree(struct mbuf *bp)
{
  struct mbuf *nbp;

  if (bp) {
    nbp = bp->next;
    MemMap[bp->type].count -= bp->size;
    totalalloced -= bp->size;
    free(bp->base);
    free(bp);
    return(nbp);
  }
  return(bp);
}

void
pfree(struct mbuf *bp)
{
  while (bp)
    bp = mbfree(bp);
}

struct mbuf *
mbread(bp, ptr, len)
struct mbuf *bp;
u_char *ptr;
int len;
{
  int nb;

  while (bp && len > 0) {
    if (len > bp->cnt)
      nb = bp->cnt;
    else
      nb = len;
    bcopy(MBUF_CTOP(bp), ptr, nb);
    ptr += nb;
    bp->cnt -= nb;
    len -= nb;
    bp->offset += nb;
    if (bp->cnt == 0) {
#ifdef notdef
      bp = bp->next;
#else
      bp = mbfree(bp);
#endif
    }
  }
  return(bp);
}

void
mbwrite(bp, ptr, cnt)
struct mbuf *bp;
u_char *ptr;
int cnt;
{
  int plen;
  int nb;

  plen = plength(bp);
  if (plen < cnt)
    cnt = plen;

  while (cnt > 0) {
    nb = (cnt < bp->cnt)? cnt : bp->cnt;
    bcopy(ptr, MBUF_CTOP(bp), nb);
    cnt -= bp->cnt;
    bp = bp->next;
  }
}

void
DumpBp(bp)
struct mbuf *bp;
{
  u_char *cp;
  int cnt, loc;

  logprintf("dump bp = %x (%d)\n", bp, plength(bp));
  loc = 0;
  while (bp) {
    cp = MBUF_CTOP(bp);
    cnt = bp->cnt;
    while (cnt > 0) {
      logprintf("%02x", *cp++);
      loc++;
      if (loc == 16) {
	loc = 0;
	logprintf("\n");
      } else
	logprintf(" ");
      cnt--;
    }
    bp = bp->next;
  }
  if (loc) logprintf("\n");
}

int
ShowMemMap()
{
  int i;

  for (i = 0; i <= MB_MAX; i += 2) {
    printf("%d: %d   %d: %d\r\n",
	i, MemMap[i].count, i+1, MemMap[i+1].count);
  }
  return(1);
}

void
LogMemory()
{
#ifdef DEBUG
  logprintf("mem alloced: %d\n", totalalloced);
  logprintf(" 1: %d  2: %d   3: %d   4: %d\n",
	MemMap[1].count, MemMap[2].count, MemMap[3].count, MemMap[4].count);
  logprintf(" 5: %d  6: %d   7: %d   8: %d\n",
	MemMap[5].count, MemMap[6].count, MemMap[7].count, MemMap[8].count);
#endif
}

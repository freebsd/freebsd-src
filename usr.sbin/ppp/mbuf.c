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
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>

#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "descriptor.h"
#include "prompt.h"
#include "main.h"

static struct memmap {
  struct mbuf *queue;
  int fragments, octets;
} MemMap[MB_MAX + 1];

static int totalalloced;
static unsigned long long mbuf_Mallocs, mbuf_Frees;

int
mbuf_Length(struct mbuf *bp)
{
  int len;

  for (len = 0; bp; bp = bp->next)
    len += bp->cnt;
  return len;
}

struct mbuf *
mbuf_Alloc(int cnt, int type)
{
  struct mbuf *bp;

  if (type > MB_MAX) {
    log_Printf(LogERROR, "Bad mbuf type %d\n", type);
    type = MB_UNKNOWN;
  }
  bp = malloc(sizeof(struct mbuf) + cnt);
  if (bp == NULL) {
    log_Printf(LogALERT, "failed to allocate memory: %ld\n",
               (long)sizeof(struct mbuf));
    AbortProgram(EX_OSERR);
  }
  mbuf_Mallocs++;
  memset(bp, '\0', sizeof(struct mbuf));
  MemMap[type].fragments++;
  MemMap[type].octets += cnt;
  totalalloced += cnt;
  bp->size = bp->cnt = cnt;
  bp->type = type;
  return bp;
}

struct mbuf *
mbuf_FreeSeg(struct mbuf *bp)
{
  struct mbuf *nbp;

  if (bp) {
    nbp = bp->next;
    MemMap[bp->type].fragments--;
    MemMap[bp->type].octets -= bp->size;
    totalalloced -= bp->size;
    free(bp);
    mbuf_Frees++;
    bp = nbp;
  }

  return bp;
}

void
mbuf_Free(struct mbuf *bp)
{
  while (bp)
    bp = mbuf_FreeSeg(bp);
}

struct mbuf *
mbuf_Read(struct mbuf *bp, void *v, size_t len)
{
  int nb;
  u_char *ptr = v;

  while (bp && len > 0) {
    if (len > bp->cnt)
      nb = bp->cnt;
    else
      nb = len;
    if (nb) {
      memcpy(ptr, MBUF_CTOP(bp), nb);
      ptr += nb;
      bp->cnt -= nb;
      len -= nb;
      bp->offset += nb;
    }
    if (bp->cnt == 0)
      bp = mbuf_FreeSeg(bp);
  }

  while (bp && bp->cnt == 0)
    bp = mbuf_FreeSeg(bp);

  return bp;
}

size_t
mbuf_View(struct mbuf *bp, void *v, size_t len)
{
  size_t nb, l = len;
  u_char *ptr = v;

  while (bp && l > 0) {
    if (l > bp->cnt)
      nb = bp->cnt;
    else
      nb = l;
    memcpy(ptr, MBUF_CTOP(bp), nb);
    ptr += nb;
    l -= nb;
    bp = bp->next;
  }

  return len - l;
}

struct mbuf *
mbuf_Prepend(struct mbuf *bp, const void *ptr, size_t len, size_t extra)
{
  struct mbuf *head;

  if (bp && bp->offset) {
    if (bp->offset >= len) {
      bp->offset -= len;
      bp->cnt += len;
      memcpy(MBUF_CTOP(bp), ptr, len);
      return bp;
    }
    len -= bp->offset;
    memcpy(bp + sizeof *bp, (const char *)ptr + len, bp->offset);
    bp->cnt += bp->offset;
    bp->offset = 0;
  }

  head = mbuf_Alloc(len + extra, bp ? bp->type : MB_UNKNOWN);
  head->offset = extra;
  head->cnt -= extra;
  memcpy(MBUF_CTOP(head), ptr, len);
  head->next = bp;

  return head;
}

struct mbuf *
mbuf_Truncate(struct mbuf *bp, size_t n)
{
  if (n == 0) {
    mbuf_Free(bp);
    return NULL;
  }

  for (; bp; bp = bp->next, n -= bp->cnt)
    if (n < bp->cnt) {
      bp->cnt = n;
      mbuf_Free(bp->next);
      bp->next = NULL;
      break;
    }

  return bp;
}

void
mbuf_Write(struct mbuf *bp, const void *ptr, size_t cnt)
{
  int plen;
  int nb;

  plen = mbuf_Length(bp);
  if (plen < cnt)
    cnt = plen;

  while (cnt > 0) {
    nb = (cnt < bp->cnt) ? cnt : bp->cnt;
    memcpy(MBUF_CTOP(bp), ptr, nb);
    cnt -= bp->cnt;
    bp = bp->next;
  }
}

int
mbuf_Show(struct cmdargs const *arg)
{
  int i;
  static const char *mbuftype[] = { 
    "ip in", "ip out", "nat in", "nat out", "mp in", "mp out",
    "vj in", "vj out", "icompd in", "icompd out", "compd in", "compd out",
    "lqr in", "lqr out", "echo in", "echo out", "proto in", "proto out",
    "acf in", "acf out", "sync in", "sync out", "hdlc in", "hdlc out",
    "async in", "async out", "cbcp in", "cbcp out", "chap in", "chap out",
    "pap in", "pap out", "ccp in", "ccp out", "ipcp in", "ipcp out",
    "lcp in", "lcp out", "unknown"
  };

  prompt_Printf(arg->prompt, "Fragments (octets) in use:\n");
  for (i = 0; i < MB_MAX; i += 2)
    prompt_Printf(arg->prompt, "%10.10s: %04d (%06d)\t%10.10s: %04d (%06d)\n",
	    mbuftype[i], MemMap[i].fragments, MemMap[i].octets,
            mbuftype[i+1], MemMap[i+1].fragments, MemMap[i+1].octets);

  if (i == MB_MAX)
    prompt_Printf(arg->prompt, "%10.10s: %04d (%06d)\n",
                  mbuftype[i], MemMap[i].fragments, MemMap[i].octets);

  prompt_Printf(arg->prompt, "Mallocs: %llu,   Frees: %llu\n",
                mbuf_Mallocs, mbuf_Frees);

  return 0;
}

struct mbuf *
mbuf_Dequeue(struct mqueue *q)
{
  struct mbuf *bp;
  
  log_Printf(LogDEBUG, "mbuf_Dequeue: queue len = %d\n", q->qlen);
  bp = q->top;
  if (bp) {
    q->top = q->top->pnext;
    q->qlen--;
    if (q->top == NULL) {
      q->last = q->top;
      if (q->qlen)
	log_Printf(LogERROR, "mbuf_Dequeue: Not zero (%d)!!!\n", q->qlen);
    }
    bp->pnext = NULL;
  }

  return bp;
}

void
mbuf_Enqueue(struct mqueue *queue, struct mbuf *bp)
{
  if (bp != NULL) {
    if (queue->last) {
      queue->last->pnext = bp;
      queue->last = bp;
    } else
      queue->last = queue->top = bp;
    queue->qlen++;
    log_Printf(LogDEBUG, "mbuf_Enqueue: len = %d\n", queue->qlen);
  }
}

struct mbuf *
mbuf_Contiguous(struct mbuf *bp)
{
  /* Put it all in one contigous (aligned) mbuf */

  if (bp != NULL) {
    if (bp->next != NULL) {
      struct mbuf *nbp;
      u_char *cp;

      nbp = mbuf_Alloc(mbuf_Length(bp), bp->type);

      for (cp = MBUF_CTOP(nbp); bp; bp = mbuf_FreeSeg(bp)) {
        memcpy(cp, MBUF_CTOP(bp), bp->cnt);
        cp += bp->cnt;
      }
      bp = nbp;
    }
#ifndef __i386__	/* Do any other archs not care about alignment ? */
    else if ((bp->offset & (sizeof(long) - 1)) != 0) {
      bcopy(MBUF_CTOP(bp), bp + 1, bp->cnt);
      bp->offset = 0;
    }
#endif
  }

  return bp;
}

void
mbuf_SetType(struct mbuf *bp, int type)
{
  for (; bp; bp = bp->next)
    if (type != bp->type) {
      MemMap[bp->type].fragments--;
      MemMap[bp->type].octets -= bp->size;
      bp->type = type;
      MemMap[type].fragments++;
      MemMap[type].octets += bp->size;
    }
}

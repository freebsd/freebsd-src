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
  size_t fragments;
  size_t octets;
} MemMap[MB_MAX + 1];

static int totalalloced;
static unsigned long long mbuf_Mallocs, mbuf_Frees;

int
m_length(struct mbuf *bp)
{
  int len;

  for (len = 0; bp; bp = bp->m_next)
    len += bp->m_len;
  return len;
}

struct mbuf *
m_get(size_t m_len, int type)
{
  struct mbuf *bp;

  if (type > MB_MAX) {
    log_Printf(LogERROR, "Bad mbuf type %d\n", type);
    type = MB_UNKNOWN;
  }
  bp = malloc(sizeof *bp + m_len);
  if (bp == NULL) {
    log_Printf(LogALERT, "failed to allocate memory: %ld\n",
               (long)sizeof(struct mbuf));
    AbortProgram(EX_OSERR);
  }
  mbuf_Mallocs++;
  memset(bp, '\0', sizeof(struct mbuf));
  MemMap[type].fragments++;
  MemMap[type].octets += m_len;
  totalalloced += m_len;
  bp->m_size = bp->m_len = m_len;
  bp->m_type = type;
  return bp;
}

struct mbuf *
m_free(struct mbuf *bp)
{
  struct mbuf *nbp;

  if (bp) {
    nbp = bp->m_next;
    MemMap[bp->m_type].fragments--;
    MemMap[bp->m_type].octets -= bp->m_size;
    totalalloced -= bp->m_size;
    free(bp);
    mbuf_Frees++;
    bp = nbp;
  }

  return bp;
}

void
m_freem(struct mbuf *bp)
{
  while (bp)
    bp = m_free(bp);
}

struct mbuf *
mbuf_Read(struct mbuf *bp, void *v, size_t len)
{
  int nb;
  u_char *ptr = v;

  while (bp && len > 0) {
    if (len > bp->m_len)
      nb = bp->m_len;
    else
      nb = len;
    if (nb) {
      memcpy(ptr, MBUF_CTOP(bp), nb);
      ptr += nb;
      bp->m_len -= nb;
      len -= nb;
      bp->m_offset += nb;
    }
    if (bp->m_len == 0)
      bp = m_free(bp);
  }

  while (bp && bp->m_len == 0)
    bp = m_free(bp);

  return bp;
}

size_t
mbuf_View(struct mbuf *bp, void *v, size_t len)
{
  size_t nb, l = len;
  u_char *ptr = v;

  while (bp && l > 0) {
    if (l > bp->m_len)
      nb = bp->m_len;
    else
      nb = l;
    memcpy(ptr, MBUF_CTOP(bp), nb);
    ptr += nb;
    l -= nb;
    bp = bp->m_next;
  }

  return len - l;
}

struct mbuf *
m_prepend(struct mbuf *bp, const void *ptr, size_t len, size_t extra)
{
  struct mbuf *head;

  if (bp && bp->m_offset) {
    if (bp->m_offset >= len) {
      bp->m_offset -= len;
      bp->m_len += len;
      memcpy(MBUF_CTOP(bp), ptr, len);
      return bp;
    }
    len -= bp->m_offset;
    memcpy(bp + 1, (const char *)ptr + len, bp->m_offset);
    bp->m_len += bp->m_offset;
    bp->m_offset = 0;
  }

  head = m_get(len + extra, bp ? bp->m_type : MB_UNKNOWN);
  head->m_offset = extra;
  head->m_len -= extra;
  memcpy(MBUF_CTOP(head), ptr, len);
  head->m_next = bp;

  return head;
}

struct mbuf *
m_adj(struct mbuf *bp, ssize_t n)
{
  if (n > 0) {
    while (bp) {
      if (n < bp->m_len) {
        bp->m_len = n;
        bp->m_offset += n;
        return bp;
      }
      n -= bp->m_len;
      bp = m_free(bp);
    }
  } else {
    if ((n = m_length(bp) + n) <= 0) {
      m_freem(bp);
      return NULL;
    }
    for (; bp; bp = bp->m_next, n -= bp->m_len)
      if (n < bp->m_len) {
        bp->m_len = n;
        m_freem(bp->m_next);
        bp->m_next = NULL;
        break;
      }
  }

  return bp;
}

void
mbuf_Write(struct mbuf *bp, const void *ptr, size_t m_len)
{
  int plen;
  int nb;

  plen = m_length(bp);
  if (plen < m_len)
    m_len = plen;

  while (m_len > 0) {
    nb = (m_len < bp->m_len) ? m_len : bp->m_len;
    memcpy(MBUF_CTOP(bp), ptr, nb);
    m_len -= bp->m_len;
    bp = bp->m_next;
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
    prompt_Printf(arg->prompt, "%10.10s: %04lu (%06lu)\t"
                  "%10.10s: %04lu (%06lu)\n",
	          mbuftype[i], (u_long)MemMap[i].fragments,
                  (u_long)MemMap[i].octets, mbuftype[i+1],
                  (u_long)MemMap[i+1].fragments, (u_long)MemMap[i+1].octets);

  if (i == MB_MAX)
    prompt_Printf(arg->prompt, "%10.10s: %04lu (%06lu)\n",
                  mbuftype[i], (u_long)MemMap[i].fragments,
                  (u_long)MemMap[i].octets);

  prompt_Printf(arg->prompt, "Mallocs: %llu,   Frees: %llu\n",
                mbuf_Mallocs, mbuf_Frees);

  return 0;
}

struct mbuf *
m_dequeue(struct mqueue *q)
{
  struct mbuf *bp;
  
  log_Printf(LogDEBUG, "m_dequeue: queue len = %lu\n", (u_long)q->len);
  bp = q->top;
  if (bp) {
    q->top = q->top->m_nextpkt;
    q->len--;
    if (q->top == NULL) {
      q->last = q->top;
      if (q->len)
	log_Printf(LogERROR, "m_dequeue: Not zero (%lu)!!!\n",
                   (u_long)q->len);
    }
    bp->m_nextpkt = NULL;
  }

  return bp;
}

void
m_enqueue(struct mqueue *queue, struct mbuf *bp)
{
  if (bp != NULL) {
    if (queue->last) {
      queue->last->m_nextpkt = bp;
      queue->last = bp;
    } else
      queue->last = queue->top = bp;
    queue->len++;
    log_Printf(LogDEBUG, "m_enqueue: len = %d\n", queue->len);
  }
}

struct mbuf *
m_pullup(struct mbuf *bp)
{
  /* Put it all in one contigous (aligned) mbuf */

  if (bp != NULL) {
    if (bp->m_next != NULL) {
      struct mbuf *nbp;
      u_char *cp;

      nbp = m_get(m_length(bp), bp->m_type);

      for (cp = MBUF_CTOP(nbp); bp; bp = m_free(bp)) {
        memcpy(cp, MBUF_CTOP(bp), bp->m_len);
        cp += bp->m_len;
      }
      bp = nbp;
    }
#ifndef __i386__	/* Do any other archs not care about alignment ? */
    else if ((bp->m_offset & (sizeof(long) - 1)) != 0) {
      bcopy(MBUF_CTOP(bp), bp + 1, bp->m_len);
      bp->m_offset = 0;
    }
#endif
  }

  return bp;
}

void
m_settype(struct mbuf *bp, int type)
{
  for (; bp; bp = bp->m_next)
    if (type != bp->m_type) {
      MemMap[bp->m_type].fragments--;
      MemMap[bp->m_type].octets -= bp->m_size;
      bp->m_type = type;
      MemMap[type].fragments++;
      MemMap[type].octets += bp->m_size;
    }
}

/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
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
 *  $Id: link.c,v 1.1.2.20 1998/05/01 19:24:59 brian Exp $
 *
 */

#include <sys/types.h>

#include <stdio.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "lcpproto.h"
#include "fsm.h"
#include "descriptor.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "prompt.h"

void
link_AddInOctets(struct link *l, int n)
{
  throughput_addin(&l->throughput, n);
}

void
link_AddOutOctets(struct link *l, int n)
{
  throughput_addout(&l->throughput, n);
}

void
link_SequenceQueue(struct link *l)
{
  log_Printf(LogDEBUG, "link_SequenceQueue\n");
  while (l->Queue[PRI_NORMAL].qlen)
    mbuf_Enqueue(l->Queue + PRI_LINK, mbuf_Dequeue(l->Queue + PRI_NORMAL));
}

int
link_QueueLen(struct link *l)
{
  int i, len;

  for (i = 0, len = 0; i < LINK_QUEUES; i++)
    len += l->Queue[i].qlen;

  return len;
}

int
link_QueueBytes(struct link *l)
{
  int i, len, bytes;
  struct mbuf *m;

  bytes = 0;
  for (i = 0, len = 0; i < LINK_QUEUES; i++) {
    len = l->Queue[i].qlen;
    m = l->Queue[i].top;
    while (len--) {
      bytes += mbuf_Length(m);
      m = m->pnext;
    }
  }

  return bytes;
}

struct mbuf *
link_Dequeue(struct link *l)
{
  int pri;
  struct mbuf *bp;

  for (bp = (struct mbuf *)0, pri = LINK_QUEUES - 1; pri >= 0; pri--)
    if (l->Queue[pri].qlen) {
      bp = mbuf_Dequeue(l->Queue + pri);
      log_Printf(LogDEBUG, "link_Dequeue: Dequeued from queue %d,"
                " containing %d more packets\n", pri, l->Queue[pri].qlen);
      break;
    }

  return bp;
}

/*
 * Write to the link. Actualy, requested packets are queued, and go out
 * at some later time depending on the physical link implementation.
 */
void
link_Write(struct link *l, int pri, const char *ptr, int count)
{
  struct mbuf *bp;

  if(pri < 0 || pri >= LINK_QUEUES)
    pri = 0;

  bp = mbuf_Alloc(count, MB_LINK);
  memcpy(MBUF_CTOP(bp), ptr, count);

  mbuf_Enqueue(l->Queue + pri, bp);
}

void
link_Output(struct link *l, int pri, struct mbuf *bp)
{
  struct mbuf *wp;
  int len;

  if(pri < 0 || pri >= LINK_QUEUES)
    pri = 0;

  len = mbuf_Length(bp);
  wp = mbuf_Alloc(len, MB_LINK);
  mbuf_Read(bp, MBUF_CTOP(wp), len);
  mbuf_Enqueue(l->Queue + pri, wp);
}

static struct protostatheader {
  u_short number;
  const char *name;
} ProtocolStat[NPROTOSTAT] = {
  { PROTO_IP, "IP" },
  { PROTO_VJUNCOMP, "VJ_UNCOMP" },
  { PROTO_VJCOMP, "VJ_COMP" },
  { PROTO_COMPD, "COMPD" },
  { PROTO_ICOMPD, "ICOMPD" },
  { PROTO_LCP, "LCP" },
  { PROTO_IPCP, "IPCP" },
  { PROTO_CCP, "CCP" },
  { PROTO_PAP, "PAP" },
  { PROTO_LQR, "LQR" },
  { PROTO_CHAP, "CHAP" },
  { PROTO_MP, "MULTILINK" },
  { 0, "Others" }
};

void
link_ProtocolRecord(struct link *l, u_short proto, int type)
{
  int i;

  for (i = 0; i < NPROTOSTAT; i++)
    if (ProtocolStat[i].number == proto)
      break;

  if (type == PROTO_IN)
    l->proto_in[i]++;
  else
    l->proto_out[i]++;
}

void
link_ReportProtocolStatus(struct link *l, struct prompt *prompt)
{
  int i;

  prompt_Printf(prompt, "    Protocol     in        out      "
                "Protocol      in       out\n");
  for (i = 0; i < NPROTOSTAT; i++) {
    prompt_Printf(prompt, "   %-9s: %8lu, %8lu",
	    ProtocolStat[i].name, l->proto_in[i], l->proto_out[i]);
    if ((i % 2) == 0)
      prompt_Printf(prompt, "\n");
  }
  if (!(i % 2))
    prompt_Printf(prompt, "\n");
}

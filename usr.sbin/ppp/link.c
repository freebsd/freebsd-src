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
 *  $Id: link.c,v 1.1.2.9 1998/02/27 01:22:36 brian Exp $
 *
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <termios.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "lcpproto.h"
#include "loadalias.h"
#include "vars.h"
#include "link.h"
#include "fsm.h"
#include "bundle.h"
#include "descriptor.h"
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
  LogPrintf(LogDEBUG, "link_SequenceQueue\n");
  while (l->Queue[PRI_NORMAL].qlen)
    Enqueue(l->Queue + PRI_LINK, Dequeue(l->Queue + PRI_NORMAL));
}

int
link_QueueLen(struct link *l)
{
  int i, len;

  for (i = 0, len = 0; i < LINK_QUEUES; i++)
    len += l->Queue[i].qlen;

  return len;
}

struct mbuf *
link_Dequeue(struct link *l)
{
  int pri;
  struct mbuf *bp;

  for (bp = (struct mbuf *)0, pri = LINK_QUEUES - 1; pri >= 0; pri--)
    if (l->Queue[pri].qlen) {
      bp = Dequeue(l->Queue + pri);
      LogPrintf(LogDEBUG, "link_Dequeue: Dequeued from queue %d,"
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

  bp = mballoc(count, MB_LINK);
  memcpy(MBUF_CTOP(bp), ptr, count);

  Enqueue(l->Queue + pri, bp);
}

void
link_StartOutput(struct link *l, struct bundle *bundle)
{
  (*l->StartOutput)(l, bundle);
}

void
link_Output(struct link *l, int pri, struct mbuf *bp)
{
  struct mbuf *wp;
  int len;

  if(pri < 0 || pri >= LINK_QUEUES)
    pri = 0;

  len = plength(bp);
  wp = mballoc(len, MB_LINK);
  mbread(bp, MBUF_CTOP(wp), len);
  Enqueue(l->Queue + pri, wp);
}

int
link_IsActive(struct link *l)
{
  return (*l->IsActive)(l);
}

void
link_Close(struct link *l, struct bundle *bundle, int dedicated_force, int stay)
{
  (*l->Close)(l, dedicated_force);
  bundle_LinkLost(bundle, l, stay);
}

void
link_Destroy(struct link *l)
{
  (*l->Destroy)(l);
}

static struct protostatheader {
  u_short number;
  const char *name;
} ProtocolStat[NPROTOSTAT] = {
  { PROTO_IP, "IP" },
  { PROTO_VJUNCOMP, "VJ_UNCOMP" },
  { PROTO_VJCOMP, "VJ_COMP" },
  { PROTO_COMPD, "COMPD" },
  { PROTO_LCP, "LCP" },
  { PROTO_IPCP, "IPCP" },
  { PROTO_CCP, "CCP" },
  { PROTO_PAP, "PAP" },
  { PROTO_LQR, "LQR" },
  { PROTO_CHAP, "CHAP" },
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
link_ReportProtocolStatus(struct link *l)
{
  int i;

  prompt_Printf(&prompt, "    Protocol     in        out      "
                "Protocol      in       out\n");
  for (i = 0; i < NPROTOSTAT; i++) {
    prompt_Printf(&prompt, "   %-9s: %8lu, %8lu",
	    ProtocolStat[i].name, l->proto_in[i], l->proto_out[i]);
    if ((i % 2) == 0)
      prompt_Printf(&prompt, "\n");
  }
  if (i % 2)
    prompt_Printf(&prompt, "\n");
}

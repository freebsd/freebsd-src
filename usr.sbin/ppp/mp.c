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
 *	$Id: mp.c,v 1.1.2.4 1998/04/16 00:26:11 brian Exp $
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "auth.h"
#include "lcp.h"
#include "lqr.h"
#include "hdlc.h"
#include "async.h"
#include "ccp.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "chat.h"
#include "lcpproto.h"
#include "filter.h"
#include "mp.h"
#include "chap.h"
#include "datalink.h"
#include "bundle.h"
#include "ip.h"

static u_int32_t
inc_seq(struct mp *mp, u_int32_t seq)
{
  seq++;
  if (mp->is12bit) {
    if (seq & 0xfffff000)
      seq = 0;
  } else if (seq & 0xff000000)
    seq = 0;
  return seq;
}

static int
mp_ReadHeader(struct mp *mp, struct mbuf *m, struct mp_header *header)
{
  if (mp->is12bit) {
    header->seq = *(u_int16_t *)MBUF_CTOP(m);
    if (header->seq & 0x3000) {
      LogPrintf(LogWARN, "Oops - MP header without required zero bits\n");
      return 0;
    }
    header->begin = header->seq & 0x8000;
    header->end = header->seq & 0x4000;
    header->seq &= 0x0fff;
    return 2;
  } else {
    header->seq = *(u_int32_t *)MBUF_CTOP(m);
    if (header->seq & 0x3f000000) {
      LogPrintf(LogWARN, "Oops - MP header without required zero bits\n");
      return 0;
    }
    header->begin = header->seq & 0x80000000;
    header->end = header->seq & 0x40000000;
    header->seq &= 0x00ffffff;
    return 4;
  }
}

static void
mp_LayerStart(void *v, struct fsm *fp)
{
  /* The given FSM is about to start up ! */
}

static void
mp_LayerUp(void *v, struct fsm *fp)
{
  /* The given fsm is now up */
}

static void
mp_LayerDown(void *v, struct fsm *fp)
{
  /* The given FSM has been told to come down */
}

static void
mp_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm is now down */
}

void
mp_Init(struct mp *mp, struct bundle *bundle)
{
  mp->is12bit = 0;
  mp->seq.out = 0;
  mp->seq.min_in = 0;
  mp->seq.next_in = 0;
  mp->inbufs = NULL;
  mp->bundle = bundle;

  mp->link.type = MP_LINK;
  mp->link.name = "mp";
  mp->link.len = sizeof *mp;
  throughput_init(&mp->link.throughput);
  memset(&mp->link.Timer, '\0', sizeof mp->link.Timer);
  memset(mp->link.Queue, '\0', sizeof mp->link.Queue);
  memset(mp->link.proto_in, '\0', sizeof mp->link.proto_in);
  memset(mp->link.proto_out, '\0', sizeof mp->link.proto_out);

  mp->fsmp.LayerStart = mp_LayerStart;
  mp->fsmp.LayerUp = mp_LayerUp;
  mp->fsmp.LayerDown = mp_LayerDown;
  mp->fsmp.LayerFinish = mp_LayerFinish;
  mp->fsmp.object = mp;

  lcp_Init(&mp->link.lcp, mp->bundle, &mp->link, NULL);
  ccp_Init(&mp->link.ccp, mp->bundle, &mp->link, &mp->fsmp);

  /* Our lcp's already up 'cos of the NULL parent */
  FsmUp(&mp->link.ccp.fsm);
  FsmOpen(&mp->link.ccp.fsm);

  mp->active = 1;

  bundle_LayerUp(mp->bundle, &mp->link.lcp.fsm);
}

void
mp_linkInit(struct mp_link *mplink)
{
  mplink->seq = 0;
  mplink->weight = 1500;
}

void
mp_Input(struct mp *mp, struct mbuf *m, struct physical *p)
{
  struct mp_header mh, h;
  struct mbuf *q, *last;
  int32_t seq;

  if (mp_ReadHeader(mp, m, &mh) == 0) {
    pfree(m);
    return;
  }

  seq = p->dl->mp.seq;
  p->dl->mp.seq = mh.seq;
  if (mp->seq.min_in == seq) {
    /*
     * We've received new data on the link that has our min (oldest) seq.
     * Figure out which link now has the smallest (oldest) seq.
     */
    struct datalink *dl;

    mp->seq.min_in = p->dl->mp.seq;
    for (dl = mp->bundle->links; dl; dl = dl->next)
      if (mp->seq.min_in > dl->mp.seq)
        mp->seq.min_in = dl->mp.seq;
  }

  /*
   * Now process as many of our fragments as we can, adding our new
   * fragment in as we go, and ordering with the oldest at the top of
   * the queue.
   */

  if (!mp->inbufs) {
    mp->inbufs = m;
    m = NULL;
  }

  last = NULL;
  seq = mp->seq.next_in;
  q = mp->inbufs;
  while (q) {
    mp_ReadHeader(mp, q, &h);
    if (m && h.seq > mh.seq) {
      /* Our received fragment fits in before this one, so link it in */
      if (last)
        last->pnext = m;
      else
        mp->inbufs = m;
      m->pnext = q;
      q = m;
      h = mh;
      m = NULL;
    }

    if (h.seq != seq) {
      /* we're missing something :-( */
      if (mp->seq.min_in > seq) {
        /* we're never gonna get it */
        struct mbuf *next;

        /* Zap all older fragments */
        while (mp->inbufs != q) {
          next = mp->inbufs->pnext;
          pfree(mp->inbufs);
          mp->inbufs = next;
        }

        /*
         * Zap everything until the next `end' fragment OR just before
         * the next `begin' fragment OR 'till seq.min_in - whichever
         * comes first.
         */
        do {
          mp_ReadHeader(mp, mp->inbufs, &h);
          if (h.begin) {
            h.seq--;  /* We're gonna look for fragment with h.seq+1 */
            break;
          }
          next = mp->inbufs->pnext;
          pfree(mp->inbufs);
          mp->inbufs = next;
        } while (h.seq >= mp->seq.min_in || h.end);

        /*
         * Continue processing things from here.
         * This deals with the possibility that we received a fragment
         * on the slowest link that invalidates some of our data (because
         * of the hole at `q'), but where there are subsequent `whole'
         * packets that have already been received.
         */

        mp->seq.next_in = seq = h.seq + 1;
        last = NULL;
        q = mp->inbufs;
      } else
        /* we may still receive the missing fragment */
        break;
    } else if (h.end) {
      /* We've got something, reassemble */
      struct mbuf **frag = &q;
      int len;
      u_short proto = 0;
      u_char ch;

      do {
        *frag = mp->inbufs;
        mp->inbufs = mp->inbufs->pnext;
        len = mp_ReadHeader(mp, *frag, &h);
        (*frag)->offset += len;
        (*frag)->cnt -= len;
        (*frag)->pnext = NULL;
        if (frag == &q && !h.begin) {
          LogPrintf(LogWARN, "Oops - MP frag %lu should have a begin flag\n",
                    (u_long)h.seq);
          pfree(q);
          q = NULL;
        } else if (frag != &q && h.begin) {
          LogPrintf(LogWARN, "Oops - MP frag %lu should have an end flag\n",
                    (u_long)h.seq - 1);
          /*
           * Stuff our fragment back at the front of the queue and zap
           * our half-assembed packet.
           */
          (*frag)->pnext = mp->inbufs;
          mp->inbufs = *frag;
          *frag = NULL;
          pfree(q);
          q = NULL;
          frag = &q;
          h.end = 0;	/* just in case it's a whole packet */
        } else
          do
            frag = &(*frag)->next;
          while ((*frag)->next != NULL);
      } while (!h.end);

      if (q) {
        do {
          q = mbread(q, &ch, 1);
          proto = proto << 8;
          proto += ch;
        } while (!(proto & 1));
        hdlc_DecodePacket(mp->bundle, proto, q, &mp->link);
      }

      mp->seq.next_in = seq = h.seq + 1;
      last = NULL;
      q = mp->inbufs;
    } else {
      /* Look for the next fragment */
      seq++;
      last = q;
      q = q->pnext;
    }
  }

  if (m) {
    /* We still have to find a home for our new fragment */
    last = NULL;
    for (q = mp->inbufs; q; last = q, q = q->pnext) {
      mp_ReadHeader(mp, q, &h);
      if (h.seq > mh.seq) {
        /* Our received fragment fits in before this one, so link it in */
        if (last)
          last->pnext = m;
        else
          mp->inbufs = m;
        m->pnext = q;
        break;
      }
    }
  }
}

static void
mp_Output(struct mp *mp, struct link *l, struct mbuf *m, int begin, int end)
{
  struct mbuf *mo;
  u_char *cp;
  u_int32_t *seq32;
  u_int16_t *seq16;

  mo = mballoc(4, MB_MP);
  mo->next = m;
  cp = MBUF_CTOP(mo);
  seq32 = (u_int32_t *)cp;
  seq16 = (u_int16_t *)cp;
  *seq32 = 0;
  *cp = (begin << 7) | (end << 6);
  if (mp->is12bit) {
    *seq16 |= (u_int16_t)mp->seq.out;
    mo->cnt = 2;
  } else {
    *seq32 |= (u_int32_t)mp->seq.out;
    mo->cnt = 4;
  }
  mp->seq.out = inc_seq(mp, mp->seq.out);

  HdlcOutput(l, PRI_NORMAL, PROTO_MP, mo);
}

int
mp_FillQueues(struct bundle *bundle)
{
  struct mp *mp = &bundle->ncp.mp;
  struct datalink *dl;
  int total, add, len, begin, end, looped;
  struct mbuf *m, *mo;

  /*
   * XXX:  This routine is fairly simplistic.  It should re-order the
   *       links based on the amount of data less than the links weight
   *       that was queued.  That way we'd ``prefer'' the least used
   *       links the next time 'round.
   */

  total = 0;
  for (dl = bundle->links; dl; dl = dl->next) {
    if (dl->physical->out)
      /* this link has suffered a short write.  Let it continue */
      continue;
    add = link_QueueLen(&dl->physical->link);
    total += add;
    if (add)
      /* this link has got stuff already queued.  Let it continue */
      continue;
    if (!link_QueueLen(&mp->link) && !IpFlushPacket(&mp->link, bundle))
      /* Nothing else to send */
      break;

    m = link_Dequeue(&mp->link);
    len = plength(m);
    add += len;
    begin = 1;
    end = 0;
    looped = 0;

    for (; !end; dl = dl->next) {
      if (dl == NULL) {
        /* Keep going 'till we get rid of the whole of `m' */
        looped = 1;
        dl = bundle->links;
      }
      if (len <= dl->mp.weight + LINK_MINWEIGHT) {
        mo = m;
        end = 1;
      } else {
        mo = mballoc(dl->mp.weight, MB_MP);
        mo->cnt = dl->mp.weight;
        len -= mo->cnt;
        m = mbread(m, MBUF_CTOP(mo), mo->cnt);
      }
      mp_Output(mp, &dl->physical->link, mo, begin, end);
      begin = 0;
    }
    if (looped)
      break;
  }

  return total;
}

int
mp_SetDatalinkWeight(struct cmdargs const *arg)
{
  int val;

  if (arg->argc != arg->argn+1)
    return -1;
  
  val = atoi(arg->argv[arg->argn]);
  if (val < LINK_MINWEIGHT) {
    LogPrintf(LogWARN, "Link weights must not be less than %d\n",
              LINK_MINWEIGHT);
    return 1;
  }
  arg->cx->mp.weight = val;
  return 0;
}

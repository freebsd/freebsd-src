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
 *	$Id: mp.c,v 1.2 1998/05/21 21:47:05 brian Exp $
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <paths.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

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
#include "prompt.h"
#include "id.h"
#include "arp.h"

void
peerid_Init(struct peerid *peer)
{
  peer->enddisc.class = 0;
  *peer->enddisc.address = '\0';
  peer->enddisc.len = 0;
  *peer->authname = '\0';
}

int
peerid_Equal(const struct peerid *p1, const struct peerid *p2)
{
  return !strcmp(p1->authname, p2->authname) &&
         p1->enddisc.class == p2->enddisc.class &&
         p1->enddisc.len == p2->enddisc.len &&
         !memcmp(p1->enddisc.address, p2->enddisc.address, p1->enddisc.len);
}

static u_int32_t
inc_seq(unsigned is12bit, u_int32_t seq)
{
  seq++;
  if (is12bit) {
    if (seq & 0xfffff000)
      seq = 0;
  } else if (seq & 0xff000000)
    seq = 0;
  return seq;
}

static int
isbefore(unsigned is12bit, u_int32_t seq1, u_int32_t seq2)
{
  u_int32_t max = (is12bit ? 0xfff : 0xffffff) - 0x200;

  if (seq1 > max) {
    if (seq2 < 0x200 || seq2 > seq1)
      return 1;
  } else if ((seq1 > 0x200 || seq2 <= max) && seq1 < seq2)
    return 1;

  return 0;
}

static int
mp_ReadHeader(struct mp *mp, struct mbuf *m, struct mp_header *header)
{
  if (mp->local_is12bit) {
    header->seq = ntohs(*(u_int16_t *)MBUF_CTOP(m));
    if (header->seq & 0x3000) {
      log_Printf(LogWARN, "Oops - MP header without required zero bits\n");
      return 0;
    }
    header->begin = header->seq & 0x8000 ? 1 : 0;
    header->end = header->seq & 0x4000 ? 1 : 0;
    header->seq &= 0x0fff;
    return 2;
  } else {
    header->seq = ntohl(*(u_int32_t *)MBUF_CTOP(m));
    if (header->seq & 0x3f000000) {
      log_Printf(LogWARN, "Oops - MP header without required zero bits\n");
      return 0;
    }
    header->begin = header->seq & 0x80000000 ? 1 : 0;
    header->end = header->seq & 0x40000000 ? 1 : 0;
    header->seq &= 0x00ffffff;
    return 4;
  }
}

static void
mp_LayerStart(void *v, struct fsm *fp)
{
  /* The given FSM (ccp) is about to start up ! */
}

static void
mp_LayerUp(void *v, struct fsm *fp)
{
  /* The given fsm (ccp) is now up */
}

static void
mp_LayerDown(void *v, struct fsm *fp)
{
  /* The given FSM (ccp) has been told to come down */
}

static void
mp_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm (ccp) is now down */
  if (fp->state == ST_CLOSED && fp->open_mode == OPEN_PASSIVE)
    fsm_Open(fp);		/* CCP goes to ST_STOPPED */
}

void
mp_Init(struct mp *mp, struct bundle *bundle)
{
  mp->peer_is12bit = mp->local_is12bit = 0;
  mp->peer_mrru = mp->local_mrru = 0;

  peerid_Init(&mp->peer);

  mp->out.seq = 0;
  mp->out.link = 0;
  mp->seq.min_in = 0;
  mp->seq.next_in = 0;
  mp->inbufs = NULL;
  mp->bundle = bundle;

  mp->link.type = MP_LINK;
  mp->link.name = "mp";
  mp->link.len = sizeof *mp;

  throughput_init(&mp->link.throughput);
  memset(mp->link.Queue, '\0', sizeof mp->link.Queue);
  memset(mp->link.proto_in, '\0', sizeof mp->link.proto_in);
  memset(mp->link.proto_out, '\0', sizeof mp->link.proto_out);

  mp->fsmp.LayerStart = mp_LayerStart;
  mp->fsmp.LayerUp = mp_LayerUp;
  mp->fsmp.LayerDown = mp_LayerDown;
  mp->fsmp.LayerFinish = mp_LayerFinish;
  mp->fsmp.object = mp;

  mpserver_Init(&mp->server);

  mp->cfg.mrru = 0;
  mp->cfg.shortseq = NEG_ENABLED|NEG_ACCEPTED;
  mp->cfg.enddisc.class = 0;
  *mp->cfg.enddisc.address = '\0';
  mp->cfg.enddisc.len = 0;

  lcp_Init(&mp->link.lcp, mp->bundle, &mp->link, NULL);
  ccp_Init(&mp->link.ccp, mp->bundle, &mp->link, &mp->fsmp);
}

int
mp_Up(struct mp *mp, struct datalink *dl)
{
  struct lcp *lcp = &dl->physical->link.lcp;

  if (mp->active) {
    /* We're adding a link - do a last validation on our parameters */
    if (!peerid_Equal(&dl->peer, &mp->peer)) {
      log_Printf(LogPHASE, "%s: Inappropriate peer !\n", dl->name);
      return MP_FAILED;
    }
    if (mp->local_mrru != lcp->want_mrru ||
        mp->peer_mrru != lcp->his_mrru ||
        mp->local_is12bit != lcp->want_shortseq ||
        mp->peer_is12bit != lcp->his_shortseq) {
      log_Printf(LogPHASE, "%s: Invalid MRRU/SHORTSEQ MP parameters !\n",
                dl->name);
      return MP_FAILED;
    }
    return MP_ADDED;
  } else {
    /* First link in multilink mode */

    mp->local_mrru = lcp->want_mrru;
    mp->peer_mrru = lcp->his_mrru;
    mp->local_is12bit = lcp->want_shortseq;
    mp->peer_is12bit = lcp->his_shortseq;
    mp->peer = dl->peer;

    throughput_init(&mp->link.throughput);
    memset(mp->link.Queue, '\0', sizeof mp->link.Queue);
    memset(mp->link.proto_in, '\0', sizeof mp->link.proto_in);
    memset(mp->link.proto_out, '\0', sizeof mp->link.proto_out);

    mp->out.seq = 0;
    mp->out.link = 0;
    mp->seq.min_in = 0;
    mp->seq.next_in = 0;

    /*
     * Now we create our server socket.
     * If it already exists, join it.  Otherwise, create and own it
     */
    switch (mpserver_Open(&mp->server, &mp->peer)) {
    case MPSERVER_CONNECTED:
      log_Printf(LogPHASE, "mp: Transfer link on %s\n",
                mp->server.socket.sun_path);
      mp->server.send.dl = dl;		/* Defer 'till it's safe to send */
      return MP_LINKSENT;
    case MPSERVER_FAILED:
      return MP_FAILED;
    case MPSERVER_LISTENING:
      log_Printf(LogPHASE, "mp: Listening on %s\n", mp->server.socket.sun_path);
      log_Printf(LogPHASE, "    First link: %s\n", dl->name);

      /* Re-point our IPCP layer at our MP link */
      ipcp_SetLink(&mp->bundle->ncp.ipcp, &mp->link);

      /* Our lcp's already up 'cos of the NULL parent */
      ccp_SetOpenMode(&mp->link.ccp);
      fsm_Up(&mp->link.ccp.fsm);
      fsm_Open(&mp->link.ccp.fsm);

      mp->active = 1;
      break;
    }
  }

  return MP_UP;
}

void
mp_Down(struct mp *mp)
{
  if (mp->active) {
    struct mbuf *next;

    /* Don't want any more of these */
    mpserver_Close(&mp->server);

    /* CCP goes down with a bang */
    fsm_Down(&mp->link.ccp.fsm);
    fsm_Close(&mp->link.ccp.fsm);

    /* Received fragments go in the bit-bucket */
    while (mp->inbufs) {
      next = mp->inbufs->pnext;
      mbuf_Free(mp->inbufs);
      mp->inbufs = next;
    }

    peerid_Init(&mp->peer);
    mp->active = 0;
  }
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
    mbuf_Free(m);
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
    if (m && isbefore(mp->local_is12bit, mh.seq, h.seq)) {
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
          log_Printf(LogDEBUG, "Drop frag\n");
          next = mp->inbufs->pnext;
          mbuf_Free(mp->inbufs);
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
            /* We might be able to process this ! */
            h.seq--;  /* We're gonna look for fragment with h.seq+1 */
            break;
          }
          next = mp->inbufs->pnext;
          log_Printf(LogDEBUG, "Drop frag %u\n", h.seq);
          mbuf_Free(mp->inbufs);
          mp->inbufs = next;
        } while (mp->inbufs && (h.seq >= mp->seq.min_in || h.end));

        /*
         * Continue processing things from here.
         * This deals with the possibility that we received a fragment
         * on the slowest link that invalidates some of our data (because
         * of the hole at `q'), but where there are subsequent `whole'
         * packets that have already been received.
         */

        mp->seq.next_in = seq = inc_seq(mp->local_is12bit, h.seq);
        last = NULL;
        q = mp->inbufs;
      } else
        /* we may still receive the missing fragment */
        break;
    } else if (h.end) {
      /* We've got something, reassemble */
      struct mbuf **frag = &q;
      int len;
      u_long first = -1;

      do {
        *frag = mp->inbufs;
        mp->inbufs = mp->inbufs->pnext;
        len = mp_ReadHeader(mp, *frag, &h);
        if (first == -1)
          first = h.seq;
        (*frag)->offset += len;
        (*frag)->cnt -= len;
        (*frag)->pnext = NULL;
        if (frag == &q && !h.begin) {
          log_Printf(LogWARN, "Oops - MP frag %lu should have a begin flag\n",
                    (u_long)h.seq);
          mbuf_Free(q);
          q = NULL;
        } else if (frag != &q && h.begin) {
          log_Printf(LogWARN, "Oops - MP frag %lu should have an end flag\n",
                    (u_long)h.seq - 1);
          /*
           * Stuff our fragment back at the front of the queue and zap
           * our half-assembed packet.
           */
          (*frag)->pnext = mp->inbufs;
          mp->inbufs = *frag;
          *frag = NULL;
          mbuf_Free(q);
          q = NULL;
          frag = &q;
          h.end = 0;	/* just in case it's a whole packet */
        } else
          do
            frag = &(*frag)->next;
          while (*frag != NULL);
      } while (!h.end);

      if (q) {
        u_short proto;
        u_char ch;

        q = mbuf_Read(q, &ch, 1);
        proto = ch;
        if (!(proto & 1)) {
          q = mbuf_Read(q, &ch, 1);
          proto <<= 8;
          proto += ch;
        }
        if (log_IsKept(LogDEBUG))
          log_Printf(LogDEBUG, "MP: Reassembled frags %ld-%lu, length %d\n",
                    first, (u_long)h.seq, mbuf_Length(q));
        hdlc_DecodePacket(mp->bundle, proto, q, &mp->link);
      }

      mp->seq.next_in = seq = inc_seq(mp->local_is12bit, h.seq);
      last = NULL;
      q = mp->inbufs;
    } else {
      /* Look for the next fragment */
      seq = inc_seq(mp->local_is12bit, seq);
      last = q;
      q = q->pnext;
    }
  }

  if (m) {
    /* We still have to find a home for our new fragment */
    last = NULL;
    for (q = mp->inbufs; q; last = q, q = q->pnext) {
      mp_ReadHeader(mp, q, &h);
      if (isbefore(mp->local_is12bit, mh.seq, h.seq))
        break;
    }
    /* Our received fragment fits in here */
    if (last)
      last->pnext = m;
    else
      mp->inbufs = m;
    m->pnext = q;
  }
}

static void
mp_Output(struct mp *mp, struct link *l, struct mbuf *m, u_int32_t begin,
          u_int32_t end)
{
  struct mbuf *mo;

  /* Stuff an MP header on the front of our packet and send it */
  mo = mbuf_Alloc(4, MB_MP);
  mo->next = m;
  if (mp->peer_is12bit) {
    u_int16_t *seq16;

    seq16 = (u_int16_t *)MBUF_CTOP(mo);
    *seq16 = htons((begin << 15) | (end << 14) | (u_int16_t)mp->out.seq);
    mo->cnt = 2;
  } else {
    u_int32_t *seq32;

    seq32 = (u_int32_t *)MBUF_CTOP(mo);
    *seq32 = htonl((begin << 31) | (end << 30) | (u_int32_t)mp->out.seq);
    mo->cnt = 4;
  }
  if (log_IsKept(LogDEBUG))
    log_Printf(LogDEBUG, "MP[frag %d]: Send %d bytes on link `%s'\n",
              mp->out.seq, mbuf_Length(mo), l->name);
  mp->out.seq = inc_seq(mp->peer_is12bit, mp->out.seq);

  if (!ccp_Compress(&l->ccp, l, PRI_NORMAL, PROTO_MP, mo))
    hdlc_Output(l, PRI_NORMAL, PROTO_MP, mo);
}

int
mp_FillQueues(struct bundle *bundle)
{
  struct mp *mp = &bundle->ncp.mp;
  struct datalink *dl, *fdl;
  int total, add, len, thislink, nlinks;
  u_int32_t begin, end;
  struct mbuf *m, *mo;

  thislink = nlinks = 0;
  for (fdl = NULL, dl = bundle->links; dl; dl = dl->next) {
    if (!fdl) {
      if (thislink == mp->out.link)
        fdl = dl;
      else
        thislink++;
    }
    nlinks++;
  }

  if (!fdl) {
    fdl = bundle->links;
    if (!fdl)
      return 0;
    thislink = 0;
  }

  total = 0;
  for (dl = fdl; nlinks > 0; dl = dl->next, nlinks--, thislink++) {
    if (!dl) {
      dl = bundle->links;
      thislink = 0;
    }

    if (dl->state != DATALINK_OPEN)
      continue;

    if (dl->physical->out)
      /* this link has suffered a short write.  Let it continue */
      continue;

    add = link_QueueLen(&dl->physical->link);
    total += add;
    if (add)
      /* this link has got stuff already queued.  Let it continue */
      continue;

    if (!link_QueueLen(&mp->link) && !ip_FlushPacket(&mp->link, bundle))
      /* Nothing else to send */
      break;

    m = link_Dequeue(&mp->link);
    len = mbuf_Length(m);
    begin = 1;
    end = 0;

    while (!end) {
      if (dl->state == DATALINK_OPEN) {
        if (len <= dl->mp.weight + LINK_MINWEIGHT) {
          /*
           * XXX: Should we remember how much of our `weight' wasn't sent
           *      so that we can compensate next time ?
           */
          mo = m;
          end = 1;
        } else {
          mo = mbuf_Alloc(dl->mp.weight, MB_MP);
          mo->cnt = dl->mp.weight;
          len -= mo->cnt;
          m = mbuf_Read(m, MBUF_CTOP(mo), mo->cnt);
        }
        mp_Output(mp, &dl->physical->link, mo, begin, end);
        begin = 0;
      }

      if (!end) {
        nlinks--;
        dl = dl->next;
        if (!dl) {
          dl = bundle->links;
          thislink = 0;
        } else
          thislink++;
      }
    }
  }
  mp->out.link = thislink;		/* Start here next time */

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
    log_Printf(LogWARN, "Link weights must not be less than %d\n",
              LINK_MINWEIGHT);
    return 1;
  }
  arg->cx->mp.weight = val;
  return 0;
}

int
mp_ShowStatus(struct cmdargs const *arg)
{
  struct mp *mp = &arg->bundle->ncp.mp;

  prompt_Printf(arg->prompt, "Multilink is %sactive\n", mp->active ? "" : "in");
  if (mp->active) {
    struct mbuf *m;
    int bufs = 0;

    prompt_Printf(arg->prompt, "Socket:         %s\n",
                  mp->server.socket.sun_path);
    for (m = mp->inbufs; m; m = m->pnext)
      bufs++;
    prompt_Printf(arg->prompt, "Pending frags:  %d\n", bufs);
  }

  prompt_Printf(arg->prompt, "\nMy Side:\n");
  if (mp->active) {
    prompt_Printf(arg->prompt, " MRRU:          %u\n", mp->local_mrru);
    prompt_Printf(arg->prompt, " Short Seq:     %s\n",
                  mp->local_is12bit ? "on" : "off");
  }
  prompt_Printf(arg->prompt, " Discriminator: %s\n",
                mp_Enddisc(mp->cfg.enddisc.class, mp->cfg.enddisc.address,
                           mp->cfg.enddisc.len));

  prompt_Printf(arg->prompt, "\nHis Side:\n");
  if (mp->active) {
    prompt_Printf(arg->prompt, " Auth Name:     %s\n", mp->peer.authname);
    prompt_Printf(arg->prompt, " Next SEQ:      %u\n", mp->out.seq);
    prompt_Printf(arg->prompt, " MRRU:          %u\n", mp->peer_mrru);
    prompt_Printf(arg->prompt, " Short Seq:     %s\n",
                  mp->peer_is12bit ? "on" : "off");
  }
  prompt_Printf(arg->prompt,   " Discriminator: %s\n",
                mp_Enddisc(mp->peer.enddisc.class, mp->peer.enddisc.address,
                           mp->peer.enddisc.len));

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  
  prompt_Printf(arg->prompt, " MRRU:          ");
  if (mp->cfg.mrru)
    prompt_Printf(arg->prompt, "%d (multilink enabled)\n", mp->cfg.mrru);
  else
    prompt_Printf(arg->prompt, "disabled\n");
  prompt_Printf(arg->prompt, " Short Seq:     %s\n",
                  command_ShowNegval(mp->cfg.shortseq));

  return 0;
}

const char *
mp_Enddisc(u_char c, const char *address, int len)
{
  static char result[100];
  int f, header;

  switch (c) {
    case ENDDISC_NULL:
      sprintf(result, "Null Class");
      break;

    case ENDDISC_LOCAL:
      snprintf(result, sizeof result, "Local Addr: %.*s", len, address);
      break;

    case ENDDISC_IP:
      if (len == 4)
        snprintf(result, sizeof result, "IP %s",
                 inet_ntoa(*(const struct in_addr *)address));
      else
        sprintf(result, "IP[%d] ???", len);
      break;

    case ENDDISC_MAC:
      if (len == 6) {
        const u_char *m = (const u_char *)address;
        snprintf(result, sizeof result, "MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 m[0], m[1], m[2], m[3], m[4], m[5]);
      } else
        sprintf(result, "MAC[%d] ???", len);
      break;

    case ENDDISC_MAGIC:
      sprintf(result, "Magic: 0x");
      header = strlen(result);
      if (len > sizeof result - header - 1)
        len = sizeof result - header - 1;
      for (f = 0; f < len; f++)
        sprintf(result + header + 2 * f, "%02x", address[f]);
      break;

    case ENDDISC_PSN:
      snprintf(result, sizeof result, "PSN: %.*s", len, address);
      break;

     default:
      sprintf(result, "%d: ", (int)c);
      header = strlen(result);
      if (len > sizeof result - header - 1)
        len = sizeof result - header - 1;
      for (f = 0; f < len; f++)
        sprintf(result + header + 2 * f, "%02x", address[f]);
      break;
  }
  return result;
}

int
mp_SetEnddisc(struct cmdargs const *arg)
{
  struct mp *mp = &arg->bundle->ncp.mp;
  struct in_addr addr;

  if (bundle_Phase(arg->bundle) != PHASE_DEAD) {
    log_Printf(LogWARN, "set enddisc: Only available at phase DEAD\n");
    return 1;
  }

  if (arg->argc == arg->argn) {
    mp->cfg.enddisc.class = 0;
    *mp->cfg.enddisc.address = '\0';
    mp->cfg.enddisc.len = 0;
  } else if (arg->argc > arg->argn) {
    if (!strcasecmp(arg->argv[arg->argn], "label")) {
      mp->cfg.enddisc.class = ENDDISC_LOCAL;
      strcpy(mp->cfg.enddisc.address, arg->bundle->cfg.label);
      mp->cfg.enddisc.len = strlen(mp->cfg.enddisc.address);
    } else if (!strcasecmp(arg->argv[arg->argn], "ip")) {
      if (arg->bundle->ncp.ipcp.my_ip.s_addr == INADDR_ANY)
        addr = arg->bundle->ncp.ipcp.cfg.my_range.ipaddr;
      else
        addr = arg->bundle->ncp.ipcp.my_ip;
      memcpy(mp->cfg.enddisc.address, &addr.s_addr, sizeof addr.s_addr);
      mp->cfg.enddisc.class = ENDDISC_IP;
      mp->cfg.enddisc.len = sizeof arg->bundle->ncp.ipcp.my_ip.s_addr;
    } else if (!strcasecmp(arg->argv[arg->argn], "mac")) {
      struct sockaddr_dl hwaddr;
      int s;

      if (arg->bundle->ncp.ipcp.my_ip.s_addr == INADDR_ANY)
        addr = arg->bundle->ncp.ipcp.cfg.my_range.ipaddr;
      else
        addr = arg->bundle->ncp.ipcp.my_ip;

      s = ID0socket(AF_INET, SOCK_DGRAM, 0);
      if (s < 0) {
        log_Printf(LogERROR, "set enddisc: socket(): %s\n", strerror(errno));
        return 2;
      }
      if (get_ether_addr(s, addr, &hwaddr)) {
        mp->cfg.enddisc.class = ENDDISC_MAC;
        memcpy(mp->cfg.enddisc.address, hwaddr.sdl_data + hwaddr.sdl_nlen,
               hwaddr.sdl_alen);
        mp->cfg.enddisc.len = hwaddr.sdl_alen;
      } else {
        log_Printf(LogWARN, "set enddisc: Can't locate MAC address for %s\n",
                  inet_ntoa(addr));
        close(s);
        return 4;
      }
      close(s);
    } else if (!strcasecmp(arg->argv[arg->argn], "magic")) {
      int f;

      randinit();
      for (f = 0; f < 20; f += sizeof(long))
        *(long *)(mp->cfg.enddisc.address + f) = random();
      mp->cfg.enddisc.class = ENDDISC_MAGIC;
      mp->cfg.enddisc.len = 20;
    } else if (!strcasecmp(arg->argv[arg->argn], "psn")) {
      if (arg->argc > arg->argn+1) {
        mp->cfg.enddisc.class = ENDDISC_PSN;
        strcpy(mp->cfg.enddisc.address, arg->argv[arg->argn+1]);
        mp->cfg.enddisc.len = strlen(mp->cfg.enddisc.address);
      } else {
        log_Printf(LogWARN, "PSN endpoint requires additional data\n");
        return 5;
      }
    } else {
      log_Printf(LogWARN, "%s: Unrecognised endpoint type\n",
                arg->argv[arg->argn]);
      return 6;
    }
  }

  return 0;
}

static int
mpserver_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e,
                   int *n)
{
  struct mpserver *s = descriptor2mpserver(d);

  if (s->send.dl != NULL) {
    /* We've connect()ed */
    if (!link_QueueLen(&s->send.dl->physical->link) &&
        !s->send.dl->physical->out) {
      /* Only send if we've transmitted all our data (i.e. the ConfigAck) */
      datalink_RemoveFromSet(s->send.dl, r, w, e);
      bundle_SendDatalink(s->send.dl, s->fd, &s->socket);
      s->send.dl = NULL;
      close(s->fd);
      s->fd = -1;
    } else
      /* Never read from a datalink that's on death row ! */
      datalink_RemoveFromSet(s->send.dl, r, NULL, NULL);
  } else if (r && s->fd >= 0) {
    if (*n < s->fd + 1)
      *n = s->fd + 1;
    FD_SET(s->fd, r);
    log_Printf(LogTIMER, "mp: fdset(r) %d\n", s->fd);
    return 1;
  }
  return 0;
}

static int
mpserver_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct mpserver *s = descriptor2mpserver(d);
  return s->fd >= 0 && FD_ISSET(s->fd, fdset);
}

static void
mpserver_Read(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct mpserver *s = descriptor2mpserver(d);
  struct sockaddr in;
  int fd, size;

  size = sizeof in;
  fd = accept(s->fd, &in, &size);
  if (fd < 0) {
    log_Printf(LogERROR, "mpserver_Read: accept(): %s\n", strerror(errno));
    return;
  }

  if (in.sa_family == AF_LOCAL)
    bundle_ReceiveDatalink(bundle, fd, (struct sockaddr_un *)&in);

  close(fd);
}

static void
mpserver_Write(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  /* We never want to write here ! */
  log_Printf(LogERROR, "mpserver_Write: Internal error: Bad call !\n");
}

void
mpserver_Init(struct mpserver *s)
{
  s->desc.type = MPSERVER_DESCRIPTOR;
  s->desc.next = NULL;
  s->desc.UpdateSet = mpserver_UpdateSet;
  s->desc.IsSet = mpserver_IsSet;
  s->desc.Read = mpserver_Read;
  s->desc.Write = mpserver_Write;
  s->send.dl = NULL;
  s->fd = -1;
  memset(&s->socket, '\0', sizeof s->socket);
}

int
mpserver_Open(struct mpserver *s, struct peerid *peer)
{
  int f, l;
  mode_t mask;

  if (s->fd != -1) {
    log_Printf(LogERROR, "Internal error !  mpserver already open\n");
    mpserver_Close(s);
  }

  l = snprintf(s->socket.sun_path, sizeof s->socket.sun_path, "%sppp-%s-%02x-",
               _PATH_VARRUN, peer->authname, peer->enddisc.class);

  for (f = 0; f < peer->enddisc.len && l < sizeof s->socket.sun_path - 2; f++) {
    snprintf(s->socket.sun_path + l, sizeof s->socket.sun_path - l,
             "%02x", *(u_char *)(peer->enddisc.address+f));
    l += 2;
  }

  s->socket.sun_family = AF_LOCAL;
  s->socket.sun_len = sizeof s->socket;
  s->fd = ID0socket(PF_LOCAL, SOCK_STREAM, 0);
  if (s->fd < 0) {
    log_Printf(LogERROR, "mpserver: socket: %s\n", strerror(errno));
    return MPSERVER_FAILED;
  }

  setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, (struct sockaddr *)&s->socket,
             sizeof s->socket);
  mask = umask(0177);
  if (ID0bind_un(s->fd, &s->socket) < 0) {
    if (errno != EADDRINUSE) {
      log_Printf(LogPHASE, "mpserver: can't create bundle socket %s (%s)\n",
                s->socket.sun_path, strerror(errno));
      umask(mask);
      close(s->fd);
      s->fd = -1;
      return MPSERVER_FAILED;
    }
    umask(mask);
    if (ID0connect_un(s->fd, &s->socket) < 0) {
      log_Printf(LogPHASE, "mpserver: can't connect to bundle socket %s (%s)\n",
                s->socket.sun_path, strerror(errno));
      if (errno == ECONNREFUSED)
        log_Printf(LogPHASE, "          Has the previous server died badly ?\n");
      close(s->fd);
      s->fd = -1;
      return MPSERVER_FAILED;
    }

    /* Donate our link to the other guy */
    return MPSERVER_CONNECTED;
  }

  /* Listen for other ppp invocations that want to donate links */
  if (listen(s->fd, 5) != 0) {
    log_Printf(LogERROR, "mpserver: Unable to listen to socket"
              " - BUNDLE overload?\n");
    mpserver_Close(s);
  }

  return MPSERVER_LISTENING;
}

void
mpserver_Close(struct mpserver *s)
{
  if (s->send.dl != NULL) {
    bundle_SendDatalink(s->send.dl, s->fd, &s->socket);
    s->send.dl = NULL;
    close(s->fd);
    s->fd = -1;
  } else if (s->fd >= 0) {
    close(s->fd);
    if (ID0unlink(s->socket.sun_path) == -1)
      log_Printf(LogERROR, "%s: Failed to remove: %s\n", s->socket.sun_path,
                strerror(errno));
    memset(&s->socket, '\0', sizeof s->socket);
    s->fd = -1;
  }
}

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
 *	$Id: mp.h,v 1.1.2.1 1998/04/03 19:21:46 brian Exp $
 */

struct mbuf;
struct physical;
struct bundle;
struct cmdargs;

struct mp {
  struct link link;

  unsigned active : 1;
  unsigned is12bit : 1;        /* 12 / 24bit seq nos */

  struct {
    u_int32_t out;            /* next outgoing seq */
    u_int32_t min_in;         /* minimum received incoming seq */
    u_int32_t next_in;        /* next incoming seq to process */
  } seq;
  struct mbuf *inbufs;        /* Received fragments */

  struct fsm_parent fsmp;     /* Our callback functions */

  struct bundle *bundle;
};

struct mp_link {
  u_int32_t seq;              /* 12 or 24 bit incoming seq */
  int weight;                 /* bytes to send with each write */
};

struct mp_header {
  unsigned begin : 1;
  unsigned end : 1;
  u_int32_t seq;
};

extern void mp_Init(struct mp *, struct bundle *);
extern void mp_linkInit(struct mp_link *);
extern void mp_Input(struct mp *, struct mbuf *, struct physical *);
extern int mp_FillQueues(struct bundle *);
extern int mp_SetDatalinkWeight(struct cmdargs const *);

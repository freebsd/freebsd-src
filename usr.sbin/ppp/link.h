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
 *  $Id: link.h,v 1.2 1998/05/21 21:46:14 brian Exp $
 *
 */


#define PHYSICAL_LINK 1
#define MP_LINK       2

#define LINK_QUEUES (PRI_MAX + 1)
#define NPROTOSTAT 13

struct bundle;
struct prompt;

struct link {
  int type;                               /* _LINK type */
  const char *name;                       /* Points to datalink::name */
  int len;                                /* full size of parent struct */
  struct pppThroughput throughput;        /* Link throughput statistics */
  struct mqueue Queue[LINK_QUEUES];       /* Our output queue of mbufs */

  u_long proto_in[NPROTOSTAT];            /* outgoing protocol stats */
  u_long proto_out[NPROTOSTAT];           /* incoming protocol stats */

  struct lcp lcp;                         /* Our line control FSM */
  struct ccp ccp;                         /* Our compression FSM */
};

extern void link_AddInOctets(struct link *, int);
extern void link_AddOutOctets(struct link *, int);

extern void link_SequenceQueue(struct link *);
extern int link_QueueLen(struct link *);
extern int link_QueueBytes(struct link *);
extern struct mbuf *link_Dequeue(struct link *);
extern void link_Write(struct link *, int, const char *, int);
extern void link_StartOutput(struct link *, struct bundle *);
extern void link_Output(struct link *, int, struct mbuf *);

#define PROTO_IN  1                       /* third arg to link_ProtocolRecord */
#define PROTO_OUT 2
extern void link_ProtocolRecord(struct link *, u_short, int);
extern void link_ReportProtocolStatus(struct link *, struct prompt *);

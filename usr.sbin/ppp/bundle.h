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
 *	$Id: bundle.h,v 1.1.2.5 1998/02/06 02:24:02 brian Exp $
 */

#define	PHASE_DEAD		0	/* Link is dead */
#define	PHASE_ESTABLISH		1	/* Establishing link */
#define	PHASE_AUTHENTICATE	2	/* Being authenticated */
#define	PHASE_NETWORK		3	/* We're alive ! */
#define	PHASE_TERMINATE		4	/* Terminating link */

struct physical;
struct link;
struct fsm;

struct bundle {
  int unit;                   /* The tun number */
  int ifIndex;                /* The interface number */
  int tun_fd;                 /* The /dev/tunX descriptor */
  char dev[20];               /* The /dev/tunX name */
  char *ifname;               /* The interface name */
  int routing_seq;            /* The current routing sequence number */
  u_int phase;                /* Curent phase */

  struct physical *physical;  /* For the time being */
};

extern struct bundle *bundle_Create(const char *dev);
extern const char *bundle_PhaseName(struct bundle *);
#define bundle_Phase(b) ((b)->phase)
extern void bundle_NewPhase(struct bundle *, struct physical *, u_int);
extern void bundle_LayerStart(struct bundle *, struct fsm *);
extern void bundle_LayerUp(struct bundle *, struct fsm *);
extern int  bundle_LinkIsUp(const struct bundle *);
extern void bundle_SetRoute(struct bundle *, int, struct in_addr,
                            struct in_addr, struct in_addr, int);
extern void bundle_LinkLost(struct bundle *, struct link *);
extern void bundle_Close(struct bundle *, struct fsm *);
extern void bundle_LayerDown(struct bundle *, struct fsm *);
extern void bundle_LayerFinish(struct bundle *, struct fsm *);

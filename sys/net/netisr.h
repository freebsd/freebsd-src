/*-
 * Copyright (c) 1980, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)netisr.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

#ifndef _NET_NETISR_H_
#define _NET_NETISR_H_

/*
 * The networking code runs off software interrupts.
 *
 * You can switch into the network by doing splnet() and return by splx().
 * The software interrupt level for the network is higher than the software
 * level for the clock (so you can enter the network in routines called
 * at timeout time).
 */

/*
 * Each ``pup-level-1'' input queue has a bit in a ``netisr'' status
 * word which is used to de-multiplex a single software
 * interrupt used for scheduling the network code to calls
 * on the lowest level routine of each protocol.
 */
#define	NETISR_POLL	0		/* polling callback, must be first */
#define	NETISR_IP	2		/* same as AF_INET */
#define	NETISR_ROUTE	14		/* routing socket */
#define	NETISR_AARP	15		/* Appletalk ARP */
#define	NETISR_ATALK2	16		/* Appletalk phase 2 */
#define	NETISR_ATALK1	17		/* Appletalk phase 1 */
#define	NETISR_ARP	18		/* same as AF_LINK */
#define	NETISR_IPX	23		/* same as AF_IPX */
#define	NETISR_USB	25		/* USB soft interrupt */
#define	NETISR_PPP	26		/* PPP soft interrupt */
#define	NETISR_IPV6	27
#define	NETISR_NATM	28
#define	NETISR_ATM	29
#define	NETISR_NETGRAPH	30
#define	NETISR_POLLMORE	31		/* polling callback, must be last */


#ifndef LOCORE
#ifdef _KERNEL

void legacy_setsoftnet(void);

extern volatile unsigned int	netisr;	/* scheduling bits for network */
#define	schednetisr(anisr) do {						\
	atomic_set_rel_int(&netisr, 1 << (anisr));			\
	legacy_setsoftnet();						\
} while (0)
/* used to atomically schedule multiple netisrs */
#define	schednetisrbits(isrbits) do {					\
	atomic_set_rel_int(&netisr, isrbits);				\
	legacy_setsoftnet();						\
} while (0)

struct ifqueue;
struct mbuf;

typedef void netisr_t (struct mbuf *);
  
void	netisr_dispatch(int, struct mbuf *);
int	netisr_queue(int, struct mbuf *);
#define	NETISR_MPSAFE	0x0001		/* ISR does not need Giant */
void	netisr_register(int, netisr_t *, struct ifqueue *, int);
void	netisr_unregister(int);

#endif
#endif

#endif

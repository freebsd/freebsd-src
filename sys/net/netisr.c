/*-
 * Copyright (c) 2001,2002,2003 Jonathan Lemon <jlemon@FreeBSD.org>
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
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
 * $FreeBSD$
 */

#include "opt_device_polling.h"
#include "opt_net.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rtprio.h>
#include <sys/systm.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/stdarg.h>

#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/netisr.h>

volatile unsigned int	netisr;	/* scheduling bits for network */

struct netisr {
	netisr_t	*ni_handler;
	struct ifqueue	*ni_queue;
	int		ni_flags;
} netisrs[32];

static void *net_ih;

void
legacy_setsoftnet(void)
{
	swi_sched(net_ih, 0);
}

void
netisr_register(int num, netisr_t *handler, struct ifqueue *inq, int flags)
{
	
	KASSERT(!(num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs))),
	    ("bad isr %d", num));
	netisrs[num].ni_handler = handler;
	netisrs[num].ni_queue = inq;
	netisrs[num].ni_flags = flags;
}

void
netisr_unregister(int num)
{
	struct netisr *ni;
	
	KASSERT(!(num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs))),
	    ("bad isr %d", num));
	ni = &netisrs[num];
	ni->ni_handler = NULL;
	if (ni->ni_queue != NULL)
		IF_DRAIN(ni->ni_queue);
	ni->ni_queue = NULL;
}

struct isrstat {
	int	isrs_count;			/* dispatch count */
	int	isrs_directed;			/* ...directly dispatched */
	int	isrs_deferred;			/* ...queued instead */
	int	isrs_queued;			/* intentionally queueued */
	int	isrs_drop;			/* dropped 'cuz no handler */
	int	isrs_swi_count;			/* swi_net handlers called */
};
static struct isrstat isrstat;

SYSCTL_NODE(_net, OID_AUTO, isr, CTLFLAG_RW, 0, "netisr counters");

static int	netisr_direct = 1;
SYSCTL_INT(_net_isr, OID_AUTO, direct, CTLFLAG_RW, 
    &netisr_direct, 0, "enable direct dispatch");
TUNABLE_INT("net.isr.direct", &netisr_direct);

SYSCTL_INT(_net_isr, OID_AUTO, count, CTLFLAG_RD,
    &isrstat.isrs_count, 0, "");
SYSCTL_INT(_net_isr, OID_AUTO, directed, CTLFLAG_RD, 
    &isrstat.isrs_directed, 0, "");
SYSCTL_INT(_net_isr, OID_AUTO, deferred, CTLFLAG_RD, 
    &isrstat.isrs_deferred, 0, "");
SYSCTL_INT(_net_isr, OID_AUTO, queued, CTLFLAG_RD, 
    &isrstat.isrs_queued, 0, "");
SYSCTL_INT(_net_isr, OID_AUTO, drop, CTLFLAG_RD, 
    &isrstat.isrs_drop, 0, "");
SYSCTL_INT(_net_isr, OID_AUTO, swi_count, CTLFLAG_RD, 
    &isrstat.isrs_swi_count, 0, "");

/*
 * Process all packets currently present in a netisr queue.  Used to
 * drain an existing set of packets waiting for processing when we
 * begin direct dispatch, to avoid processing packets out of order.
 */
static void
netisr_processqueue(struct netisr *ni)
{
	struct mbuf *m;

	for (;;) {
		IF_DEQUEUE(ni->ni_queue, m);
		if (m == NULL)
			break;
		ni->ni_handler(m);
	}
}

/*
 * Call the netisr directly instead of queueing the packet, if possible.
 */
void
netisr_dispatch(int num, struct mbuf *m)
{
	struct netisr *ni;
	
	isrstat.isrs_count++;		/* XXX redundant */
	KASSERT(!(num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs))),
	    ("bad isr %d", num));
	ni = &netisrs[num];
	if (ni->ni_queue == NULL) {
		isrstat.isrs_drop++;
		m_freem(m);
		return;
	}
	/*
	 * Do direct dispatch only for MPSAFE netisrs (and
	 * only when enabled).  Note that when a netisr is
	 * marked MPSAFE we permit multiple concurrent instances
	 * to run.  We guarantee only the order in which
	 * packets are processed for each "dispatch point" in
	 * the system (i.e. call to netisr_dispatch or
	 * netisr_queue).  This insures ordering of packets
	 * from an interface but does not guarantee ordering
	 * between multiple places in the system (e.g. IP
	 * dispatched from interfaces vs. IP queued from IPSec).
	 */
	if (netisr_direct && (ni->ni_flags & NETISR_MPSAFE)) {
		isrstat.isrs_directed++;
		/*
		 * NB: We used to drain the queue before handling
		 * the packet but now do not.  Doing so here will
		 * not preserve ordering so instead we fallback to
		 * guaranteeing order only from dispatch points
		 * in the system (see above).
		 */
		ni->ni_handler(m);
	} else {
		isrstat.isrs_deferred++;
		if (IF_HANDOFF(ni->ni_queue, m, NULL))
			schednetisr(num);
	}
}

/*
 * Same as above, but always queue.
 * This is either used in places where we are not confident that
 * direct dispatch is possible, or where queueing is required.
 * It returns (0) on success and ERRNO on failure.  On failure the
 * mbuf has been free'd.
 */
int
netisr_queue(int num, struct mbuf *m)
{
	struct netisr *ni;
	
	KASSERT(!(num < 0 || num >= (sizeof(netisrs)/sizeof(*netisrs))),
	    ("bad isr %d", num));
	ni = &netisrs[num];
	if (ni->ni_queue == NULL) {
		isrstat.isrs_drop++;
		m_freem(m);
		return (ENXIO);
	}
	isrstat.isrs_queued++;
	if (!IF_HANDOFF(ni->ni_queue, m, NULL))
		return (ENOBUFS);	/* IF_HANDOFF has free'd the mbuf */
	schednetisr(num);
	return (0);
}

static void
swi_net(void *dummy)
{
	struct netisr *ni;
	u_int bits;
	int i;
#ifdef DEVICE_POLLING
	const int polling = 1;
#else
	const int polling = 0;
#endif

	do {
		bits = atomic_readandclear_int(&netisr);
		if (bits == 0)
			break;
		while ((i = ffs(bits)) != 0) {
			isrstat.isrs_swi_count++;
			i--;
			bits &= ~(1 << i);
			ni = &netisrs[i];
			if (ni->ni_handler == NULL) {
				printf("swi_net: unregistered isr %d.\n", i);
				continue;
			}
			if ((ni->ni_flags & NETISR_MPSAFE) == 0) {
				mtx_lock(&Giant);
				if (ni->ni_queue == NULL)
					ni->ni_handler(NULL);
				else
					netisr_processqueue(ni);
				mtx_unlock(&Giant);
			} else {
				if (ni->ni_queue == NULL)
					ni->ni_handler(NULL);
				else
					netisr_processqueue(ni);
			}
		}
	} while (polling);
}

static void
start_netisr(void *dummy)
{

	if (swi_add(NULL, "net", swi_net, NULL, SWI_NET, INTR_MPSAFE, &net_ih))
		panic("start_netisr");
}
SYSINIT(start_netisr, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_netisr, NULL)

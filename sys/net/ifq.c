/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/socket.h>

#ifndef ALTQ
#define	ALTQ	/* Needed for ifq.h prototypes only. */
#endif

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/ifq.h>

int
drbr_enqueue(struct ifnet *ifp, struct buf_ring *br, struct mbuf *m)
{
	int error = 0;

	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_ENQUEUE(&ifp->if_snd, m, error);
		if (error)
			if_inc_counter((ifp), IFCOUNTER_OQDROPS, 1);
		return (error);
	}
	error = buf_ring_enqueue(br, m);
	if (error)
		m_freem(m);

	return (error);
}

void
drbr_putback(struct ifnet *ifp, struct buf_ring *br, struct mbuf *m_new)
{
	/*
	 * The top of the list needs to be swapped
	 * for this one.
	 */
	if (ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd)) {
		/*
		 * Peek in altq case dequeued it
		 * so put it back.
		 */
		IFQ_DRV_PREPEND(&ifp->if_snd, m_new);
		return;
	}
	buf_ring_putback_sc(br, m_new);
}

struct mbuf *
drbr_peek(struct ifnet *ifp, struct buf_ring *br)
{
	struct mbuf *m;
	if (ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd)) {
		/*
		 * Pull it off like a dequeue
		 * since drbr_advance() does nothing
		 * for altq and drbr_putback() will
		 * use the old prepend function.
		 */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		return (m);
	}
	return ((struct mbuf *)buf_ring_peek_clear_sc(br));
}

void
drbr_flush(struct ifnet *ifp, struct buf_ring *br)
{
	struct mbuf *m;

	if (ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd))
		IFQ_PURGE(&ifp->if_snd);
	while ((m = (struct mbuf *)buf_ring_dequeue_sc(br)) != NULL)
		m_freem(m);
}

struct mbuf *
drbr_dequeue(struct ifnet *ifp, struct buf_ring *br)
{
	struct mbuf *m;

	if (ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		return (m);
	}
	return ((struct mbuf *)buf_ring_dequeue_sc(br));
}

void
drbr_advance(struct ifnet *ifp, struct buf_ring *br)
{
	/* Nothing to do here since peek dequeues in altq case */
	if (ifp != NULL && ALTQ_IS_ENABLED(&ifp->if_snd))
		return;
	return (buf_ring_advance_sc(br));
}

struct mbuf *
drbr_dequeue_cond(struct ifnet *ifp, struct buf_ring *br,
    int (*func) (struct mbuf *, void *), void *arg)
{
	struct mbuf *m;
	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_LOCK(&ifp->if_snd);
		IFQ_POLL_NOLOCK(&ifp->if_snd, m);
		if (m != NULL && func(m, arg) == 0) {
			IFQ_UNLOCK(&ifp->if_snd);
			return (NULL);
		}
		IFQ_DEQUEUE_NOLOCK(&ifp->if_snd, m);
		IFQ_UNLOCK(&ifp->if_snd);
		return (m);
	}
	m = (struct mbuf *)buf_ring_peek(br);
	if (m == NULL || func(m, arg) == 0)
		return (NULL);

	return ((struct mbuf *)buf_ring_dequeue_sc(br));
}

int
drbr_empty(struct ifnet *ifp, struct buf_ring *br)
{
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		return (IFQ_IS_EMPTY(&ifp->if_snd));
	return (buf_ring_empty(br));
}

int
drbr_needs_enqueue(struct ifnet *ifp, struct buf_ring *br)
{
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		return (1);
	return (!buf_ring_empty(br));
}

int
drbr_inuse(struct ifnet *ifp, struct buf_ring *br)
{
	if (ALTQ_IS_ENABLED(&ifp->if_snd))
		return (ifp->if_snd.ifq_len);
	return (buf_ring_count(br));
}


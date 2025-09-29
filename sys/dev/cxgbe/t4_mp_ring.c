/*-
 * Copyright (c) 2014 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

#include "t4_mp_ring.h"

#if defined(__i386__)
#define atomic_cmpset_acq_64 atomic_cmpset_64
#define atomic_cmpset_rel_64 atomic_cmpset_64
#endif

/*
 * mp_ring handles multiple threads (producers) enqueueing data to a tx queue.
 * The thread that is writing the hardware descriptors is the consumer and it
 * runs with the consumer lock held.  A producer becomes the consumer if there
 * isn't one already.  The consumer runs with the flags sets to BUSY and
 * consumes everything (IDLE or COALESCING) or gets STALLED.  If it is running
 * over its budget it sets flags to TOO_BUSY.  A producer that observes a
 * TOO_BUSY consumer will become the new consumer by setting flags to
 * TAKING_OVER.  The original consumer stops and sets the flags back to BUSY for
 * the new consumer.
 *
 * COALESCING is the same as IDLE except there are items being held in the hope
 * that they can be coalesced with items that follow.  The driver must arrange
 * for a tx update or some other event that transmits all the held items in a
 * timely manner if nothing else is enqueued.
 */

union ring_state {
	struct {
		uint16_t pidx_head;
		uint16_t pidx_tail;
		uint16_t cidx;
		uint16_t flags;
	};
	uint64_t state;
};

enum {
	IDLE = 0,	/* tx is all caught up, nothing to do. */
	COALESCING,	/* IDLE, but tx frames are being held for coalescing */
	BUSY,		/* consumer is running already, or will be shortly. */
	TOO_BUSY,	/* consumer is running and is beyond its budget */
	TAKING_OVER,	/* new consumer taking over from a TOO_BUSY consumer */
	STALLED,	/* consumer stopped due to lack of resources. */
};

enum {
	C_FAST = 0,
	C_2,
	C_3,
	C_TAKEOVER,
};

static inline uint16_t
space_available(struct mp_ring *r, union ring_state s)
{
	uint16_t x = r->size - 1;

	if (s.cidx == s.pidx_head)
		return (x);
	else if (s.cidx > s.pidx_head)
		return (s.cidx - s.pidx_head - 1);
	else
		return (x - s.pidx_head + s.cidx);
}

static inline uint16_t
increment_idx(struct mp_ring *r, uint16_t idx, uint16_t n)
{
	int x = r->size - idx;

	MPASS(x > 0);
	return (x > n ? idx + n : n - x);
}

/*
 * Consumer.  Called with the consumer lock held and a guarantee that there is
 * work to do.
 */
static void
drain_ring(struct mp_ring *r, int budget)
{
	union ring_state os, ns;
	int n, pending, total;
	uint16_t cidx;
	uint16_t pidx;
	bool coalescing;

	mtx_assert(r->cons_lock, MA_OWNED);

	os.state = atomic_load_acq_64(&r->state);
	MPASS(os.flags == BUSY);

	cidx = os.cidx;
	pidx = os.pidx_tail;
	MPASS(cidx != pidx);

	pending = 0;
	total = 0;

	while (cidx != pidx) {

		/* Items from cidx to pidx are available for consumption. */
		n = r->drain(r, cidx, pidx, &coalescing);
		if (n == 0) {
			critical_enter();
			os.state = atomic_load_64(&r->state);
			do {
				ns.state = os.state;
				ns.cidx = cidx;

				MPASS(os.flags == BUSY ||
				    os.flags == TOO_BUSY ||
				    os.flags == TAKING_OVER);

				if (os.flags == TAKING_OVER)
					ns.flags = BUSY;
				else
					ns.flags = STALLED;
			} while (atomic_fcmpset_64(&r->state, &os.state,
			    ns.state) == 0);
			critical_exit();
			if (os.flags == TAKING_OVER)
				counter_u64_add(r->abdications, 1);
			else if (ns.flags == STALLED)
				counter_u64_add(r->stalls, 1);
			break;
		}
		cidx = increment_idx(r, cidx, n);
		pending += n;
		total += n;
		counter_u64_add(r->consumed, n);

		os.state = atomic_load_64(&r->state);
		do {
			MPASS(os.flags == BUSY || os.flags == TOO_BUSY ||
			    os.flags == TAKING_OVER);

			ns.state = os.state;
			ns.cidx = cidx;
			if (__predict_false(os.flags == TAKING_OVER)) {
				MPASS(total >= budget);
				ns.flags = BUSY;
				continue;
			}
			if (cidx == os.pidx_tail) {
				ns.flags = coalescing ? COALESCING : IDLE;
				continue;
			}
			if (total >= budget) {
				ns.flags = TOO_BUSY;
				continue;
			}
			MPASS(os.flags == BUSY);
			if (pending < 32)
				break;
		} while (atomic_fcmpset_acq_64(&r->state, &os.state, ns.state) == 0);

		if (__predict_false(os.flags == TAKING_OVER)) {
			MPASS(ns.flags == BUSY);
			counter_u64_add(r->abdications, 1);
			break;
		}

		if (ns.flags == IDLE || ns.flags == COALESCING) {
			MPASS(ns.pidx_tail == cidx);
			if (ns.pidx_head != ns.pidx_tail)
				counter_u64_add(r->cons_idle2, 1);
			else
				counter_u64_add(r->cons_idle, 1);
			break;
		}

		/*
		 * The acquire style atomic above guarantees visibility of items
		 * associated with any pidx change that we notice here.
		 */
		pidx = ns.pidx_tail;
		pending = 0;
	}

#ifdef INVARIANTS
	if (os.flags == TAKING_OVER)
		MPASS(ns.flags == BUSY);
	else {
		MPASS(ns.flags == IDLE || ns.flags == COALESCING ||
		    ns.flags == STALLED);
	}
#endif
}

static void
drain_txpkts(struct mp_ring *r, union ring_state os, int budget)
{
	union ring_state ns;
	uint16_t cidx = os.cidx;
	uint16_t pidx = os.pidx_tail;
	bool coalescing;

	mtx_assert(r->cons_lock, MA_OWNED);
	MPASS(os.flags == BUSY);
	MPASS(cidx == pidx);

	r->drain(r, cidx, pidx, &coalescing);
	MPASS(coalescing == false);
	critical_enter();
	os.state = atomic_load_64(&r->state);
	do {
		ns.state = os.state;
		MPASS(os.flags == BUSY);
		MPASS(os.cidx == cidx);
		if (ns.cidx == ns.pidx_tail)
			ns.flags = IDLE;
		else
			ns.flags = BUSY;
	} while (atomic_fcmpset_acq_64(&r->state, &os.state, ns.state) == 0);
	critical_exit();

	if (ns.flags == BUSY)
		drain_ring(r, budget);
}

int
mp_ring_alloc(struct mp_ring **pr, int size, void *cookie, ring_drain_t drain,
    ring_can_drain_t can_drain, struct malloc_type *mt, struct mtx *lck,
    int flags)
{
	struct mp_ring *r;
	int i;

	/* All idx are 16b so size can be 65536 at most */
	if (pr == NULL || size < 2 || size > 65536 || drain == NULL ||
	    can_drain == NULL)
		return (EINVAL);
	*pr = NULL;
	flags &= M_NOWAIT | M_WAITOK;
	MPASS(flags != 0);

	r = malloc(__offsetof(struct mp_ring, items[size]), mt, flags | M_ZERO);
	if (r == NULL)
		return (ENOMEM);
	r->size = size;
	r->cookie = cookie;
	r->mt = mt;
	r->drain = drain;
	r->can_drain = can_drain;
	r->cons_lock = lck;
	if ((r->dropped = counter_u64_alloc(flags)) == NULL)
		goto failed;
	for (i = 0; i < nitems(r->consumer); i++) {
		if ((r->consumer[i] = counter_u64_alloc(flags)) == NULL)
			goto failed;
	}
	if ((r->not_consumer = counter_u64_alloc(flags)) == NULL)
		goto failed;
	if ((r->abdications = counter_u64_alloc(flags)) == NULL)
		goto failed;
	if ((r->stalls = counter_u64_alloc(flags)) == NULL)
		goto failed;
	if ((r->consumed = counter_u64_alloc(flags)) == NULL)
		goto failed;
	if ((r->cons_idle = counter_u64_alloc(flags)) == NULL)
		goto failed;
	if ((r->cons_idle2 = counter_u64_alloc(flags)) == NULL)
		goto failed;
	*pr = r;
	return (0);
failed:
	mp_ring_free(r);
	return (ENOMEM);
}

void
mp_ring_free(struct mp_ring *r)
{
	int i;

	if (r == NULL)
		return;

	if (r->dropped != NULL)
		counter_u64_free(r->dropped);
	for (i = 0; i < nitems(r->consumer); i++) {
		if (r->consumer[i] != NULL)
			counter_u64_free(r->consumer[i]);
	}
	if (r->not_consumer != NULL)
		counter_u64_free(r->not_consumer);
	if (r->abdications != NULL)
		counter_u64_free(r->abdications);
	if (r->stalls != NULL)
		counter_u64_free(r->stalls);
	if (r->consumed != NULL)
		counter_u64_free(r->consumed);
	if (r->cons_idle != NULL)
		counter_u64_free(r->cons_idle);
	if (r->cons_idle2 != NULL)
		counter_u64_free(r->cons_idle2);

	free(r, r->mt);
}

/*
 * Enqueue n items and maybe drain the ring for some time.
 *
 * Returns an errno.
 */
int
mp_ring_enqueue(struct mp_ring *r, void **items, int n, int budget)
{
	union ring_state os, ns;
	uint16_t pidx_start, pidx_stop;
	int i, nospc, cons;
	bool consumer;

	MPASS(items != NULL);
	MPASS(n > 0);

	/*
	 * Reserve room for the new items.  Our reservation, if successful, is
	 * from 'pidx_start' to 'pidx_stop'.
	 */
	nospc = 0;
	os.state = atomic_load_64(&r->state);
	for (;;) {
		for (;;) {
			if (__predict_true(space_available(r, os) >= n))
				break;

			/* Not enough room in the ring. */

			MPASS(os.flags != IDLE);
			MPASS(os.flags != COALESCING);
			if (__predict_false(++nospc > 100)) {
				counter_u64_add(r->dropped, n);
				return (ENOBUFS);
			}
			if (os.flags == STALLED)
				mp_ring_check_drainage(r, 64);
			else
				cpu_spinwait();
			os.state = atomic_load_64(&r->state);
		}

		/* There is room in the ring. */

		cons = -1;
		ns.state = os.state;
		ns.pidx_head = increment_idx(r, os.pidx_head, n);
		if (os.flags == IDLE || os.flags == COALESCING) {
			MPASS(os.pidx_tail == os.cidx);
			if (os.pidx_head == os.pidx_tail) {
				cons = C_FAST;
				ns.pidx_tail = increment_idx(r, os.pidx_tail, n);
			} else
				cons = C_2;
			ns.flags = BUSY;
		} else if (os.flags == TOO_BUSY) {
			cons = C_TAKEOVER;
			ns.flags = TAKING_OVER;
		}
		critical_enter();
		if (atomic_fcmpset_64(&r->state, &os.state, ns.state))
			break;
		critical_exit();
		cpu_spinwait();
	};

	pidx_start = os.pidx_head;
	pidx_stop = ns.pidx_head;

	if (cons == C_FAST) {
		i = pidx_start;
		do {
			r->items[i] = *items++;
			if (__predict_false(++i == r->size))
				i = 0;
		} while (i != pidx_stop);
		critical_exit();
		counter_u64_add(r->consumer[C_FAST], 1);
		mtx_lock(r->cons_lock);
		drain_ring(r, budget);
		mtx_unlock(r->cons_lock);
		return (0);
	}

	/*
	 * Wait for other producers who got in ahead of us to enqueue their
	 * items, one producer at a time.  It is our turn when the ring's
	 * pidx_tail reaches the beginning of our reservation (pidx_start).
	 */
	while (ns.pidx_tail != pidx_start) {
		cpu_spinwait();
		ns.state = atomic_load_64(&r->state);
	}

	/* Now it is our turn to fill up the area we reserved earlier. */
	i = pidx_start;
	do {
		r->items[i] = *items++;
		if (__predict_false(++i == r->size))
			i = 0;
	} while (i != pidx_stop);

	/*
	 * Update the ring's pidx_tail.  The release style atomic guarantees
	 * that the items are visible to any thread that sees the updated pidx.
	 */
	os.state = atomic_load_64(&r->state);
	do {
		consumer = false;
		ns.state = os.state;
		ns.pidx_tail = pidx_stop;
		if (os.flags == IDLE || os.flags == COALESCING ||
		    (os.flags == STALLED && r->can_drain(r))) {
			MPASS(cons == -1);
			consumer = true;
			ns.flags = BUSY;
		}
	} while (atomic_fcmpset_rel_64(&r->state, &os.state, ns.state) == 0);
	critical_exit();

	if (cons == -1) {
		if (consumer)
			cons = C_3;
		else {
			counter_u64_add(r->not_consumer, 1);
			return (0);
		}
	}
	MPASS(cons > C_FAST && cons < nitems(r->consumer));
	counter_u64_add(r->consumer[cons], 1);
	mtx_lock(r->cons_lock);
	drain_ring(r, budget);
	mtx_unlock(r->cons_lock);

	return (0);
}

/*
 * Enqueue n items but never drain the ring.  Can be called
 * to enqueue new items while draining the ring.
 *
 * Returns an errno.
 */
int
mp_ring_enqueue_only(struct mp_ring *r, void **items, int n)
{
	union ring_state os, ns;
	uint16_t pidx_start, pidx_stop;
	int i;

	MPASS(items != NULL);
	MPASS(n > 0);

	/*
	 * Reserve room for the new items.  Our reservation, if successful, is
	 * from 'pidx_start' to 'pidx_stop'.
	 */
	os.state = atomic_load_64(&r->state);

	/* Should only be used from the drain callback. */
	MPASS(os.flags == BUSY || os.flags == TOO_BUSY ||
	    os.flags == TAKING_OVER);

	for (;;) {
		if (__predict_false(space_available(r, os) < n)) {
			/* Not enough room in the ring. */
			counter_u64_add(r->dropped, n);
			return (ENOBUFS);
		}

		/* There is room in the ring. */

		ns.state = os.state;
		ns.pidx_head = increment_idx(r, os.pidx_head, n);
		critical_enter();
		if (atomic_fcmpset_64(&r->state, &os.state, ns.state))
			break;
		critical_exit();
		cpu_spinwait();
	};

	pidx_start = os.pidx_head;
	pidx_stop = ns.pidx_head;

	/*
	 * Wait for other producers who got in ahead of us to enqueue their
	 * items, one producer at a time.  It is our turn when the ring's
	 * pidx_tail reaches the beginning of our reservation (pidx_start).
	 */
	while (ns.pidx_tail != pidx_start) {
		cpu_spinwait();
		ns.state = atomic_load_64(&r->state);
	}

	/* Now it is our turn to fill up the area we reserved earlier. */
	i = pidx_start;
	do {
		r->items[i] = *items++;
		if (__predict_false(++i == r->size))
			i = 0;
	} while (i != pidx_stop);

	/*
	 * Update the ring's pidx_tail.  The release style atomic guarantees
	 * that the items are visible to any thread that sees the updated pidx.
	 */
	os.state = atomic_load_64(&r->state);
	do {
		ns.state = os.state;
		ns.pidx_tail = pidx_stop;
	} while (atomic_fcmpset_rel_64(&r->state, &os.state, ns.state) == 0);
	critical_exit();

	counter_u64_add(r->not_consumer, 1);
	return (0);
}

void
mp_ring_check_drainage(struct mp_ring *r, int budget)
{
	union ring_state os, ns;

	os.state = atomic_load_64(&r->state);
	if (os.flags == STALLED && r->can_drain(r)) {
		MPASS(os.cidx != os.pidx_tail);	/* implied by STALLED */
		ns.state = os.state;
		ns.flags = BUSY;
		if (atomic_cmpset_acq_64(&r->state, os.state, ns.state)) {
			mtx_lock(r->cons_lock);
			drain_ring(r, budget);
			mtx_unlock(r->cons_lock);
		}
	} else if (os.flags == COALESCING) {
		MPASS(os.cidx == os.pidx_tail);
		ns.state = os.state;
		ns.flags = BUSY;
		if (atomic_cmpset_acq_64(&r->state, os.state, ns.state)) {
			mtx_lock(r->cons_lock);
			drain_txpkts(r, ns, budget);
			mtx_unlock(r->cons_lock);
		}
	}
}

void
mp_ring_reset_stats(struct mp_ring *r)
{
	int i;

	counter_u64_zero(r->dropped);
	for (i = 0; i < nitems(r->consumer); i++)
		counter_u64_zero(r->consumer[i]);
	counter_u64_zero(r->not_consumer);
	counter_u64_zero(r->abdications);
	counter_u64_zero(r->stalls);
	counter_u64_zero(r->consumed);
	counter_u64_zero(r->cons_idle);
	counter_u64_zero(r->cons_idle2);
}

bool
mp_ring_is_idle(struct mp_ring *r)
{
	union ring_state s;

	s.state = atomic_load_64(&r->state);
	if (s.pidx_head == s.pidx_tail && s.pidx_tail == s.cidx &&
	    s.flags == IDLE)
		return (true);

	return (false);
}

void
mp_ring_sysctls(struct mp_ring *r, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *children)
{
	struct sysctl_oid *oid;

	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "mp_ring", CTLFLAG_RD |
	    CTLFLAG_MPSAFE, NULL, "mp_ring statistics");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_U64(ctx, children, OID_AUTO, "state", CTLFLAG_RD,
	    __DEVOLATILE(uint64_t *, &r->state), 0, "ring state");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "dropped", CTLFLAG_RD,
	    &r->dropped, "# of items dropped");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "consumed",
	    CTLFLAG_RD, &r->consumed, "# of items consumed");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "fast_consumer",
	    CTLFLAG_RD, &r->consumer[C_FAST],
	    "# of times producer became consumer (fast)");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "consumer2",
	    CTLFLAG_RD, &r->consumer[C_2],
	    "# of times producer became consumer (2)");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "consumer3",
	    CTLFLAG_RD, &r->consumer[C_3],
	    "# of times producer became consumer (3)");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "takeovers",
	    CTLFLAG_RD, &r->consumer[C_TAKEOVER],
	    "# of times producer took over from another consumer.");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "not_consumer",
	    CTLFLAG_RD, &r->not_consumer,
	    "# of times producer did not become consumer");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "abdications",
	    CTLFLAG_RD, &r->abdications, "# of consumer abdications");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "stalls",
	    CTLFLAG_RD, &r->stalls, "# of consumer stalls");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "cons_idle",
	    CTLFLAG_RD, &r->cons_idle,
	    "# of times consumer ran fully to completion");
	SYSCTL_ADD_COUNTER_U64(ctx, children, OID_AUTO, "cons_idle2",
	    CTLFLAG_RD, &r->cons_idle2,
	    "# of times consumer idled when another enqueue was in progress");
}

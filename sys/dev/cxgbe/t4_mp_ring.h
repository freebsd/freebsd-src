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
 *
 */

#ifndef __CXGBE_MP_RING_H
#define __CXGBE_MP_RING_H

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

struct mp_ring;
typedef u_int (*ring_drain_t)(struct mp_ring *, u_int, u_int, bool *);
typedef u_int (*ring_can_drain_t)(struct mp_ring *);

struct mp_ring {
	volatile uint64_t	state __aligned(CACHE_LINE_SIZE);
	struct malloc_type *	mt;

	int			size __aligned(CACHE_LINE_SIZE);
	void *			cookie;
	ring_drain_t		drain;
	ring_can_drain_t	can_drain;	/* cheap, may be unreliable */
	struct mtx *		cons_lock;
	counter_u64_t		dropped;
	counter_u64_t		consumer[4];
	counter_u64_t		not_consumer;
	counter_u64_t		abdications;
	counter_u64_t		consumed;
	counter_u64_t		cons_idle;
	counter_u64_t		cons_idle2;
	counter_u64_t		stalls;

	void * volatile		items[] __aligned(CACHE_LINE_SIZE);
};

int mp_ring_alloc(struct mp_ring **, int, void *, ring_drain_t,
    ring_can_drain_t, struct malloc_type *, struct mtx *, int);
void mp_ring_free(struct mp_ring *);
int mp_ring_enqueue(struct mp_ring *, void **, int, int);
int mp_ring_enqueue_only(struct mp_ring *, void **, int);
void mp_ring_check_drainage(struct mp_ring *, int);
void mp_ring_reset_stats(struct mp_ring *);
bool mp_ring_is_idle(struct mp_ring *);
void mp_ring_sysctls(struct mp_ring *, struct sysctl_ctx_list *,
    struct sysctl_oid_list *);

#endif

/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
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

#ifndef __MACHINE_COUNTER_H__
#define __MACHINE_COUNTER_H__

#include <sys/pcpu.h>
#include <machine/atomic.h>
#ifdef INVARIANTS
#include <sys/proc.h>
#endif

#define	counter_enter()	critical_enter()
#define	counter_exit()	critical_exit()

#ifdef IN_SUBR_COUNTER_C

static inline uint64_t
counter_u64_read_one(uint64_t *p, int cpu)
{

	return (atomic_load_64((uint64_t *)((char *)p + sizeof(struct pcpu) *
	    cpu)));
}

static inline uint64_t
counter_u64_fetch_inline(uint64_t *p)
{
	uint64_t r;
	int i;

	r = 0;
	for (i = 0; i < mp_ncpus; i++)
		r += counter_u64_read_one((uint64_t *)p, i);

	return (r);
}

static void
counter_u64_zero_one_cpu(void *arg)
{

	atomic_store_64((uint64_t *)((char *)arg + sizeof(struct pcpu) *
	    PCPU_GET(cpuid)), 0);
}

static inline void
counter_u64_zero_inline(counter_u64_t c)
{

	smp_rendezvous(smp_no_rendevous_barrier, counter_u64_zero_one_cpu,
	    smp_no_rendevous_barrier, c);
}
#endif

#define	counter_u64_add_protected(c, inc)	do {	\
	CRITICAL_ASSERT(curthread);			\
	atomic_add_64((uint64_t *)zpcpu_get(c), (inc));	\
} while (0)

static inline void
counter_u64_add(counter_u64_t c, int64_t inc)
{

	counter_enter();
	counter_u64_add_protected(c, inc);
	counter_exit();
}

#endif	/* ! __MACHINE_COUNTER_H__ */

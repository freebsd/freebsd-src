/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Justin Hibbits
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/param.h>

#include <vm/vm.h>
#include <vm/pmap.h>

enum {
    ATOMIC64_ADD,
    ATOMIC64_CLEAR,
    ATOMIC64_CMPSET,
    ATOMIC64_FCMPSET,
    ATOMIC64_FETCHADD,
    ATOMIC64_LOAD,
    ATOMIC64_SET,
    ATOMIC64_SUBTRACT,
    ATOMIC64_STORE,
    ATOMIC64_SWAP
};

#ifdef _KERNEL
#ifdef SMP

#define	A64_POOL_SIZE	MAXCPU
/* Estimated size of a cacheline */
#define	CACHE_ALIGN	CACHE_LINE_SIZE
static struct mtx a64_mtx_pool[A64_POOL_SIZE];

#define GET_MUTEX(p) \
    (&a64_mtx_pool[(pmap_kextract((vm_offset_t)p) / CACHE_ALIGN) % (A64_POOL_SIZE)])

#define LOCK_A64()			\
    struct mtx *_amtx = GET_MUTEX(p);	\
    if (smp_started) mtx_lock(_amtx)

#define UNLOCK_A64()	if (smp_started) mtx_unlock(_amtx)

#else	/* !SMP */

#define	LOCK_A64()	{ register_t s = intr_disable()
#define	UNLOCK_A64()	intr_restore(s); }

#endif	/* SMP */

#define ATOMIC64_EMU_UN(op, rt, block, ret) \
    rt \
    atomic_##op##_64(volatile uint64_t *p) {			\
	uint64_t tmp __unused;					\
	LOCK_A64();						\
	block;							\
	UNLOCK_A64();						\
	ret; } struct hack

#define	ATOMIC64_EMU_BIN(op, rt, block, ret) \
    rt \
    atomic_##op##_64(volatile uint64_t *p, uint64_t v) {	\
	uint64_t tmp __unused;					\
	LOCK_A64();						\
	block;							\
	UNLOCK_A64();						\
	ret; } struct hack

ATOMIC64_EMU_BIN(add, void, (*p = *p + v), return);
ATOMIC64_EMU_BIN(clear, void, *p &= ~v, return);
ATOMIC64_EMU_BIN(fetchadd, uint64_t, (*p = *p + v, v = *p - v), return (v));
ATOMIC64_EMU_UN(load, uint64_t, (tmp = *p), return (tmp));
ATOMIC64_EMU_BIN(set, void, *p |= v, return);
ATOMIC64_EMU_BIN(subtract, void, (*p = *p - v), return);
ATOMIC64_EMU_BIN(store, void, *p = v, return);
ATOMIC64_EMU_BIN(swap, uint64_t, tmp = *p; *p = v; v = tmp, return(v));

int atomic_cmpset_64(volatile uint64_t *p, uint64_t old, uint64_t new)
{
	uint64_t tmp;

	LOCK_A64();
	tmp = *p;
	if (tmp == old)
		*p = new;
	UNLOCK_A64();

	return (tmp == old);
}

int atomic_fcmpset_64(volatile uint64_t *p, uint64_t *old, uint64_t new)
{
	uint64_t tmp, tmp_old;

	LOCK_A64();
	tmp = *p;
	tmp_old = *old;
	if (tmp == tmp_old)
		*p = new;
	else
		*old = tmp;
	UNLOCK_A64();

	return (tmp == tmp_old);
}

#ifdef SMP
static void
atomic64_mtxinit(void *x __unused)
{
	int i;

	for (i = 0; i < A64_POOL_SIZE; i++)
		mtx_init(&a64_mtx_pool[i], "atomic64 mutex", NULL, MTX_DEF);
}

SYSINIT(atomic64_mtxinit, SI_SUB_LOCK, SI_ORDER_MIDDLE, atomic64_mtxinit, NULL);
#endif	/* SMP */

#endif	/* _KERNEL */

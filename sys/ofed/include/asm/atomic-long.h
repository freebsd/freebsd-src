/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_ATOMIC_LONG_H_
#define	_ATOMIC_LONG_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <machine/atomic.h>

typedef struct {
	volatile long counter;
} atomic_long_t;

#define	atomic_long_add(i, v)		atomic_long_add_return((i), (v))
#define	atomic_long_inc_return(v)	atomic_long_add_return(1, (v))

static inline long
atomic_long_add_return(long i, atomic_long_t *v)
{
	return i + atomic_fetchadd_long(&v->counter, i);
}

static inline void
atomic_long_set(atomic_long_t *v, long i)
{
	atomic_store_rel_long(&v->counter, i);
}

static inline long
atomic_long_read(atomic_long_t *v)
{
	return atomic_load_acq_long(&v->counter);
}

static inline long
atomic_long_inc(atomic_long_t *v)
{
	return atomic_fetchadd_long(&v->counter, 1) + 1;
}

static inline long
atomic_long_dec(atomic_long_t *v)
{
	return atomic_fetchadd_long(&v->counter, -1) - 1;
}

static inline long
atomic_long_dec_and_test(atomic_long_t *v)
{
	long i = atomic_long_add(-1, v);
	return i == 0 ;
}

#endif	/* _ATOMIC_LONG_H_ */

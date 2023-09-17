/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _GDMA_UTIL_H_
#define _GDMA_UTIL_H_

#include <sys/types.h>
#include <sys/param.h>

/* Log Levels */
#define MANA_ALERT	(1 << 0) /* Alerts are providing more error info.     */
#define MANA_WARNING	(1 << 1) /* Driver output is more error sensitive.    */
#define MANA_INFO	(1 << 2) /* Provides additional driver info.	      */
#define MANA_DBG	(1 << 3) /* Driver output for debugging.	      */

extern int mana_log_level;

#define mana_trace_raw(ctx, level, fmt, args...)			\
	do {							\
		((void)(ctx));					\
		if (((level) & mana_log_level) != (level))	\
			break;					\
		printf(fmt, ##args);				\
	} while (0)

#define mana_trace(ctx, level, fmt, args...)			\
	mana_trace_raw(ctx, level, "%s() [TID:%d]: "		\
	    fmt, __func__, curthread->td_tid, ##args)


#define mana_dbg(ctx, format, arg...)		\
	mana_trace(ctx, MANA_DBG, format, ##arg)
#define mana_info(ctx, format, arg...)		\
	mana_trace(ctx, MANA_INFO, format, ##arg)
#define mana_warn(ctx, format, arg...)		\
	mana_trace(ctx, MANA_WARNING, format, ##arg)
#define mana_err(ctx, format, arg...)		\
	mana_trace(ctx, MANA_ALERT, format, ##arg)

#define unlikely(x)	__predict_false(!!(x))
#define likely(x)	__predict_true(!!(x))


#define BITS_PER_LONG			(sizeof(long) * NBBY)

#define BITMAP_FIRST_WORD_MASK(start)	(~0UL << ((start) % BITS_PER_LONG))
#define BITMAP_LAST_WORD_MASK(n)	(~0UL >> (BITS_PER_LONG - (n)))
#define	BITS_TO_LONGS(n)	howmany((n), BITS_PER_LONG)
#define	BIT_MASK(nr)		(1UL << ((nr) & (BITS_PER_LONG - 1)))
#define	BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#undef	ALIGN
#define ALIGN(x, y)		roundup2((x), (y))
#define IS_ALIGNED(x, a)	(((x) & ((__typeof(x))(a) - 1)) == 0)

#define BIT(n)			(1ULL << (n))

#define PHYS_PFN(x)		((unsigned long)((x) >> PAGE_SHIFT))
#define offset_in_page(x)	((x) & PAGE_MASK)

#define min_t(type, _x, _y)						\
    ((type)(_x) < (type)(_y) ? (type)(_x) : (type)(_y))

#define test_bit(i, a)							\
    ((((volatile const unsigned long *)(a))[BIT_WORD(i)]) & BIT_MASK(i))

typedef volatile uint32_t atomic_t;

#define	atomic_add_return(v, p)		(atomic_fetchadd_int(p, v) + (v))
#define	atomic_sub_return(v, p)		(atomic_fetchadd_int(p, -(v)) - (v))
#define	atomic_inc_return(p)		atomic_add_return(1, p)
#define	atomic_dec_return(p)		atomic_sub_return(1, p)
#define atomic_read(p)			atomic_add_return(0, p)

#define usleep_range(_1, _2)						\
    pause_sbt("gdma-usleep-range", SBT_1US * _1, SBT_1US * 1, C_ABSOLUTE)

static inline void
gdma_msleep(unsigned int ms)
{
	if (ms == 0)
		ms = 1;
	pause_sbt("gdma-msleep", mstosbt(ms), 0, C_HARDCLOCK);
}

static inline void
bitmap_set(unsigned long *map, unsigned int start, int nr)
{
	const unsigned int size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	map += BIT_WORD(start);

	while (nr - bits_to_set >= 0) {
		*map |= mask_to_set;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		map++;
	}

	if (nr) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		*map |= mask_to_set;
	}
}

static inline void
bitmap_clear(unsigned long *map, unsigned int start, int nr)
{
	const unsigned int size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	map += BIT_WORD(start);

	while (nr - bits_to_clear >= 0) {
		*map &= ~mask_to_clear;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		map++;
	}

	if (nr) {
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		*map &= ~mask_to_clear;
	}
}

static inline unsigned long
find_first_zero_bit(const unsigned long *p, unsigned long max)
{
	unsigned long i, n;

	for (i = 0; i < max / BITS_PER_LONG + 1; i++) {
		n = ~p[i];
		if (n != 0)
			return (i * BITS_PER_LONG + ffsl(n) - 1);
	}
	return (max);
}

static inline unsigned long
ilog2(unsigned long x)
{
	unsigned long log = x;
	while (x >>= 1)
		log++;
	return (log);
}

static inline unsigned long
roundup_pow_of_two(unsigned long x)
{
	return (1UL << flsl(x - 1));
}

static inline int
is_power_of_2(unsigned long n)
{
	return (n == roundup_pow_of_two(n));
}

struct completion {
	unsigned int done;
	struct mtx lock;
};

void init_completion(struct completion *c);
void free_completion(struct completion *c);
void complete(struct completion *c);
void wait_for_completion(struct completion *c);
int wait_for_completion_timeout(struct completion *c, int timeout);
#endif /* _GDMA_UTIL_H_ */

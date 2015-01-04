/*-
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014 Mellanox Technologies, Ltd. All rights reserved.
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

#ifndef _LINUX_MATH64_H
#define _LINUX_MATH64_H

#include <linux/types.h>
#include <linux/bitops.h>

#if BITS_PER_LONG == 64
 
# define do_div(n, base) ({                                    \
	uint32_t __base = (base);                               \
	uint32_t __rem;                                         \
	__rem = ((uint64_t)(n)) % __base;                       \
	(n) = ((uint64_t)(n)) / __base;                         \
	__rem;                                                  \
})

/**
* div_u64_rem - unsigned 64bit divide with 32bit divisor with remainder
*
* This is commonly provided by 32bit archs to provide an optimized 64bit
* divide.
*/
static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
        *remainder = dividend % divisor;
        return dividend / divisor;
}


#elif BITS_PER_LONG == 32

static uint32_t __div64_32(uint64_t *n, uint32_t base)
{
	uint64_t rem = *n;
	uint64_t b = base;
	uint64_t res, d = 1;
	uint32_t high = rem >> 32;

	/* Reduce the thing a bit first */
	res = 0;
	if (high >= base) {
		high /= base;
		res = (uint64_t) high << 32;
		rem -= (uint64_t) (high*base) << 32;
	}

	while ((int64_t)b > 0 && b < rem) {
		b = b+b;
		d = d+d;
	}

	do {
		if (rem >= b) {
			rem -= b;
			res += d;
		}
		b >>= 1;
		d >>= 1;
	} while (d);

	*n = res;
	return rem;
}
 
# define do_div(n, base) ({                            \
	uint32_t __base = (base);                       \
	uint32_t __rem;                                 \
	(void)(((typeof((n)) *)0) == ((uint64_t *)0));  \
	if (likely(((n) >> 32) == 0)) {                 \
		__rem = (uint32_t)(n) % __base;         \
		(n) = (uint32_t)(n) / __base;           \
	} else                                          \
		__rem = __div64_32(&(n), __base);       \
	__rem;                                          \
})

#ifndef div_u64_rem
static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
        *remainder = do_div(dividend, divisor);
        return dividend;
}
#endif
 

#endif /* BITS_PER_LONG */



/**
 ** div_u64 - unsigned 64bit divide with 32bit divisor
 **
 ** This is the most common 64bit divide and should be used if possible,
 ** as many 32bit archs can optimize this variant better than a full 64bit
 ** divide.
 *  */
#ifndef div_u64

static inline u64 div_u64(u64 dividend, u32 divisor)
{
        u32 remainder;
        return div_u64_rem(dividend, divisor, &remainder);
}
#endif

#endif	/* _LINUX_MATH64_H */

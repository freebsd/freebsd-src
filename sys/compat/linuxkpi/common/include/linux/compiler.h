/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
 * Copyright (c) 2015 François Tigeot
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
#ifndef	_LINUXKPI_LINUX_COMPILER_H_
#define	_LINUXKPI_LINUX_COMPILER_H_

#include <sys/cdefs.h>
#include <sys/endian.h>

#include <compat/linuxkpi/common/include/linux/compiler_types.h>

#define	__bitwise
#define __devinitdata
#define __init
#define	__initconst
#define	__devinit
#define	__devexit
#define __exit
#define	____cacheline_aligned		__aligned(CACHE_LINE_SIZE)
#define	____cacheline_aligned_in_smp	__aligned(CACHE_LINE_SIZE)

#if __has_attribute(__counted_by__)
#define	__counted_by(_x)		__attribute__((__counted_by__(_x)))
#else
#define	__counted_by(_x)
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define	__counted_by_le(_x)		__counted_by(_x)
#define	__counted_by_be(_x)
#else
#define	__counted_by_le(_x)
#define	__counted_by_be(_x)		__counted_by(_x)
#endif

#define	likely(x)			__builtin_expect(!!(x), 1)
#define	unlikely(x)			__builtin_expect(!!(x), 0)
#define typeof(x)			__typeof(x)

#define	uninitialized_var(x)		x = x

#define	barrier()			__asm__ __volatile__("": : :"memory")

#define	lower_32_bits(n)		((u32)(n))
#define	upper_32_bits(n)		((u32)(((n) >> 16) >> 16))

#define	WRITE_ONCE(x,v) do {		\
	barrier();			\
	(*(volatile __typeof(x) *)(uintptr_t)&(x)) = (v); \
	barrier();			\
} while (0)

#define	READ_ONCE(x) ({			\
	__typeof(x) __var = ({		\
		barrier();		\
		(*(const volatile __typeof(x) *)&(x)); \
	});				\
	barrier();			\
	__var;				\
})

#define	lockless_dereference(p) READ_ONCE(p)

#define	_AT(T,X)	((T)(X))
#define	__must_be_array(a)	__same_type(a, &(a)[0])

#define	sizeof_field(_s, _m)	sizeof(((_s *)0)->_m)

#define is_signed_type(t)	((t)-1 < (t)1)
#define is_unsigned_type(t)	((t)-1 > (t)1)

#if __has_builtin(__builtin_dynamic_object_size)
#define	__struct_size(_s)	__builtin_dynamic_object_size(_s, 0)
#else
#define	__struct_size(_s)	__builtin_object_size(_s, 0)
#endif

#endif	/* _LINUXKPI_LINUX_COMPILER_H_ */

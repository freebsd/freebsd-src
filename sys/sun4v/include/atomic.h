/*-
 * Copyright (c) 1998 Doug Rabson.
 * Copyright (c) 2001 Jake Burkholder.
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
 *	from: FreeBSD: src/sys/i386/include/atomic.h,v 1.20 2001/02/11
 * $FreeBSD: src/sys/sun4v/include/atomic.h,v 1.1 2006/10/05 06:14:25 kmacy Exp $
 */

#ifndef	_MACHINE_ATOMIC_H_
#define	_MACHINE_ATOMIC_H_

#include <machine/cpufunc.h>

/* Userland needs different ASI's. */
#ifdef _KERNEL
#define	__ASI_ATOMIC	ASI_N
#else
#define	__ASI_ATOMIC	ASI_P
#endif

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and multiple processors.  See atomic(9) for details.
 * Note that efficient hardware support exists only for the 32 and 64
 * bit variants; the 8 and 16 bit versions are not provided and should
 * not be used in MI code.
 *
 * This implementation takes advantage of the fact that the sparc64
 * cas instruction is both a load and a store.  The loop is often coded
 * as follows:
 *
 *	do {
 *		expect = *p;
 *		new = expect + 1;
 *	} while (cas(p, expect, new) != expect);
 *
 * which performs an unnnecessary load on each iteration that the cas
 * operation fails.  Modified as follows:
 *
 *	expect = *p;
 *	for (;;) {
 *		new = expect + 1;
 *		result = cas(p, expect, new);
 *		if (result == expect)
 *			break;
 *		expect = result;
 *	}
 *
 * the return value of cas is used to avoid the extra reload.
 *
 * The memory barriers provided by the acq and rel variants are intended
 * to be sufficient for use of relaxed memory ordering.  Due to the
 * suggested assembly syntax of the membar operands containing a #
 * character, they cannot be used in macros.  The cmask and mmask bits
 * are hard coded in machine/cpufunc.h and used here through macros.
 * Hopefully sun will choose not to change the bit numbers.
 */

#define	itype(sz)	uint ## sz ## _t

#define	atomic_cas_32(p, e, s)	casa(p, e, s, __ASI_ATOMIC)
#define	atomic_cas_64(p, e, s)	casxa(p, e, s, __ASI_ATOMIC)

#define	atomic_cas(p, e, s, sz)						\
	atomic_cas_ ## sz(p, e, s)

#define	atomic_cas_acq(p, e, s, sz) ({					\
	itype(sz) v;							\
	v = atomic_cas(p, e, s, sz);					\
	membar(LoadLoad | LoadStore);					\
	v;								\
})

#define	atomic_cas_rel(p, e, s, sz) ({					\
	itype(sz) v;							\
	membar(LoadStore | StoreStore);					\
	v = atomic_cas(p, e, s, sz);					\
	v;								\
})

#define	atomic_op(p, op, v, sz) ({					\
	itype(sz) e, r, s;						\
	for (e = *(volatile itype(sz) *)p;; e = r) {			\
		s = e op v;						\
		r = atomic_cas_ ## sz(p, e, s);				\
		if (r == e)						\
			break;						\
	}								\
	e;								\
})

#define	atomic_op_acq(p, op, v, sz) ({					\
	itype(sz) t;							\
	t = atomic_op(p, op, v, sz);					\
	membar(LoadLoad | LoadStore);					\
	t;								\
})

#define	atomic_op_rel(p, op, v, sz) ({					\
	itype(sz) t;							\
	membar(LoadStore | StoreStore);					\
	t = atomic_op(p, op, v, sz);					\
	t;								\
})

#define	atomic_load(p, sz)						\
	atomic_cas(p, 0, 0, sz)

#define	atomic_load_acq(p, sz) ({					\
	itype(sz) v;							\
	v = atomic_load(p, sz);						\
	membar(LoadLoad | LoadStore);					\
	v;								\
})

#define	atomic_load_clear(p, sz) ({					\
	itype(sz) e, r;							\
	for (e = *(volatile itype(sz) *)p;; e = r) {			\
		r = atomic_cas(p, e, 0, sz);				\
		if (r == e)						\
			break;						\
	}								\
	e;								\
})

#define	atomic_store(p, v, sz) do {					\
	itype(sz) e, r;							\
	for (e = *(volatile itype(sz) *)p;; e = r) {			\
		r = atomic_cas(p, e, v, sz);				\
		if (r == e)						\
			break;						\
	}								\
} while (0)

#define	atomic_store_rel(p, v, sz) do {					\
	membar(LoadStore | StoreStore);					\
	atomic_store(p, v, sz);						\
} while (0)

#define	ATOMIC_GEN(name, ptype, vtype, atype, sz)			\
									\
static __inline vtype							\
atomic_add_ ## name(volatile ptype p, atype v)				\
{									\
	return ((vtype)atomic_op(p, +, v, sz));				\
}									\
static __inline vtype							\
atomic_add_acq_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_acq(p, +, v, sz));			\
}									\
static __inline vtype							\
atomic_add_rel_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_rel(p, +, v, sz));			\
}									\
									\
static __inline vtype							\
atomic_clear_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op(p, &, ~v, sz));			\
}									\
static __inline vtype							\
atomic_clear_acq_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_acq(p, &, ~v, sz));			\
}									\
static __inline vtype							\
atomic_clear_rel_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_rel(p, &, ~v, sz));			\
}									\
									\
static __inline int							\
atomic_cmpset_ ## name(volatile ptype p, vtype e, vtype s)		\
{									\
	return (((vtype)atomic_cas(p, e, s, sz)) == e);			\
}									\
static __inline int							\
atomic_cmpset_acq_ ## name(volatile ptype p, vtype e, vtype s)		\
{									\
	return (((vtype)atomic_cas_acq(p, e, s, sz)) == e);		\
}									\
static __inline int							\
atomic_cmpset_rel_ ## name(volatile ptype p, vtype e, vtype s)		\
{									\
	return (((vtype)atomic_cas_rel(p, e, s, sz)) == e);		\
}									\
									\
static __inline vtype							\
atomic_load_ ## name(volatile ptype p)					\
{									\
	return ((vtype)atomic_cas(p, 0, 0, sz));			\
}									\
static __inline vtype							\
atomic_load_acq_ ## name(volatile ptype p)				\
{									\
	return ((vtype)atomic_cas_acq(p, 0, 0, sz));			\
}									\
									\
static __inline vtype							\
atomic_readandclear_ ## name(volatile ptype p)				\
{									\
	return ((vtype)atomic_load_clear(p, sz));			\
}									\
									\
static __inline vtype							\
atomic_set_ ## name(volatile ptype p, atype v)				\
{									\
	return ((vtype)atomic_op(p, |, v, sz));				\
}									\
static __inline vtype							\
atomic_set_acq_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_acq(p, |, v, sz));			\
}									\
static __inline vtype							\
atomic_set_rel_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_rel(p, |, v, sz));			\
}									\
									\
static __inline vtype							\
atomic_subtract_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op(p, -, v, sz));				\
}									\
static __inline vtype							\
atomic_subtract_acq_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_acq(p, -, v, sz));			\
}									\
static __inline vtype							\
atomic_subtract_rel_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_rel(p, -, v, sz));			\
}									\
									\
static __inline void							\
atomic_store_ ## name(volatile ptype p, vtype v)			\
{									\
	atomic_store(p, v, sz);						\
}									\
static __inline void							\
atomic_store_rel_ ## name(volatile ptype p, vtype v)			\
{									\
	atomic_store_rel(p, v, sz);					\
}

ATOMIC_GEN(int, u_int *, u_int, u_int, 32);
ATOMIC_GEN(32, uint32_t *, uint32_t, uint32_t, 32);

ATOMIC_GEN(long, u_long *, u_long, u_long, 64);
ATOMIC_GEN(64, uint64_t *, uint64_t, uint64_t, 64);

ATOMIC_GEN(ptr, uintptr_t *, uintptr_t, uintptr_t, 64);

#define	atomic_fetchadd_int	atomic_add_int
#define	atomic_fetchadd_32	atomic_add_32

#undef ATOMIC_GEN
#undef atomic_cas
#undef atomic_cas_acq
#undef atomic_cas_rel
#undef atomic_op
#undef atomic_op_acq
#undef atomic_op_rel
#undef atomic_load_acq
#undef atomic_store_rel
#undef atomic_load_clear

#endif /* !_MACHINE_ATOMIC_H_ */

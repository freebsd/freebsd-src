/*-
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
 *
 * $FreeBSD$
 */

/*
 * This is not atomic.  It is just a stub to make things compile.
 */

#ifndef	_MACHINE_ATOMIC_H_
#define	_MACHINE_ATOMIC_H_

#define	__atomic_op(p, op, v) ({					\
	__typeof(*p) __v = (__typeof(*p))v;				\
	*p op __v;							\
})

#define	__atomic_load(p) ({						\
	__typeof(*p) __v;						\
	__v = *p;							\
	__v;								\
})

#define	__atomic_load_clear(p) ({					\
	__typeof(*p) __v;						\
	__v = *p;							\
	*p = 0;								\
	__v;								\
})

#define	__atomic_cas(p, e, s) ({					\
	u_int __v;							\
	if (*p == (__typeof(*p))e) {					\
		*p = (__typeof(*p))s;					\
		__v = 1;						\
	} else {							\
		__v = 0;						\
	}								\
	__v;								\
})

#define	__atomic_op_8(p, op, v)		__atomic_op(p, op, v)
#define	__atomic_op_16(p, op, v)	__atomic_op(p, op, v)
#define	__atomic_op_32(p, op, v)	__atomic_op(p, op, v)
#define	__atomic_load_32(p)		__atomic_load(p)
#define	__atomic_load_clear_32(p)	__atomic_load_clear(p)
#define	__atomic_cas_32(p, e, s)	__atomic_cas(p, e, s)
#define	__atomic_op_64(p, op, v)	__atomic_op(p, op, v)
#define	__atomic_load_64(p)		__atomic_load(p)
#define	__atomic_load_clear_64(p)	__atomic_load_clear(p)
#define	__atomic_cas_64(p, e, s)	__atomic_cas(p, e, s)

#define	atomic_add_8(p, v)		__atomic_op_8(p, +=, v)
#define	atomic_subtract_8(p, v)		__atomic_op_8(p, -=, v)
#define	atomic_set_8(p, v)		__atomic_op_8(p, |=, v)
#define	atomic_clear_8(p, v)		__atomic_op_8(p, &=, ~v)
#define	atomic_store_8(p, v)		__atomic_op_8(p, =, v)

#define	atomic_add_16(p, v)		__atomic_op_16(p, +=, v)
#define	atomic_subtract_16(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_16(p, v)		__atomic_op_16(p, |=, v)
#define	atomic_clear_16(p, v)		__atomic_op_16(p, &=, ~v)
#define	atomic_store_16(p, v)		__atomic_op_16(p, =, v)

#define	atomic_add_32(p, v)		__atomic_op_32(p, +=, v)
#define	atomic_subtract_32(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_32(p, v)		__atomic_op_32(p, |=, v)
#define	atomic_clear_32(p, v)		__atomic_op_32(p, &=, ~v)
#define	atomic_store_32(p, v)		__atomic_op_32(p, =, v)
#define	atomic_load_32(p)		__atomic_load_32(p)
#define	atomic_readandclear_32(p)	__atomic_load_clear_32(p)
#define	atomic_cmpset_32(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_64(p, v)		__atomic_op_64(p, +=, v)
#define	atomic_subtract_64(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_64(p, v)		__atomic_op_64(p, |=, v)
#define	atomic_clear_64(p, v)		__atomic_op_64(p, &=, ~v)
#define	atomic_store_64(p, v)		__atomic_op_64(p, =, v)
#define	atomic_load_64(p)		__atomic_load_64(p)
#define	atomic_readandclear_64(p)	__atomic_load_clear_64(p)
#define	atomic_cmpset_64(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_acq_8(p, v)		__atomic_op_8(p, +=, v)
#define	atomic_subtract_acq_8(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_acq_8(p, v)		__atomic_op_8(p, |=, v)
#define	atomic_clear_acq_8(p, v)	__atomic_op_8(p, &=, ~v)
#define	atomic_store_acq_8(p, v)	__atomic_op_8(p, =, v)

#define	atomic_add_acq_16(p, v)		__atomic_op_16(p, +=, v)
#define	atomic_subtract_acq_16(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_acq_16(p, v)		__atomic_op_16(p, |=, v)
#define	atomic_clear_acq_16(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_acq_16(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_acq_32(p, v)		__atomic_op_32(p, +=, v)
#define	atomic_subtract_acq_32(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_acq_32(p, v)		__atomic_op_32(p, |=, v)
#define	atomic_clear_acq_32(p, v)	__atomic_op_32(p, &=, ~v)
#define	atomic_store_acq_32(p, v)	__atomic_op_32(p, =, v)
#define	atomic_load_acq_32(p)		__atomic_load_32(p)
#define	atomic_cmpset_acq_32(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_acq_64(p, v)		__atomic_op_64(p, +=, v)
#define	atomic_subtract_acq_64(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_acq_64(p, v)		__atomic_op_64(p, |=, v)
#define	atomic_clear_acq_64(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_acq_64(p, v)	__atomic_op_64(p, =, v)
#define	atomic_load_acq_64(p)		__atomic_load_64(p)
#define	atomic_cmpset_acq_64(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_rel_8(p, v)		__atomic_op_8(p, +=, v)
#define	atomic_subtract_rel_8(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_rel_8(p, v)		__atomic_op_8(p, |=, v)
#define	atomic_clear_rel_8(p, v)	__atomic_op_8(p, &=, ~v)
#define	atomic_store_rel_8(p, v)	__atomic_op_8(p, =, v)

#define	atomic_add_rel_16(p, v)		__atomic_op_16(p, +=, v)
#define	atomic_subtract_rel_16(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_rel_16(p, v)		__atomic_op_16(p, |=, v)
#define	atomic_clear_rel_16(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_rel_16(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_rel_32(p, v)		__atomic_op_32(p, +=, v)
#define	atomic_subtract_rel_32(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_rel_32(p, v)		__atomic_op_32(p, |=, v)
#define	atomic_clear_rel_32(p, v)	__atomic_op_32(p, &=, ~v)
#define	atomic_store_rel_32(p, v)	__atomic_op_32(p, =, v)
#define	atomic_cmpset_rel_32(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_rel_64(p, v)		__atomic_op_64(p, +=, v)
#define	atomic_subtract_rel_64(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_rel_64(p, v)		__atomic_op_64(p, |=, v)
#define	atomic_clear_rel_64(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_rel_64(p, v)	__atomic_op_64(p, =, v)
#define	atomic_cmpset_rel_64(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_char(p, v)		__atomic_op_8(p, +=, v)
#define	atomic_subtract_char(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_char(p, v)		__atomic_op_8(p, |=, v)
#define	atomic_clear_char(p, v)		__atomic_op_8(p, &=, ~v)
#define	atomic_store_char(p, v)		__atomic_op_8(p, =, v)

#define	atomic_add_short(p, v)		__atomic_op_16(p, +=, v)
#define	atomic_subtract_short(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_short(p, v)		__atomic_op_16(p, |=, v)
#define	atomic_clear_short(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_short(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_int(p, v)		__atomic_op_32(p, +=, v)
#define	atomic_subtract_int(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_int(p, v)		__atomic_op_32(p, |=, v)
#define	atomic_clear_int(p, v)		__atomic_op_32(p, &=, ~v)
#define	atomic_store_int(p, v)		__atomic_op_32(p, =, v)
#define	atomic_load_int(p)		__atomic_load_32(p)
#define	atomic_readandclear_int(p)	__atomic_load_clear_32(p)
#define	atomic_cmpset_int(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_long(p, v)		__atomic_op_64(p, +=, v)
#define	atomic_subtract_long(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_long(p, v)		__atomic_op_64(p, |=, v)
#define	atomic_clear_long(p, v)		__atomic_op_64(p, &=, ~v)
#define	atomic_store_long(p, v)		__atomic_op_64(p, =, v)
#define	atomic_load_long(p)		__atomic_load_64(p)
#define	atomic_readandclear_long(p)	__atomic_load_clear_64(p)
#define	atomic_cmpset_long(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_acq_char(p, v)	__atomic_op_8(p, +=, v)
#define	atomic_subtract_acq_char(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_acq_char(p, v)	__atomic_op_8(p, |=, v)
#define	atomic_clear_acq_char(p, v)	__atomic_op_8(p, &=, ~v)
#define	atomic_store_acq_char(p, v)	__atomic_op_8(p, =, v)

#define	atomic_add_acq_short(p, v)	__atomic_op_16(p, +=, v)
#define	atomic_subtract_acq_short(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_acq_short(p, v)	__atomic_op_16(p, |=, v)
#define	atomic_clear_acq_short(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_acq_short(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_acq_int(p, v)	__atomic_op_32(p, +=, v)
#define	atomic_subtract_acq_int(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_acq_int(p, v)	__atomic_op_32(p, |=, v)
#define	atomic_clear_acq_int(p, v)	__atomic_op_32(p, &=, ~v)
#define	atomic_store_acq_int(p, v)	__atomic_op_32(p, =, v)
#define	atomic_load_acq_int(p)		__atomic_load_32(p)
#define	atomic_cmpset_acq_int(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_acq_long(p, v)	__atomic_op_64(p, +=, v)
#define	atomic_subtract_acq_long(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_acq_long(p, v)	__atomic_op_64(p, |=, v)
#define	atomic_clear_acq_long(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_acq_long(p, v)	__atomic_op_64(p, =, v)
#define	atomic_load_acq_long(p)		__atomic_load_64(p)
#define	atomic_cmpset_acq_long(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_rel_char(p, v)	__atomic_op_8(p, +=, v)
#define	atomic_subtract_rel_char(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_rel_char(p, v)	__atomic_op_8(p, |=, v)
#define	atomic_clear_rel_char(p, v)	__atomic_op_8(p, &=, ~v)
#define	atomic_store_rel_char(p, v)	__atomic_op_8(p, =, v)

#define	atomic_add_rel_short(p, v)	__atomic_op_16(p, +=, v)
#define	atomic_subtract_rel_short(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_rel_short(p, v)	__atomic_op_16(p, |=, v)
#define	atomic_clear_rel_short(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_rel_short(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_rel_int(p, v)	__atomic_op_32(p, +=, v)
#define	atomic_subtract_rel_int(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_rel_int(p, v)	__atomic_op_32(p, |=, v)
#define	atomic_clear_rel_int(p, v)	__atomic_op_32(p, &=, ~v)
#define	atomic_store_rel_int(p, v)	__atomic_op_32(p, =, v)
#define	atomic_cmpset_rel_int(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_rel_long(p, v)	__atomic_op_64(p, +=, v)
#define	atomic_subtract_rel_long(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_rel_long(p, v)	__atomic_op_64(p, |=, v)
#define	atomic_clear_rel_long(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_rel_long(p, v)	__atomic_op_64(p, =, v)
#define	atomic_cmpset_rel_long(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_char(p, v)		__atomic_op_8(p, +=, v)
#define	atomic_subtract_char(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_char(p, v)		__atomic_op_8(p, |=, v)
#define	atomic_clear_char(p, v)		__atomic_op_8(p, &=, ~v)
#define	atomic_store_char(p, v)		__atomic_op_8(p, =, v)

#define	atomic_add_short(p, v)		__atomic_op_16(p, +=, v)
#define	atomic_subtract_short(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_short(p, v)		__atomic_op_16(p, |=, v)
#define	atomic_clear_short(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_short(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_int(p, v)		__atomic_op_32(p, +=, v)
#define	atomic_subtract_int(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_int(p, v)		__atomic_op_32(p, |=, v)
#define	atomic_clear_int(p, v)		__atomic_op_32(p, &=, ~v)
#define	atomic_store_int(p, v)		__atomic_op_32(p, =, v)
#define	atomic_load_int(p)		__atomic_load_32(p)
#define	atomic_readandclear_int(p)	__atomic_load_clear_32(p)
#define	atomic_cmpset_int(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_long(p, v)		__atomic_op_64(p, +=, v)
#define	atomic_subtract_long(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_long(p, v)		__atomic_op_64(p, |=, v)
#define	atomic_clear_long(p, v)		__atomic_op_64(p, &=, ~v)
#define	atomic_store_long(p, v)		__atomic_op_64(p, =, v)
#define	atomic_load_long(p)		__atomic_load_64(p)
#define	atomic_readandclear_long(p)	__atomic_load_clear_64(p)
#define	atomic_cmpset_long(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_ptr(p, v)		__atomic_op_64(p, +=, v)
#define	atomic_subtract_ptr(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_ptr(p, v)		__atomic_op_64(p, |=, v)
#define	atomic_clear_ptr(p, v)		__atomic_op_64(p, &=, ~v)
#define	atomic_store_ptr(p, v)		__atomic_op_64(p, =, v)
#define	atomic_load_ptr(p)		__atomic_load_64(p)
#define	atomic_readandclear_ptr(p)	__atomic_load_clear_64(p)
#define	atomic_cmpset_ptr(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_acq_char(p, v)	__atomic_op_8(p, +=, v)
#define	atomic_subtract_acq_char(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_acq_char(p, v)	__atomic_op_8(p, |=, v)
#define	atomic_clear_acq_char(p, v)	__atomic_op_8(p, &=, ~v)
#define	atomic_store_acq_char(p, v)	__atomic_op_8(p, =, v)

#define	atomic_add_acq_short(p, v)	__atomic_op_16(p, +=, v)
#define	atomic_subtract_acq_short(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_acq_short(p, v)	__atomic_op_16(p, |=, v)
#define	atomic_clear_acq_short(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_acq_short(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_acq_int(p, v)	__atomic_op_32(p, +=, v)
#define	atomic_subtract_acq_int(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_acq_int(p, v)	__atomic_op_32(p, |=, v)
#define	atomic_clear_acq_int(p, v)	__atomic_op_32(p, &=, ~v)
#define	atomic_store_acq_int(p, v)	__atomic_op_32(p, =, v)
#define	atomic_load_acq_int(p)		__atomic_load_32(p)
#define	atomic_cmpset_acq_int(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_acq_long(p, v)	__atomic_op_64(p, +=, v)
#define	atomic_subtract_acq_long(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_acq_long(p, v)	__atomic_op_64(p, |=, v)
#define	atomic_clear_acq_long(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_acq_long(p, v)	__atomic_op_64(p, =, v)
#define	atomic_load_acq_long(p)		__atomic_load_64(p)
#define	atomic_cmpset_acq_long(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_acq_ptr(p, v)	__atomic_op_64(p, +=, v)
#define	atomic_subtract_acq_ptr(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_acq_ptr(p, v)	__atomic_op_64(p, |=, v)
#define	atomic_clear_acq_ptr(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_acq_ptr(p, v)	__atomic_op_64(p, =, v)
#define	atomic_load_acq_ptr(p)		__atomic_load_64(p)
#define	atomic_cmpset_acq_ptr(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_rel_char(p, v)	__atomic_op_8(p, +=, v)
#define	atomic_subtract_rel_char(p, v)	__atomic_op_8(p, -=, v)
#define	atomic_set_rel_char(p, v)	__atomic_op_8(p, |=, v)
#define	atomic_clear_rel_char(p, v)	__atomic_op_8(p, &=, ~v)
#define	atomic_store_rel_char(p, v)	__atomic_op_8(p, =, v)

#define	atomic_add_rel_short(p, v)	__atomic_op_16(p, +=, v)
#define	atomic_subtract_rel_short(p, v)	__atomic_op_16(p, -=, v)
#define	atomic_set_rel_short(p, v)	__atomic_op_16(p, |=, v)
#define	atomic_clear_rel_short(p, v)	__atomic_op_16(p, &=, ~v)
#define	atomic_store_rel_short(p, v)	__atomic_op_16(p, =, v)

#define	atomic_add_rel_int(p, v)	__atomic_op_32(p, +=, v)
#define	atomic_subtract_rel_int(p, v)	__atomic_op_32(p, -=, v)
#define	atomic_set_rel_int(p, v)	__atomic_op_32(p, |=, v)
#define	atomic_clear_rel_int(p, v)	__atomic_op_32(p, &=, ~v)
#define	atomic_store_rel_int(p, v)	__atomic_op_32(p, =, v)
#define	atomic_cmpset_rel_int(p, e, s)	__atomic_cas_32(p, e, s)

#define	atomic_add_rel_long(p, v)	__atomic_op_64(p, +=, v)
#define	atomic_subtract_rel_long(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_rel_long(p, v)	__atomic_op_64(p, |=, v)
#define	atomic_clear_rel_long(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_rel_long(p, v)	__atomic_op_64(p, =, v)
#define	atomic_cmpset_rel_long(p, e, s)	__atomic_cas_64(p, e, s)

#define	atomic_add_rel_ptr(p, v)	__atomic_op_64(p, +=, v)
#define	atomic_subtract_rel_ptr(p, v)	__atomic_op_64(p, -=, v)
#define	atomic_set_rel_ptr(p, v)	__atomic_op_64(p, |=, v)
#define	atomic_clear_rel_ptr(p, v)	__atomic_op_64(p, &=, ~v)
#define	atomic_store_rel_ptr(p, v)	__atomic_op_64(p, =, v)
#define	atomic_cmpset_rel_ptr(p, e, s)	__atomic_cas_64(p, e, s)

#endif /* !_MACHINE_ATOMIC_H_ */

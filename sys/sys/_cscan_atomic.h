/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andrew Turner
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef _SYS__CSAN_ATOMIC_H_
#define	_SYS__CSAN_ATOMIC_H_

#ifndef _MACHINE_ATOMIC_H_
#error do not include this header, use machine/atomic.h
#endif

#define	KCSAN_ATOMIC_FUNC_1(op, name, type)				\
	void kcsan_atomic_##op##_##name(volatile type *, type);		\
	void kcsan_atomic_##op##_acq_##name(volatile type *, type);	\
	void kcsan_atomic_##op##_rel_##name(volatile type *, type)

#define	KCSAN_ATOMIC_CMPSET(name, type)					\
	int kcsan_atomic_cmpset_##name(volatile type *, type, type);	\
	int kcsan_atomic_cmpset_acq_##name(volatile type *, type, type); \
	int kcsan_atomic_cmpset_rel_##name(volatile type *, type, type)

#define	KCSAN_ATOMIC_FCMPSET(name, type)				\
	int kcsan_atomic_fcmpset_##name(volatile type *, type *, type);	\
	int kcsan_atomic_fcmpset_acq_##name(volatile type *, type *, type); \
	int kcsan_atomic_fcmpset_rel_##name(volatile type *, type *, type)

#define	KCSAN_ATOMIC_READ(op, name, type)				\
	type kcsan_atomic_##op##_##name(volatile type *, type)

#define	KCSAN_ATOMIC_READANDCLEAR(name, type)				\
	type kcsan_atomic_readandclear_##name(volatile type *)

#define	KCSAN_ATOMIC_LOAD(name, type)					\
	type kcsan_atomic_load_##name(volatile type *);			\
	type kcsan_atomic_load_acq_##name(volatile type *)

#define	KCSAN_ATOMIC_STORE(name, type)					\
	void kcsan_atomic_store_##name(volatile type *, type);		\
	void kcsan_atomic_store_rel_##name(volatile type *, type)

#define	KCSAN_ATOMIC_TEST(op, name, type)				\
	int kcsan_atomic_##op##_##name(volatile type *, u_int);		\
	int kcsan_atomic_##op##_acq_##name(volatile type *, u_int)

#define	KCSAN_ATOMIC_FUNCS(name, type)					\
	KCSAN_ATOMIC_FUNC_1(add, name, type);				\
	KCSAN_ATOMIC_FUNC_1(clear, name, type);				\
	KCSAN_ATOMIC_CMPSET(name, type);				\
	KCSAN_ATOMIC_FCMPSET(name, type);				\
	KCSAN_ATOMIC_READ(fetchadd, name, type);			\
	KCSAN_ATOMIC_LOAD(name, type);					\
	KCSAN_ATOMIC_READANDCLEAR(name, type);				\
	KCSAN_ATOMIC_FUNC_1(set, name, type);				\
	KCSAN_ATOMIC_FUNC_1(subtract, name, type);			\
	KCSAN_ATOMIC_STORE(name, type);					\
	KCSAN_ATOMIC_READ(swap, name, type);				\
	KCSAN_ATOMIC_TEST(testandclear, name, type);			\
	KCSAN_ATOMIC_TEST(testandset, name, type)

KCSAN_ATOMIC_FUNCS(char, uint8_t);
KCSAN_ATOMIC_FUNCS(short, uint16_t);
KCSAN_ATOMIC_FUNCS(int, u_int);
KCSAN_ATOMIC_FUNCS(long, u_long);
KCSAN_ATOMIC_FUNCS(ptr, uintptr_t);
KCSAN_ATOMIC_FUNCS(8, uint8_t);
KCSAN_ATOMIC_FUNCS(16, uint16_t);
KCSAN_ATOMIC_FUNCS(32, uint32_t);
KCSAN_ATOMIC_FUNCS(64, uint64_t);

void	kcsan_atomic_thread_fence_acq(void);
void	kcsan_atomic_thread_fence_acq_rel(void);
void	kcsan_atomic_thread_fence_rel(void);
void	kcsan_atomic_thread_fence_seq_cst(void);

#ifndef KCSAN_RUNTIME

#define	atomic_add_char			kcsan_atomic_add_char
#define	atomic_add_acq_char		kcsan_atomic_add_acq_char
#define	atomic_add_rel_char		kcsan_atomic_add_rel_char
#define	atomic_clear_char		kcsan_atomic_clear_char
#define	atomic_clear_acq_char		kcsan_atomic_clear_acq_char
#define	atomic_clear_rel_char		kcsan_atomic_clear_rel_char
#define	atomic_cmpset_char		kcsan_atomic_cmpset_char
#define	atomic_cmpset_acq_char		kcsan_atomic_cmpset_acq_char
#define	atomic_cmpset_rel_char		kcsan_atomic_cmpset_rel_char
#define	atomic_fcmpset_char		kcsan_atomic_fcmpset_char
#define	atomic_fcmpset_acq_char		kcsan_atomic_fcmpset_acq_char
#define	atomic_fcmpset_rel_char		kcsan_atomic_fcmpset_rel_char
#define	atomic_fetchadd_char		kcsan_atomic_fetchadd_char
#define	atomic_load_char		kcsan_atomic_load_char
#define	atomic_load_acq_char		kcsan_atomic_load_acq_char
#define	atomic_readandclear_char	kcsan_atomic_readandclear_char
#define	atomic_set_char			kcsan_atomic_set_char
#define	atomic_set_acq_char		kcsan_atomic_set_acq_char
#define	atomic_set_rel_char		kcsan_atomic_set_rel_char
#define	atomic_subtract_char		kcsan_atomic_subtract_char
#define	atomic_subtract_acq_char	kcsan_atomic_subtract_acq_char
#define	atomic_subtract_rel_char	kcsan_atomic_subtract_rel_char
#define	atomic_store_char		kcsan_atomic_store_char
#define	atomic_store_rel_char		kcsan_atomic_store_rel_char
#define	atomic_swap_char		kcsan_atomic_swap_char
#define	atomic_testandclear_char	kcsan_atomic_testandclear_char
#define	atomic_testandset_char		kcsan_atomic_testandset_char

#define	atomic_add_short		kcsan_atomic_add_short
#define	atomic_add_acq_short		kcsan_atomic_add_acq_short
#define	atomic_add_rel_short		kcsan_atomic_add_rel_short
#define	atomic_clear_short		kcsan_atomic_clear_short
#define	atomic_clear_acq_short		kcsan_atomic_clear_acq_short
#define	atomic_clear_rel_short		kcsan_atomic_clear_rel_short
#define	atomic_cmpset_short		kcsan_atomic_cmpset_short
#define	atomic_cmpset_acq_short		kcsan_atomic_cmpset_acq_short
#define	atomic_cmpset_rel_short		kcsan_atomic_cmpset_rel_short
#define	atomic_fcmpset_short		kcsan_atomic_fcmpset_short
#define	atomic_fcmpset_acq_short	kcsan_atomic_fcmpset_acq_short
#define	atomic_fcmpset_rel_short	kcsan_atomic_fcmpset_rel_short
#define	atomic_fetchadd_short		kcsan_atomic_fetchadd_short
#define	atomic_load_short		kcsan_atomic_load_short
#define	atomic_load_acq_short		kcsan_atomic_load_acq_short
#define	atomic_readandclear_short	kcsan_atomic_readandclear_short
#define	atomic_set_short		kcsan_atomic_set_short
#define	atomic_set_acq_short		kcsan_atomic_set_acq_short
#define	atomic_set_rel_short		kcsan_atomic_set_rel_short
#define	atomic_subtract_short		kcsan_atomic_subtract_short
#define	atomic_subtract_acq_short	kcsan_atomic_subtract_acq_short
#define	atomic_subtract_rel_short	kcsan_atomic_subtract_rel_short
#define	atomic_store_short		kcsan_atomic_store_short
#define	atomic_store_rel_short		kcsan_atomic_store_rel_short
#define	atomic_swap_short		kcsan_atomic_swap_short
#define	atomic_testandclear_short	kcsan_atomic_testandclear_short
#define	atomic_testandset_short		kcsan_atomic_testandset_short

#define	atomic_add_int			kcsan_atomic_add_int
#define	atomic_add_acq_int		kcsan_atomic_add_acq_int
#define	atomic_add_rel_int		kcsan_atomic_add_rel_int
#define	atomic_clear_int		kcsan_atomic_clear_int
#define	atomic_clear_acq_int		kcsan_atomic_clear_acq_int
#define	atomic_clear_rel_int		kcsan_atomic_clear_rel_int
#define	atomic_cmpset_int		kcsan_atomic_cmpset_int
#define	atomic_cmpset_acq_int		kcsan_atomic_cmpset_acq_int
#define	atomic_cmpset_rel_int		kcsan_atomic_cmpset_rel_int
#define	atomic_fcmpset_int		kcsan_atomic_fcmpset_int
#define	atomic_fcmpset_acq_int		kcsan_atomic_fcmpset_acq_int
#define	atomic_fcmpset_rel_int		kcsan_atomic_fcmpset_rel_int
#define	atomic_fetchadd_int		kcsan_atomic_fetchadd_int
#define	atomic_load_int			kcsan_atomic_load_int
#define	atomic_load_acq_int		kcsan_atomic_load_acq_int
#define	atomic_readandclear_int		kcsan_atomic_readandclear_int
#define	atomic_set_int			kcsan_atomic_set_int
#define	atomic_set_acq_int		kcsan_atomic_set_acq_int
#define	atomic_set_rel_int		kcsan_atomic_set_rel_int
#define	atomic_subtract_int		kcsan_atomic_subtract_int
#define	atomic_subtract_acq_int		kcsan_atomic_subtract_acq_int
#define	atomic_subtract_rel_int		kcsan_atomic_subtract_rel_int
#define	atomic_store_int		kcsan_atomic_store_int
#define	atomic_store_rel_int		kcsan_atomic_store_rel_int
#define	atomic_swap_int			kcsan_atomic_swap_int
#define	atomic_testandclear_int		kcsan_atomic_testandclear_int
#define	atomic_testandset_int		kcsan_atomic_testandset_int

#define	atomic_add_long			kcsan_atomic_add_long
#define	atomic_add_acq_long		kcsan_atomic_add_acq_long
#define	atomic_add_rel_long		kcsan_atomic_add_rel_long
#define	atomic_clear_long		kcsan_atomic_clear_long
#define	atomic_clear_acq_long		kcsan_atomic_clear_acq_long
#define	atomic_clear_rel_long		kcsan_atomic_clear_rel_long
#define	atomic_cmpset_long		kcsan_atomic_cmpset_long
#define	atomic_cmpset_acq_long		kcsan_atomic_cmpset_acq_long
#define	atomic_cmpset_rel_long		kcsan_atomic_cmpset_rel_long
#define	atomic_fcmpset_long		kcsan_atomic_fcmpset_long
#define	atomic_fcmpset_acq_long		kcsan_atomic_fcmpset_acq_long
#define	atomic_fcmpset_rel_long		kcsan_atomic_fcmpset_rel_long
#define	atomic_fetchadd_long		kcsan_atomic_fetchadd_long
#define	atomic_load_long		kcsan_atomic_load_long
#define	atomic_load_acq_long		kcsan_atomic_load_acq_long
#define	atomic_readandclear_long	kcsan_atomic_readandclear_long
#define	atomic_set_long			kcsan_atomic_set_long
#define	atomic_set_acq_long		kcsan_atomic_set_acq_long
#define	atomic_set_rel_long		kcsan_atomic_set_rel_long
#define	atomic_subtract_long		kcsan_atomic_subtract_long
#define	atomic_subtract_acq_long	kcsan_atomic_subtract_acq_long
#define	atomic_subtract_rel_long	kcsan_atomic_subtract_rel_long
#define	atomic_store_long		kcsan_atomic_store_long
#define	atomic_store_rel_long		kcsan_atomic_store_rel_long
#define	atomic_swap_long		kcsan_atomic_swap_long
#define	atomic_testandclear_long	kcsan_atomic_testandclear_long
#define	atomic_testandset_long		kcsan_atomic_testandset_long
#define	atomic_testandset_acq_long	kcsan_atomic_testandset_acq_long

#define	atomic_add_ptr			kcsan_atomic_add_ptr
#define	atomic_add_acq_ptr		kcsan_atomic_add_acq_ptr
#define	atomic_add_rel_ptr		kcsan_atomic_add_rel_ptr
#define	atomic_clear_ptr		kcsan_atomic_clear_ptr
#define	atomic_clear_acq_ptr		kcsan_atomic_clear_acq_ptr
#define	atomic_clear_rel_ptr		kcsan_atomic_clear_rel_ptr
#define	atomic_cmpset_ptr		kcsan_atomic_cmpset_ptr
#define	atomic_cmpset_acq_ptr		kcsan_atomic_cmpset_acq_ptr
#define	atomic_cmpset_rel_ptr		kcsan_atomic_cmpset_rel_ptr
#define	atomic_fcmpset_ptr		kcsan_atomic_fcmpset_ptr
#define	atomic_fcmpset_acq_ptr		kcsan_atomic_fcmpset_acq_ptr
#define	atomic_fcmpset_rel_ptr		kcsan_atomic_fcmpset_rel_ptr
#define	atomic_fetchadd_ptr		kcsan_atomic_fetchadd_ptr
#define	atomic_load_ptr(x)		({					\
	__typeof(*x) __retptr;							\
	__retptr = (void *)kcsan_atomic_load_ptr((volatile uintptr_t *)(x));	\
	__retptr;								\
})
#define	atomic_load_acq_ptr		kcsan_atomic_load_acq_ptr
#define	atomic_load_consume_ptr(x)	({					\
	__typeof(*x) __retptr;							\
	__retptr = (void *)kcsan_atomic_load_acq_ptr((volatile uintptr_t *)(x));\
	__retptr;								\
})
#define	atomic_readandclear_ptr		kcsan_atomic_readandclear_ptr
#define	atomic_set_ptr			kcsan_atomic_set_ptr
#define	atomic_set_acq_ptr		kcsan_atomic_set_acq_ptr
#define	atomic_set_rel_ptr		kcsan_atomic_set_rel_ptr
#define	atomic_subtract_ptr		kcsan_atomic_subtract_ptr
#define	atomic_subtract_acq_ptr		kcsan_atomic_subtract_acq_ptr
#define	atomic_subtract_rel_ptr		kcsan_atomic_subtract_rel_ptr
#define	atomic_store_ptr(x, v)		({					\
	__typeof(*x) __value = (v);						\
	kcsan_atomic_store_ptr((volatile uintptr_t *)(x), (uintptr_t)(__value));\
})
#define	atomic_store_rel_ptr		kcsan_atomic_store_rel_ptr
#define	atomic_swap_ptr			kcsan_atomic_swap_ptr
#define	atomic_testandclear_ptr		kcsan_atomic_testandclear_ptr
#define	atomic_testandset_ptr		kcsan_atomic_testandset_ptr

#define	atomic_add_8			kcsan_atomic_add_8
#define	atomic_add_acq_8		kcsan_atomic_add_acq_8
#define	atomic_add_rel_8		kcsan_atomic_add_rel_8
#define	atomic_clear_8			kcsan_atomic_clear_8
#define	atomic_clear_acq_8		kcsan_atomic_clear_acq_8
#define	atomic_clear_rel_8		kcsan_atomic_clear_rel_8
#define	atomic_cmpset_8			kcsan_atomic_cmpset_8
#define	atomic_cmpset_acq_8		kcsan_atomic_cmpset_acq_8
#define	atomic_cmpset_rel_8		kcsan_atomic_cmpset_rel_8
#define	atomic_fcmpset_8		kcsan_atomic_fcmpset_8
#define	atomic_fcmpset_acq_8		kcsan_atomic_fcmpset_acq_8
#define	atomic_fcmpset_rel_8		kcsan_atomic_fcmpset_rel_8
#define	atomic_fetchadd_8		kcsan_atomic_fetchadd_8
#define	atomic_load_8			kcsan_atomic_load_8
#define	atomic_load_acq_8		kcsan_atomic_load_acq_8
#define	atomic_readandclear_8		kcsan_atomic_readandclear_8
#define	atomic_set_8			kcsan_atomic_set_8
#define	atomic_set_acq_8		kcsan_atomic_set_acq_8
#define	atomic_set_rel_8		kcsan_atomic_set_rel_8
#define	atomic_subtract_8		kcsan_atomic_subtract_8
#define	atomic_subtract_acq_8		kcsan_atomic_subtract_acq_8
#define	atomic_subtract_rel_8		kcsan_atomic_subtract_rel_8
#define	atomic_store_8			kcsan_atomic_store_8
#define	atomic_store_rel_8		kcsan_atomic_store_rel_8
#define	atomic_swap_8			kcsan_atomic_swap_8
#define	atomic_testandclear_8		kcsan_atomic_testandclear_8
#define	atomic_testandset_8		kcsan_atomic_testandset_8

#define	atomic_add_16			kcsan_atomic_add_16
#define	atomic_add_acq_16		kcsan_atomic_add_acq_16
#define	atomic_add_rel_16		kcsan_atomic_add_rel_16
#define	atomic_clear_16			kcsan_atomic_clear_16
#define	atomic_clear_acq_16		kcsan_atomic_clear_acq_16
#define	atomic_clear_rel_16		kcsan_atomic_clear_rel_16
#define	atomic_cmpset_16		kcsan_atomic_cmpset_16
#define	atomic_cmpset_acq_16		kcsan_atomic_cmpset_acq_16
#define	atomic_cmpset_rel_16		kcsan_atomic_cmpset_rel_16
#define	atomic_fcmpset_16		kcsan_atomic_fcmpset_16
#define	atomic_fcmpset_acq_16		kcsan_atomic_fcmpset_acq_16
#define	atomic_fcmpset_rel_16		kcsan_atomic_fcmpset_rel_16
#define	atomic_fetchadd_16		kcsan_atomic_fetchadd_16
#define	atomic_load_16			kcsan_atomic_load_16
#define	atomic_load_acq_16		kcsan_atomic_load_acq_16
#define	atomic_readandclear_16		kcsan_atomic_readandclear_16
#define	atomic_set_16			kcsan_atomic_set_16
#define	atomic_set_acq_16		kcsan_atomic_set_acq_16
#define	atomic_set_rel_16		kcsan_atomic_set_rel_16
#define	atomic_subtract_16		kcsan_atomic_subtract_16
#define	atomic_subtract_acq_16		kcsan_atomic_subtract_acq_16
#define	atomic_subtract_rel_16		kcsan_atomic_subtract_rel_16
#define	atomic_store_16			kcsan_atomic_store_16
#define	atomic_store_rel_16		kcsan_atomic_store_rel_16
#define	atomic_swap_16			kcsan_atomic_swap_16
#define	atomic_testandclear_16		kcsan_atomic_testandclear_16
#define	atomic_testandset_16		kcsan_atomic_testandset_16

#define	atomic_add_32			kcsan_atomic_add_32
#define	atomic_add_acq_32		kcsan_atomic_add_acq_32
#define	atomic_add_rel_32		kcsan_atomic_add_rel_32
#define	atomic_clear_32			kcsan_atomic_clear_32
#define	atomic_clear_acq_32		kcsan_atomic_clear_acq_32
#define	atomic_clear_rel_32		kcsan_atomic_clear_rel_32
#define	atomic_cmpset_32		kcsan_atomic_cmpset_32
#define	atomic_cmpset_acq_32		kcsan_atomic_cmpset_acq_32
#define	atomic_cmpset_rel_32		kcsan_atomic_cmpset_rel_32
#define	atomic_fcmpset_32		kcsan_atomic_fcmpset_32
#define	atomic_fcmpset_acq_32		kcsan_atomic_fcmpset_acq_32
#define	atomic_fcmpset_rel_32		kcsan_atomic_fcmpset_rel_32
#define	atomic_fetchadd_32		kcsan_atomic_fetchadd_32
#define	atomic_load_32			kcsan_atomic_load_32
#define	atomic_load_acq_32		kcsan_atomic_load_acq_32
#define	atomic_readandclear_32		kcsan_atomic_readandclear_32
#define	atomic_set_32			kcsan_atomic_set_32
#define	atomic_set_acq_32		kcsan_atomic_set_acq_32
#define	atomic_set_rel_32		kcsan_atomic_set_rel_32
#define	atomic_subtract_32		kcsan_atomic_subtract_32
#define	atomic_subtract_acq_32		kcsan_atomic_subtract_acq_32
#define	atomic_subtract_rel_32		kcsan_atomic_subtract_rel_32
#define	atomic_store_32			kcsan_atomic_store_32
#define	atomic_store_rel_32		kcsan_atomic_store_rel_32
#define	atomic_swap_32			kcsan_atomic_swap_32
#define	atomic_testandclear_32		kcsan_atomic_testandclear_32
#define	atomic_testandset_32		kcsan_atomic_testandset_32

#define	atomic_add_64			kcsan_atomic_add_64
#define	atomic_add_acq_64		kcsan_atomic_add_acq_64
#define	atomic_add_rel_64		kcsan_atomic_add_rel_64
#define	atomic_clear_64			kcsan_atomic_clear_64
#define	atomic_clear_acq_64		kcsan_atomic_clear_acq_64
#define	atomic_clear_rel_64		kcsan_atomic_clear_rel_64
#define	atomic_cmpset_64		kcsan_atomic_cmpset_64
#define	atomic_cmpset_acq_64		kcsan_atomic_cmpset_acq_64
#define	atomic_cmpset_rel_64		kcsan_atomic_cmpset_rel_64
#define	atomic_fcmpset_64		kcsan_atomic_fcmpset_64
#define	atomic_fcmpset_acq_64		kcsan_atomic_fcmpset_acq_64
#define	atomic_fcmpset_rel_64		kcsan_atomic_fcmpset_rel_64
#define	atomic_fetchadd_64		kcsan_atomic_fetchadd_64
#define	atomic_load_64			kcsan_atomic_load_64
#define	atomic_load_acq_64		kcsan_atomic_load_acq_64
#define	atomic_readandclear_64		kcsan_atomic_readandclear_64
#define	atomic_set_64			kcsan_atomic_set_64
#define	atomic_set_acq_64		kcsan_atomic_set_acq_64
#define	atomic_set_rel_64		kcsan_atomic_set_rel_64
#define	atomic_subtract_64		kcsan_atomic_subtract_64
#define	atomic_subtract_acq_64		kcsan_atomic_subtract_acq_64
#define	atomic_subtract_rel_64		kcsan_atomic_subtract_rel_64
#define	atomic_store_64			kcsan_atomic_store_64
#define	atomic_store_rel_64		kcsan_atomic_store_rel_64
#define	atomic_swap_64			kcsan_atomic_swap_64
#define	atomic_testandclear_64		kcsan_atomic_testandclear_64
#define	atomic_testandset_64		kcsan_atomic_testandset_64

#define	atomic_thread_fence_acq		kcsan_atomic_thread_fence_acq
#define	atomic_thread_fence_acq_rel	kcsan_atomic_thread_fence_acq_rel
#define	atomic_thread_fence_rel		kcsan_atomic_thread_fence_rel
#define	atomic_thread_fence_seq_cst	kcsan_atomic_thread_fence_seq_cst
#define	atomic_interrupt_fence		__compiler_membar

#endif /* !KCSAN_RUNTIME */

#endif /* !_SYS__CSAN_ATOMIC_H_ */

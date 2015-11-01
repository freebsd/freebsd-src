/*-
 * Copyright (c) 2013-2015 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef _MIPS_INCLUDE_CHERIC_H_
#define	_MIPS_INCLUDE_CHERIC_H_

#include <sys/cdefs.h>

#include <machine/cheri.h>

#if !defined(_KERNEL) && __has_feature(capabilities)
/*
 * Programmer-friendly macros for CHERI-aware C code -- requires use of
 * CHERI-aware Clang/LLVM, and full CP2 context switching, so not yet usable
 * in the kernel.
 */
#define	cheri_getlen(x)		__builtin_cheri_get_cap_length(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_getbase(x)	__builtin_cheri_get_cap_base(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_getoffset(x)	__builtin_cheri_cap_offset_get(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_getperm(x)	__builtin_cheri_get_cap_perms(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_getsealed(x)	__builtin_cheri_get_cap_sealed(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_gettag(x)		__builtin_cheri_get_cap_tag(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_gettype(x)	__builtin_cheri_get_cap_type(		\
				    __DECONST(__capability void *, (x)))

#define	cheri_andperm(x, y)	__builtin_cheri_and_cap_perms(		\
				    __DECONST(__capability void *, (x)), (y))
#define	cheri_cleartag(x)	__builtin_cheri_clear_cap_tag(		\
				    __DECONST(__capability void *, (x)))
#define	cheri_incbase(x, y)	__builtin_cheri_inc_cap_base(		\
				    __DECONST(__capability void *, (x)), (y))
#define	cheri_incoffset(x, y)	__builtin_cheri_cap_offset_increment(	\
				    __DECONST(__capability void *, (x)), (y))
#define	cheri_setlen(x, y)	__builtin_cheri_set_cap_length(		\
				    __DECONST(__capability void *, (x)), (y))
#define	cheri_setoffset(x, y)	__builtin_cheri_cap_offset_set(		\
				    __DECONST(__capability void *, (x)), (y))

#define	cheri_seal(x, y)	__builtin_cheri_seal_cap(		 \
				    __DECONST(__capability void *, (x)), \
				    __DECONST(__capability void *, (y)))
#define	cheri_unseal(x, y)	__builtin_cheri_unseal_cap(		 \
				    __DECONST(__capability void *, (x)), \
				    __DECONST(__capability void *, (y)))

#define	cheri_getcause()	__builtin_cheri_get_cause()
#define	cheri_setcause(x)	__builtin_cheri_set_cause(x)

#define	cheri_ccheckperm(c, p)	__builtin_cheri_check_perms(		\
				    __DECONST(__capability void *, (c)), (p))
#define	cheri_cchecktype(c, t)	__builtin_cheri_check_type(		\
				    __DECONST(__capability void *, (c)), (t))

#define	cheri_getdefault()	__builtin_cheri_get_global_data_cap()
#define	cheri_getidc()		__builtin_cheri_get_invoke_data_cap()
#define	cheri_getkr0c()		__builtin_cheri_get_kernel_cap1()
#define	cheri_getkr1c()		__builtin_cheri_get_kernel_cap2()
#define	cheri_getkcc()		__builtin_cheri_get_kernel_code_cap()
#define	cheri_getkdc()		__builtin_cheri_get_kernel_data_cap()
#define	cheri_getepcc()		__builtin_cheri_get_exception_program_counter_cap()
#define	cheri_getpcc()		__builtin_cheri_get_program_counter_cap()
#define	cheri_getstack()	__builtin_memcap_stack_get()

#define	cheri_local(c)		cheri_andperm((c), ~CHERI_PERM_GLOBAL)

#define	cheri_csetbounds(x, y)	__builtin_memcap_bounds_set(		\
				    __DECONST(__capability void *, (x)), (y))
static __inline __capability void *
cheri_ptr(const void *ptr, size_t len)
{
	const __capability void *c;

	/* Assume CFromPtr without base set, availability of CSetBounds. */
	c = (const __capability void *)ptr;
	return (cheri_csetbounds(c, len));
}

static __inline __capability void *
cheri_ptrperm(const void *ptr, size_t len, register_t perm)
{

	return (cheri_andperm(cheri_ptr(ptr, len), perm | CHERI_PERM_GLOBAL));
}

static __inline __capability void *
cheri_ptrpermoff(const void *ptr, size_t len, register_t perm, off_t off)
{

	return (cheri_setoffset(cheri_ptrperm(ptr, len, perm), off));
}

/*
 * Construct a capability suitable to describe a type identified by 'ptr';
 * set it to zero-length with the offset equal to the base.
 */
static __inline __capability void *
cheri_maketype(const void *ptr, register_t perm)
{

	return (cheri_ptrpermoff(ptr, 1, perm, 0));
}

static __inline __capability void *
cheri_zerocap(void)
{
	return (__capability void*)0;
}

#define	cheri_getreg(x) ({						\
	__capability void *_cap;					\
	__asm __volatile ("cmove %0, $c" #x : "=C" (_cap));		\
	_cap;								\
})

#define	cheri_setreg(x, cap) do {					\
	if ((x) == 0)							\
		__asm __volatile ("cmove $c" #x ", %0" : : "C" (cap) :	\
		    "memory");						\
	else								\
		__asm __volatile ("cmove $c" #x ", %0" : : "C" (cap));  \
} while (0)
#endif

#define CHERI_PRINT_PTR(ptr)						\
	printf("%s: " #ptr " b:%016jx l:%016zx o:%jx\n", __func__,	\
	   cheri_getbase((const __capability void *)(ptr)),		\
	   cheri_getlen((const __capability void *)(ptr)),		\
	   cheri_getoffset((const __capability void *)(ptr)))

#endif /* _MIPS_INCLUDE_CHERIC_H_ */

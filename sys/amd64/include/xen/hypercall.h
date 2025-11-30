/******************************************************************************
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * hypercall.h
 *
 * FreeBSD-specific hypervisor handling.
 *
 * Copyright (c) 2002-2004, K A Fraser
 *
 * 64-bit updates:
 *   Benjamin Liu <benjamin.liu@intel.com>
 *   Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __MACHINE_XEN_HYPERCALL_H__
#define __MACHINE_XEN_HYPERCALL_H__

#ifndef __XEN_HYPERVISOR_H__
# error "please don't include this file directly"
#endif

extern char *hypercall_page;

#define __STR(x) #x
#define STR(x) __STR(x)
#define __must_check

#define HYPERCALL_STR(name)					\
	"call hypercall_page + ("STR(__HYPERVISOR_##name)" * 32)"

#define _hypercall0(type, name)					\
({								\
	type __res;						\
	__asm__ volatile (					\
		HYPERCALL_STR(name)				\
		: "=a" (__res)					\
		:						\
		: "memory" );					\
	__res;							\
})

#define _hypercall1(type, name, a1)				\
({								\
	type __res;						\
	long __ign1;						\
	__asm__ volatile (					\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1)			\
		: "1" ((long)(a1))				\
		: "memory" );					\
	__res;							\
})

#define _hypercall2(type, name, a1, a2)				\
({								\
	type __res;						\
	long __ign1, __ign2;					\
	__asm__ volatile (					\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2)	\
		: "1" ((long)(a1)), "2" ((long)(a2))		\
		: "memory" );					\
	__res;							\
})

#define _hypercall3(type, name, a1, a2, a3)			\
({								\
	type __res;						\
	long __ign1, __ign2, __ign3;				\
	__asm__ volatile (					\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2),	\
		"=d" (__ign3)					\
		: "1" ((long)(a1)), "2" ((long)(a2)),		\
		"3" ((long)(a3))				\
		: "memory" );					\
	__res;							\
})

#define _hypercall4(type, name, a1, a2, a3, a4)			\
({								\
	type __res;						\
	long __ign1, __ign2, __ign3;				\
	register long __arg4 __asm__("r10") = (long)(a4);	\
	__asm__ volatile (					\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2),	\
		  "=d" (__ign3), "+r" (__arg4)			\
		: "1" ((long)(a1)), "2" ((long)(a2)),		\
		  "3" ((long)(a3))				\
		: "memory" );					\
	__res;							\
})

#define _hypercall5(type, name, a1, a2, a3, a4, a5)		\
({								\
	type __res;						\
	long __ign1, __ign2, __ign3;				\
	register long __arg4 __asm__("r10") = (long)(a4);	\
	register long __arg5 __asm__("r8") = (long)(a5);	\
	__asm__ volatile (					\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2),	\
		  "=d" (__ign3), "+r" (__arg4), "+r" (__arg5)	\
		: "1" ((long)(a1)), "2" ((long)(a2)),		\
		  "3" ((long)(a3))				\
		: "memory" );					\
	__res;							\
})

static inline int
privcmd_hypercall(long op, long a1, long a2, long a3, long a4, long a5)
{
	int __res;
	long __ign1, __ign2, __ign3;
	register long __arg4 __asm__("r10") = (long)(a4);
	register long __arg5 __asm__("r8") = (long)(a5);
	long __call = (long)&hypercall_page + (op * 32);

	if (op >= PAGE_SIZE / 32)
		return -EINVAL;

	__asm__ volatile (
		"call *%[call]"
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2),
		  "=d" (__ign3), "+r" (__arg4), "+r" (__arg5)
		: "1" ((long)(a1)), "2" ((long)(a2)),
		  "3" ((long)(a3)), [call] "a" (__call)
		: "memory" );

	return (__res);
}

static inline int __must_check
HYPERVISOR_set_trap_table(
	const trap_info_t *table)
{
	return _hypercall1(int, set_trap_table, table);
}

#undef __must_check

#include <xen/hypercall.h>

#endif /* __MACHINE_XEN_HYPERCALL_H__ */

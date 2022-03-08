/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2022 Elliott Mitchell
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

#ifndef __MACHINE_XEN_HYPERCALL_H__
#define __MACHINE_XEN_HYPERCALL_H__

#ifndef __XEN_HYPERVISOR_H__
# error "please don't include this file directly"
#endif

/*
 * See the Xen contrib header: xen/arch-arm.h for details on Xen's
 * hypercall calling conventions.
 *
 * The hypercall number is passed in r12/x16.
 *
 * Input parameters are in r0-r4/x0-x4 as appropriate to the number of
 * arguments.  Input registers are clobbered.
 *
 * Return is in r0/x0.
 *
 * The hypercall tag for Xen is 0x0EA1.
 */

#define	hypercallf(NUM, ARGS, REGVAR, REGASM)	\
	static inline register_t					\
	___xen_hypercall_##NUM(ARGS register_t op)			\
	{								\
		register register_t _op __asm__(OPREG) = op;		\
		REGVAR							\
		__asm__ volatile (					\
			"hvc #0x0EA1;\n"				\
			: "+r" (_op)REGASM				\
			: /* clobbered inputs, are outputs, really */	\
			: "memory"					\
		);							\
		return (_arg0);						\
	}

#ifndef __ILP32__
#define	OPREG	"x16"
#define	REGPRE	"x"
#else
#define	OPREG	"r12"
#define	REGPRE	"r"
#endif

#define	COMMAS(...)	__VA_ARGS__
#define	ARG(n)	register_t arg##n,
#define	VAR(n)	register register_t _arg##n __asm__(REGPRE __STRING(n)) = arg##n;
#define	REG(n)	, "+r" (_arg##n)


#define	hypercall0(NUM, ARGS, REGVAR, REGASM)	\
	hypercallf(NUM,, register register_t _arg0 __asm__(REGPRE"0");,	\
		COMMAS(, "=r" (_arg0)))

#define	hypercall1(NUM, ARGS, REGVAR, REGASM)	\
	hypercallf(NUM, COMMAS(ARG(0)ARGS), VAR(0)REGVAR, COMMAS(REG(0)REGASM))

#define	hypercall2(NUM, ARGS, REGVAR, REGASM)	\
	hypercall1(NUM, COMMAS(ARG(1)ARGS), VAR(1)REGVAR, COMMAS(REG(1)REGASM))

#define	hypercall3(NUM, ARGS, REGVAR, REGASM)	\
	hypercall2(NUM, COMMAS(ARG(2)ARGS), VAR(2)REGVAR, COMMAS(REG(2)REGASM))

#define	hypercall4(NUM, ARGS, REGVAR, REGASM)	\
	hypercall3(NUM, COMMAS(ARG(3)ARGS), VAR(3)REGVAR, COMMAS(REG(3)REGASM))

#define	hypercall5(NUM, ARGS, REGVAR, REGASM)	\
	hypercall4(NUM, COMMAS(ARG(4)ARGS), VAR(4)REGVAR, COMMAS(REG(4)REGASM))

#define	hypercall(NUM)	hypercall##NUM(NUM,,,)

/* the actual inline function definitions */

hypercall(0)
hypercall(1)
hypercall(2)
hypercall(3)
hypercall(4)
hypercall(5)

/* cleanup */

#undef	hypercallf
#undef	OPREG
#undef	REGPRE
#undef	COMMAS
#undef	ARG
#undef	VAR
#undef	REG

#undef	hypercall0
#undef	hypercall1
#undef	hypercall2
#undef	hypercall3
#undef	hypercall4
#undef	hypercall

/* the wrappers expected by hypercall.h */

/*
 * The reasoning behind this is Xen/ARM wants the first argument in the first
 * argument passing register, and the op code in a later register.  As such the
 * preprocessor is used to swap arguments with the goal of reducing argument
 * shuffling in the resultant binary.
 */

#define __xen_hypercall_0(op) ___xen_hypercall_0(op)
#define __xen_hypercall_1(op,a0) ___xen_hypercall_1(a0,op)
#define __xen_hypercall_2(op,a0,a1) ___xen_hypercall_2(a0,a1,op)
#define __xen_hypercall_3(op,a0,a1,a2) ___xen_hypercall_3(a0,a1,a2,op)
#define __xen_hypercall_4(op,a0,a1,a2,a3) ___xen_hypercall_4(a0,a1,a2,a3,op)
#define __xen_hypercall_5(op,a0,a1,a2,a3,a4) ___xen_hypercall_5(a0,a1,a2,a3,a4,op)

/* the wrappers presently expected by hypercall.h */

#define	_hypercall0(type, name)	\
	(type)__xen_hypercall_0(__HYPERVISOR_##name)
#define	_hypercall1(type, name, arg0)	\
	(type)__xen_hypercall_1(__HYPERVISOR_##name, (register_t)(arg0))
#define	_hypercall2(type, name, arg0, arg1)	\
	(type)__xen_hypercall_2(__HYPERVISOR_##name, (register_t)(arg0), \
		(register_t)(arg1))
#define	_hypercall3(type, name, arg0, arg1, arg2)	\
	(type)__xen_hypercall_3(__HYPERVISOR_##name, (register_t)(arg0), \
		(register_t)(arg1), (register_t)(arg2))
#define	_hypercall4(type, name, arg0, arg1, arg2, arg3)	\
	(type)__xen_hypercall_4(__HYPERVISOR_##name, (register_t)(arg0), \
		(register_t)(arg1), (register_t)(arg2), (register_t)(arg3))
#define	_hypercall5(type, name, arg0, arg1, arg2, arg3, arg4)	\
	(type)__xen_hypercall_5(__HYPERVISOR_##name, (register_t)(arg0), \
		(register_t)(arg1), (register_t)(arg2), (register_t)(arg3), \
		(register_t)(arg4))

#define	privcmd_hypercall(op, arg0, arg1, arg2, arg3, arg4)	\
	(int)__xen_hypercall_5((op), (arg0), (arg1), (arg2), (arg3), (arg4))

#include <xen/hypercall.h>

#endif /* __MACHINE_XEN_HYPERCALL_H__ */

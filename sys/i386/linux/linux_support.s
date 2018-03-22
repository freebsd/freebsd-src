/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006,2007 Konstantin Belousov
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
 * $FreeBSD$
 */

#include "linux_assym.h"		/* system definitions */
#include <machine/asmacros.h>		/* miscellaneous asm macros */

#include "assym.inc"

futex_fault_decx:
	movl	PCPU(CURPCB),%ecx
futex_fault:
	movl	$0,PCB_ONFAULT(%ecx)
	movl	$-EFAULT,%eax
	ret

ENTRY(futex_xchgl)
	movl	PCPU(CURPCB),%ecx
	movl	$futex_fault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%eax
	movl	8(%esp),%edx
	cmpl    $VM_MAXUSER_ADDRESS-4,%edx
	ja	futex_fault
	xchgl	%eax,(%edx)
	movl	12(%esp),%edx
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(futex_addl)
	movl	PCPU(CURPCB),%ecx
	movl	$futex_fault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%eax
	movl	8(%esp),%edx
	cmpl    $VM_MAXUSER_ADDRESS-4,%edx
	ja	futex_fault
#ifdef SMP
	lock
#endif
	xaddl	%eax,(%edx)
	movl	12(%esp),%edx
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(futex_orl)
	movl	PCPU(CURPCB),%ecx
	movl	$futex_fault_decx,PCB_ONFAULT(%ecx)
	movl	8(%esp),%edx
	cmpl    $VM_MAXUSER_ADDRESS-4,%edx
	ja	futex_fault
	movl	(%edx),%eax
1:	movl	%eax,%ecx
	orl	4(%esp),%ecx
#ifdef SMP
	lock
#endif
	cmpxchgl %ecx,(%edx)
	jnz	1b
futex_tail:
	movl	12(%esp),%edx
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	PCPU(CURPCB),%ecx
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(futex_andl)
	movl	PCPU(CURPCB),%ecx
	movl	$futex_fault_decx,PCB_ONFAULT(%ecx)
	movl	8(%esp),%edx
	cmpl    $VM_MAXUSER_ADDRESS-4,%edx
	ja	futex_fault
	movl	(%edx),%eax
1:	movl	%eax,%ecx
	andl	4(%esp),%ecx
#ifdef SMP
	lock
#endif
	cmpxchgl %ecx,(%edx)
	jnz	1b
	jmp	futex_tail

ENTRY(futex_xorl)
	movl	PCPU(CURPCB),%ecx
	movl	$futex_fault_decx,PCB_ONFAULT(%ecx)
	movl	8(%esp),%edx
	cmpl    $VM_MAXUSER_ADDRESS-4,%edx
	ja	futex_fault
	movl	(%edx),%eax
1:	movl	%eax,%ecx
	xorl	4(%esp),%ecx
#ifdef SMP
	lock
#endif
	cmpxchgl %ecx,(%edx)
	jnz	1b
	jmp	futex_tail

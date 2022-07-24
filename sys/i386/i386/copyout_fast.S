/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.inc"

/*
 * Fast path for copyout code.  We switch to user space %cr3 and perform
 * move operation between user memory and copyout buffer, located in the
 * trampoline area.  We must switch to trampoline stack, because both
 * user and kernel buffer accesses might cause page fault.
 *
 * Page fault handler expects %edx to point to the onfault routine.
 * Handler switches to idlePTD and calls the routine.
 * The routine must restore the stack, enable interrupts, and
 * return to the caller, informing it about failure.
 */
	.text

ENTRY(copyout_fast)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	20(%ebp),%ebx		/* KCR3 */
	/* bcopy(%esi = kaddr, %edi = PCPU(copyout_buf), %ecx = len) */
	movl	16(%ebp),%ecx
	movl	8(%ebp),%esi
	movl	%esp,%eax
	movl	$copyout_fault,%edx

	cli
	movl	PCPU(COPYOUT_BUF),%edi
pf_y1:	rep; movsb

	movl	16(%ebp),%ecx		/* len */
	movl	PCPU(COPYOUT_BUF),%esi	/* kaddr */
	movl	12(%ebp),%edi		/* uaddr */
	movl	PCPU(TRAMPSTK),%esp
	movl	PCPU(CURPCB),%edx
	movl	PCB_CR3(%edx),%edx	/* UCR3 */
	movl	%edx,%cr3
	movl	$copyout_fault,%edx
	/* bcopy(%esi = PCPU(copyout_buf), %edi = udaddr, %ecx = len) */
pf_x1:	rep; movsb

	movl	%ebx,%cr3
	movl	%eax,%esp
	sti
	xorl	%eax,%eax
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret
END(copyout_fast)

ENTRY(copyin_fast)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	movl	20(%ebp),%ebx		/* KCR3 */
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%edx	/* UCR3 */
	movl	16(%ebp),%ecx		/* len */
	movl	8(%ebp),%esi		/* udaddr */
	movl	%esp,%eax

	cli
	movl	PCPU(COPYOUT_BUF),%edi	/* kaddr */
	movl	PCPU(TRAMPSTK),%esp
	movl	%edx,%cr3
	movl	$copyout_fault,%edx
	/* bcopy(%esi = udaddr, %edi = PCPU(copyout_buf), %ecx = len) */
pf_x2:	rep; movsb

	movl	%ebx,%cr3
	movl	%eax,%esp

	/* bcopy(%esi = PCPU(copyout_buf), %edi = kaddr, %ecx = len) */
	movl	16(%ebp),%ecx
	movl	12(%ebp),%edi
	movl	PCPU(COPYOUT_BUF),%esi
pf_y2:	rep; movsb

	sti
	xorl	%eax,%eax
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret
END(copyin_fast)

	ALIGN_TEXT
copyout_fault:
	movl	%eax,%esp
	sti
	movl	$EFAULT,%eax
	popl	%ebx
	popl	%edi
	popl	%esi
	leave
	ret

ENTRY(fueword_fast)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%ecx			/* from */
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%eax
	movl	$fusufault,%edx
	movl	16(%ebp),%ebx
	movl	%esp,%esi
	cli
	movl	PCPU(TRAMPSTK),%esp
	movl	%eax,%cr3
pf_x3:	movl	(%ecx),%eax
	movl	%ebx,%cr3
	movl	%esi,%esp
	sti
	movl	12(%ebp),%edx
	movl	%eax,(%edx)
	xorl	%eax,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret
END(fueword_fast)

ENTRY(fuword16_fast)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%ecx			/* from */
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%eax
	movl	$fusufault,%edx
	movl	12(%ebp),%ebx
	movl	%esp,%esi
	cli
	movl	PCPU(TRAMPSTK),%esp
	movl	%eax,%cr3
pf_x4:	movzwl	(%ecx),%eax
	movl	%ebx,%cr3
	movl	%esi,%esp
	sti
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret
END(fuword16_fast)

ENTRY(fubyte_fast)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%ecx			/* from */
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%eax
	movl	$fusufault,%edx
	movl	12(%ebp),%ebx
	movl	%esp,%esi
	cli
	movl	PCPU(TRAMPSTK),%esp
	movl	%eax,%cr3
pf_x5:	movzbl	(%ecx),%eax
	movl	%ebx,%cr3
	movl	%esi,%esp
	sti
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret
END(fubyte_fast)

	ALIGN_TEXT
fusufault:
	movl	%esi,%esp
	sti
	xorl	%eax,%eax
	decl	%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret

ENTRY(suword_fast)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%eax
	movl	$fusufault,%edx
	movl	8(%ebp),%ecx			/* to */
	movl	12(%ebp),%edi			/* val */
	movl	16(%ebp),%ebx
	movl	%esp,%esi
	cli
	movl	PCPU(TRAMPSTK),%esp
	movl	%eax,%cr3
pf_x6:	movl	%edi,(%ecx)
	movl	%ebx,%cr3
	movl	%esi,%esp
	sti
	xorl	%eax,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret
END(suword_fast)

ENTRY(suword16_fast)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%eax
	movl	$fusufault,%edx
	movl	8(%ebp),%ecx			/* to */
	movl	12(%ebp),%edi			/* val */
	movl	16(%ebp),%ebx
	movl	%esp,%esi
	cli
	movl	PCPU(TRAMPSTK),%esp
	movl	%eax,%cr3
pf_x7:	movw	%di,(%ecx)
	movl	%ebx,%cr3
	movl	%esi,%esp
	sti
	xorl	%eax,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret
END(suword16_fast)

ENTRY(subyte_fast)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	PCPU(CURPCB),%eax
	movl	PCB_CR3(%eax),%eax
	movl	$fusufault,%edx
	movl	8(%ebp),%ecx			/* to */
	movl	12(%ebp),%edi			/* val */
	movl	16(%ebp),%ebx
	movl	%esp,%esi
	cli
	movl	PCPU(TRAMPSTK),%esp
	movl	%eax,%cr3
	movl	%edi,%eax
pf_x8:	movb	%al,(%ecx)
	movl	%ebx,%cr3
	movl	%esi,%esp
	sti
	xorl	%eax,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	leave
	ret
END(subyte_fast)

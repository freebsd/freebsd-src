/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2008-2009 Jung-uk Kim <jkim@FreeBSD.org>
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

#include <machine/asmacros.h>
#include <machine/specialreg.h>

#include "acpi_wakedata.h"
#include "assym.s"

#define	WAKEUP_CTX(member)	wakeup_ ## member - wakeup_ctx(%rdi)
#define	WAKEUP_PCB(member)	PCB_ ## member(%r11)
#define	WAKEUP_XPCB(member)	XPCB_ ## member(%r11)

ENTRY(acpi_restorecpu)
	/* Switch to KPML4phys. */
	movq	%rsi, %rax
	movq	%rax, %cr3

	/* Restore GDT. */
	lgdt	WAKEUP_CTX(gdt)
	jmp	1f
1:

	/* Fetch PCB. */
	movq	WAKEUP_CTX(xpcb), %r11

	/* Force kernel segment registers. */
	movl	$KDSEL, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movl	$KUF32SEL, %eax
	movw	%ax, %fs
	movl	$KUG32SEL, %eax
	movw	%ax, %gs

	movl	$MSR_FSBASE, %ecx
	movl	WAKEUP_PCB(FSBASE), %eax
	movl	4 + WAKEUP_PCB(FSBASE), %edx
	wrmsr
	movl	$MSR_GSBASE, %ecx
	movl	WAKEUP_PCB(GSBASE), %eax
	movl	4 + WAKEUP_PCB(GSBASE), %edx
	wrmsr
	movl	$MSR_KGSBASE, %ecx
	movl	WAKEUP_XPCB(KGSBASE), %eax
	movl	4 + WAKEUP_XPCB(KGSBASE), %edx
	wrmsr

	/* Restore EFER. */
	movl	$MSR_EFER, %ecx
	movl	WAKEUP_CTX(efer), %eax
	wrmsr

	/* Restore PAT. */
	movl	$MSR_PAT, %ecx
	movl	WAKEUP_CTX(pat), %eax
	movl	4 + WAKEUP_CTX(pat), %edx
	wrmsr

	/* Restore fast syscall stuff. */
	movl	$MSR_STAR, %ecx
	movl	WAKEUP_CTX(star), %eax
	movl	4 + WAKEUP_CTX(star), %edx
	wrmsr
	movl	$MSR_LSTAR, %ecx
	movl	WAKEUP_CTX(lstar), %eax
	movl	4 + WAKEUP_CTX(lstar), %edx
	wrmsr
	movl	$MSR_CSTAR, %ecx
	movl	WAKEUP_CTX(cstar), %eax
	movl	4 + WAKEUP_CTX(cstar), %edx
	wrmsr
	movl	$MSR_SF_MASK, %ecx
	movl	WAKEUP_CTX(sfmask), %eax
	wrmsr

	/* Restore CR0, CR2 and CR4. */
	movq	WAKEUP_XPCB(CR0), %rax
	movq	%rax, %cr0
	movq	WAKEUP_XPCB(CR2), %rax
	movq	%rax, %cr2
	movq	WAKEUP_XPCB(CR4), %rax
	movq	%rax, %cr4

	/* Restore descriptor tables. */
	lidt	WAKEUP_XPCB(IDT)
	lldt	WAKEUP_XPCB(LDT)

#define	SDT_SYSTSS	9
#define	SDT_SYSBSY	11

	/* Clear "task busy" bit and reload TR. */
	movq	PCPU(TSS), %rax
	andb	$(~SDT_SYSBSY | SDT_SYSTSS), 5(%rax)
	movw	WAKEUP_XPCB(TR), %ax
	ltr	%ax

#undef	SDT_SYSTSS
#undef	SDT_SYSBSY

	/* Restore other callee saved registers. */
	movq	WAKEUP_PCB(R15), %r15
	movq	WAKEUP_PCB(R14), %r14
	movq	WAKEUP_PCB(R13), %r13
	movq	WAKEUP_PCB(R12), %r12
	movq	WAKEUP_PCB(RBP), %rbp
	movq	WAKEUP_PCB(RSP), %rsp
	movq	WAKEUP_PCB(RBX), %rbx

	/* Restore debug registers. */
	movq	WAKEUP_PCB(DR0), %rax
	movq	%rax, %dr0
	movq	WAKEUP_PCB(DR1), %rax
	movq	%rax, %dr1
	movq	WAKEUP_PCB(DR2), %rax
	movq	%rax, %dr2
	movq	WAKEUP_PCB(DR3), %rax
	movq	%rax, %dr3
	movq	WAKEUP_PCB(DR6), %rax
	movq	%rax, %dr6
	movq	WAKEUP_PCB(DR7), %rax
	movq	%rax, %dr7

	/* Restore return address. */
	movq	WAKEUP_PCB(RIP), %rax
	movq	%rax, (%rsp)

	/* Indicate the CPU is resumed. */
	xorl	%eax, %eax
	movl	%eax, WAKEUP_CTX(cpu)

	ret
END(acpi_restorecpu)

ENTRY(acpi_savecpu)
	/* Fetch XPCB and save CPU context. */
	movq	%rdi, %r10
	call	savectx2
	movq	%r10, %r11

	/* Patch caller's return address and stack pointer. */
	movq	(%rsp), %rax
	movq	%rax, WAKEUP_PCB(RIP)
	movq	%rsp, %rax
	movq	%rax, WAKEUP_PCB(RSP)

	movl	$1, %eax
	ret
END(acpi_savecpu)

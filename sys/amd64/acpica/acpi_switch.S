/*-
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2008-2010 Jung-uk Kim <jkim@FreeBSD.org>
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

ENTRY(acpi_restorecpu)
	/* Switch to KPML4phys. */
	movq	%rsi, %rax
	movq	%rax, %cr3

	/* Restore GDT. */
	lgdt	WAKEUP_CTX(gdt)
	jmp	1f
1:

	/* Fetch PCB. */
	movq	WAKEUP_CTX(pcb), %rsi

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
	movl	PCB_FSBASE(%rsi), %eax
	movl	4 + PCB_FSBASE(%rsi), %edx
	wrmsr
	movl	$MSR_GSBASE, %ecx
	movl	PCB_GSBASE(%rsi), %eax
	movl	4 + PCB_GSBASE(%rsi), %edx
	wrmsr
	movl	$MSR_KGSBASE, %ecx
	movl	PCB_KGSBASE(%rsi), %eax
	movl	4 + PCB_KGSBASE(%rsi), %edx
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

	/* Restore CR0 except for FPU mode. */
	movq	PCB_CR0(%rsi), %rax
	movq	%rax, %rcx
	andq	$~(CR0_EM | CR0_TS), %rax
	movq	%rax, %cr0

	/* Restore CR2 and CR4. */
	movq	PCB_CR2(%rsi), %rax
	movq	%rax, %cr2
	movq	PCB_CR4(%rsi), %rax
	movq	%rax, %cr4

	/* Restore descriptor tables. */
	lidt	PCB_IDT(%rsi)
	lldt	PCB_LDT(%rsi)

#define	SDT_SYSTSS	9
#define	SDT_SYSBSY	11

	/* Clear "task busy" bit and reload TR. */
	movq	PCPU(TSS), %rax
	andb	$(~SDT_SYSBSY | SDT_SYSTSS), 5(%rax)
	movw	PCB_TR(%rsi), %ax
	ltr	%ax

#undef	SDT_SYSTSS
#undef	SDT_SYSBSY

	/* Restore other callee saved registers. */
	movq	PCB_R15(%rsi), %r15
	movq	PCB_R14(%rsi), %r14
	movq	PCB_R13(%rsi), %r13
	movq	PCB_R12(%rsi), %r12
	movq	PCB_RBP(%rsi), %rbp
	movq	PCB_RSP(%rsi), %rsp
	movq	PCB_RBX(%rsi), %rbx

	/* Restore debug registers. */
	movq	PCB_DR0(%rsi), %rax
	movq	%rax, %dr0
	movq	PCB_DR1(%rsi), %rax
	movq	%rax, %dr1
	movq	PCB_DR2(%rsi), %rax
	movq	%rax, %dr2
	movq	PCB_DR3(%rsi), %rax
	movq	%rax, %dr3
	movq	PCB_DR6(%rsi), %rax
	movq	%rax, %dr6
	movq	PCB_DR7(%rsi), %rax
	movq	%rax, %dr7

#define	__INITIAL_FPUCW__	0x037f
#define	__INITIAL_MXCSR__	0x1f80

	/* Initialize FPU and restore state if necessary. */
	fninit
	movw	$__INITIAL_FPUCW__, -2(%rsp)
	fldcw	-2(%rsp)
	movl	$__INITIAL_MXCSR__, -4(%rsp)
	ldmxcsr	-4(%rsp)
	movq	PCPU(FPCURTHREAD), %rax
	testq	%rax, %rax
	je	1f
	fxrstor	PCB_USERFPU(%rsi)
1:

#undef	__INITIAL_FPUCW__
#undef	__INITIAL_MXCSR__

	/* Reload CR0. */
	movq	%rcx, %cr0

	/* Restore return address. */
	movq	PCB_RIP(%rsi), %rax
	movq	%rax, (%rsp)

	/* Indicate the CPU is resumed. */
	xorl	%eax, %eax
	movl	%eax, WAKEUP_CTX(cpu)

	ret
END(acpi_restorecpu)

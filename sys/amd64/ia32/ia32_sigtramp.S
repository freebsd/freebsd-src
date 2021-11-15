/*-
 * Copyright (c) 2003 Peter Wemm
 * All rights reserved.
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Konstantin Belousov
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
#include <sys/syscall.h>

#include "ia32_assym.h"

	.text
/*
 * Signal trampoline, mapped as vdso into shared page, or copied to
 * top of user stack for old binaries.
 */
	ALIGN_TEXT
	.globl	__vdso_ia32_sigcode
__vdso_ia32_sigcode:
	.cfi_startproc
	.cfi_signal_frame
	.cfi_def_cfa	%esp, 0
#if 0
	.cfi_offset	%gs,  IA32_SIGF_UC + IA32_UC_GS
	.cfi_offset	%fs,  IA32_SIGF_UC + IA32_UC_FS
	.cfi_offset	%es,  IA32_SIGF_UC + IA32_UC_ES
	.cfi_offset	%ds,  IA32_SIGF_UC + IA32_UC_DS
#endif
	.cfi_offset	%edi, IA32_SIGF_UC + IA32_UC_EDI
	.cfi_offset	%esi, IA32_SIGF_UC + IA32_UC_ESI
	.cfi_offset	%ebp, IA32_SIGF_UC + IA32_UC_EBP
	.cfi_offset	%ebx, IA32_SIGF_UC + IA32_UC_EBX
	.cfi_offset	%edx, IA32_SIGF_UC + IA32_UC_EDX
	.cfi_offset	%ecx, IA32_SIGF_UC + IA32_UC_ECX
	.cfi_offset	%eax, IA32_SIGF_UC + IA32_UC_EAX
	.cfi_offset	%eip, IA32_SIGF_UC + IA32_UC_EIP
#if 0
	.cfi_offset	%cs,  IA32_SIGF_UC + IA32_UC_CS
	.cfi_offset	%flags, IA32_SIGF_UC + IA32_UC_EFLAGS
#endif
	.cfi_offset	%esp, IA32_SIGF_UC + IA32_UC_ESP
#if 0
	.cfi_offset	%ss,  IA32_SIGF_UC + IA32_UC_SS
	.cfi_offset	93 /* %fs.base */, IA32_SIGF_UC + IA32_UC_FSBASE
	.cfi_offset	94 /* %gs.base */, IA32_SIGF_UC + IA32_UC_GSBASE
#endif
	calll	*IA32_SIGF_HANDLER(%esp)
	leal	IA32_SIGF_UC(%esp),%eax	/* get ucontext */
	pushl	%eax
	.cfi_def_cfa	%esp, 4
	movl	$SYS_sigreturn,%eax
	pushl	%eax			/* junk to fake return addr. */
	.cfi_def_cfa	%esp, 8
	int	$0x80			/* enter kernel with args */
					/* on stack */
1:
	jmp	1b
	.cfi_endproc

#ifdef COMPAT_FREEBSD4
	ALIGN_TEXT
	.globl	__vdso_freebsd4_ia32_sigcode
__vdso_freebsd4_ia32_sigcode:
	calll	*IA32_SIGF_HANDLER(%esp)
	leal	IA32_SIGF_UC4(%esp),%eax/* get ucontext */
	pushl	%eax
	movl	$344,%eax		/* 4.x SYS_sigreturn */
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
					/* on stack */
1:
	jmp	1b
#endif

#ifdef COMPAT_43
	ALIGN_TEXT
	.globl	__vdso_ia32_osigcode
__vdso_ia32_osigcode:
	calll	*IA32_SIGF_HANDLER(%esp)/* call signal handler */
	leal	IA32_SIGF_SC(%esp),%eax	/* get sigcontext */
	pushl	%eax
	movl	$103,%eax		/* 3.x SYS_sigreturn */
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
1:
	jmp	1b

/*
 * Our lcall $7,$0 handler remains in user mode (ring 3), since lcalls
 * don't change the interrupt mask, so if this one went directly to the
 * kernel then there would be a window with interrupts enabled in kernel
 * mode, and all interrupt handlers would have to be almost as complicated
 * as the NMI handler to support this.
 *
 * Instead, convert the lcall to an int0x80 call.  The kernel does most
 * of the conversion by popping the lcall return values off the user
 * stack and returning to them instead of to here, except when the
 * conversion itself fails.  Adjusting the stack here is impossible for
 * vfork() and harder for other syscalls.
 */
	ALIGN_TEXT
	.globl	__vdso_lcall_tramp
__vdso_lcall_tramp:
	int	$0x80
1:	jmp	1b
#endif
	.p2align 1

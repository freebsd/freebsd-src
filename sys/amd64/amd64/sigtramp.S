/*-
 * Copyright (c) 2003 Peter Wemm <peter@freeBSD.org>
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

#include <sys/syscall.h>
#include <machine/asmacros.h>

#include "assym.inc"

	.text
/*
 * Signal trampoline, mapped as vdso into shared page.
 */
ENTRY(__vdso_sigcode)
	.cfi_startproc
	.cfi_signal_frame
	.cfi_def_cfa	%rsp, 0
	.cfi_offset	%rdi, SIGF_UC + UC_RDI
	.cfi_offset	%rsi, SIGF_UC + UC_RSI
	.cfi_offset	%rdx, SIGF_UC + UC_RDX
	.cfi_offset	%rcx, SIGF_UC + UC_RCX
	.cfi_offset	%r8,  SIGF_UC + UC_R8
	.cfi_offset	%r9,  SIGF_UC + UC_R9
	.cfi_offset	%rax, SIGF_UC + UC_RAX
	.cfi_offset	%rbx, SIGF_UC + UC_RBX
	.cfi_offset	%rbp, SIGF_UC + UC_RBP
	.cfi_offset	%r10, SIGF_UC + UC_R10
	.cfi_offset	%r11, SIGF_UC + UC_R11
	.cfi_offset	%r12, SIGF_UC + UC_R12
	.cfi_offset	%r13, SIGF_UC + UC_R13
	.cfi_offset	%r14, SIGF_UC + UC_R14
	.cfi_offset	%r15, SIGF_UC + UC_R15
#if 0
/*
 * Gnu as complains about %fs/%gs/%es/%ds registers offsets not being
 * multiple of 8, but gas + ld.bfd work for %cs/%ss.
 *
 * Clang IAS + ld.lld combination cannot handle any of the segment
 * registers.  Also, clang IAS does not know %rflags/%fs.base/%gs.base
 * registers names, use dwarf registers numbers from psABI directly.
 */	
	.cfi_offset	%fs,  SIGF_UC + UC_FS
	.cfi_offset	%gs,  SIGF_UC + UC_GS
	.cfi_offset	%es,  SIGF_UC + UC_ES
	.cfi_offset	%ds,  SIGF_UC + UC_DS
#endif
	.cfi_offset	%rip, SIGF_UC + UC_RIP
#if 0
	.cfi_offset	%cs,  SIGF_UC + UC_CS
#endif
	.cfi_offset	49 /* %rflags */, SIGF_UC + UC_RFLAGS
	.cfi_offset	%rsp, SIGF_UC + UC_RSP
#if 0
	.cfi_offset	%ss,  SIGF_UC + UC_SS
#endif
	.cfi_offset	58 /* %fs.base */, SIGF_UC + UC_FSBASE
	.cfi_offset	59 /* %gs.base */, SIGF_UC + UC_GSBASE
	call	*SIGF_HANDLER(%rsp)	/* call signal handler */
	lea	SIGF_UC(%rsp),%rdi	/* get ucontext_t */
	pushq	$0			/* junk to fake return addr. */
	.cfi_def_cfa	%rsp, 8
	movq	$SYS_sigreturn,%rax
	syscall				/* enter kernel with args */
0:	hlt				/* trap priviliged instruction */
	jmp	0b
	.cfi_endproc
END(__vdso_sigcode)

	.section .note.GNU-stack,"",%progbits

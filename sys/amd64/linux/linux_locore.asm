/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2022 Dmitry Chagin <dchagin@freeBSD.org>
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

#include "linux_assym.h"			/* system definitions */
#include <machine/asmacros.h>			/* miscellaneous asm macros */

#include <amd64/linux/linux_syscall.h>		/* system call numbers */

	.data

	.globl linux_platform
linux_platform:
	.asciz "x86_64"

	.text

ENTRY(linux_rt_sigcode)
	.cfi_startproc
	.cfi_signal_frame
	.cfi_def_cfa	%rsp, LINUX_RT_SIGF_SC
	.cfi_offset	%r8, L_SC_R8
	.cfi_offset	%r9, L_SC_R9
	.cfi_offset	%r10, L_SC_R10
	.cfi_offset	%r11, L_SC_R11
	.cfi_offset	%r12, L_SC_R12
	.cfi_offset	%r13, L_SC_R13
	.cfi_offset	%r14, L_SC_R14
	.cfi_offset	%r15, L_SC_R15
	.cfi_offset	%rdi, L_SC_RDI
	.cfi_offset	%rsi, L_SC_RSI
	.cfi_offset	%rbp, L_SC_RBP
	.cfi_offset	%rbx, L_SC_RBX
	.cfi_offset	%rdx, L_SC_RDX
	.cfi_offset	%rax, L_SC_RAX
	.cfi_offset	%rcx, L_SC_RCX
	.cfi_offset	%rip, L_SC_RIP
	.cfi_offset	%rsp, L_SC_RSP

	movq	%rsp, %rbx			/* rt_sigframe for rt_sigreturn */
	call	*%rcx				/* call signal handler */
	movq	$LINUX_SYS_linux_rt_sigreturn, %rax
	syscall
0:	hlt
	jmp	0b
	.cfi_endproc
END(linux_rt_sigcode)

#if 0
	.section .note.Linux, "a",@note
	.long 2f - 1f		/* namesz */
	.balign 4
	.long 4f - 3f		/* descsz */
	.long 0
1:
	.asciz "Linux"
2:
	.balign 4
3:
	.long LINUX_VERSION_CODE
4:
	.balign 4
	.previous
#endif

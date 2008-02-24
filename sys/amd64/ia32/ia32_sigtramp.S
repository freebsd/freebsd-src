/*-
 * Copyright (c) 2003 Peter Wemm
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
 * $FreeBSD: src/sys/amd64/ia32/ia32_sigtramp.S,v 1.3 2006/09/23 13:42:09 davidxu Exp $
 */

#include "opt_compat.h"

#include <machine/asmacros.h>
#include <sys/syscall.h>

#include "ia32_assym.h"

	.text
	.code32
/*
 * Signal trampoline, copied to top of user stack
 * XXX may need to be MD to match backend sendsig handoff protocol
 */
	ALIGN_TEXT
	.globl	ia32_sigcode
ia32_sigcode:
	calll	*IA32_SIGF_HANDLER(%esp)
	leal	IA32_SIGF_UC(%esp),%eax	/* get ucontext */
	pushl	%eax
	movl	IA32_UC_ES(%eax),%es	/* restore %es */
	movl	IA32_UC_DS(%eax),%ds	/* restore %ds */
	movl	$SYS_sigreturn,%eax
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
					/* on stack */
1:
	jmp	1b

#ifdef COMPAT_FREEBSD4
	ALIGN_TEXT
freebsd4_ia32_sigcode:
	calll	*IA32_SIGF_HANDLER(%esp)
	leal	IA32_SIGF_UC4(%esp),%eax/* get ucontext */
	pushl	%eax
	movl	IA32_UC4_ES(%eax),%es	/* restore %es */
	movl	IA32_UC4_DS(%eax),%ds	/* restore %ds */
	movl	$344,%eax		/* 4.x SYS_sigreturn */
	pushl	%eax			/* junk to fake return addr. */
	int	$0x80			/* enter kernel with args */
					/* on stack */
1:
	jmp	1b
#endif

	ALIGN_TEXT
esigcode:

	.data
	.globl	sz_ia32_sigcode
sz_ia32_sigcode:
	.long	esigcode-ia32_sigcode
#ifdef COMPAT_FREEBSD4
	.globl	sz_freebsd4_ia32_sigcode
sz_freebsd4_ia32_sigcode:
	.long	esigcode-freebsd4_ia32_sigcode
#endif

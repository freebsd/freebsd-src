/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#include <sys/syscall.h>

#include <machine/asmacros.h>
#include <machine/pstate.h>

#include "assym.s"

	.globl	kernbase
	.set	kernbase,KERNBASE

/*
 * void _start(caddr_t metadata, u_long o1, u_long o2, u_long o3, 
 *	       u_long ofw_vec)
 */
ENTRY(_start)
	wrpr	%g0, PSTATE_IE | PSTATE_PRIV | PSTATE_PEF, %pstate
	mov	%o0, %g1
	mov	%o4, %g2
	flushw
	wrpr	%g0, 1, %cwp
	wrpr	%g0, 0, %cleanwin
	wrpr	%g0, 0, %pil

	SET(kstack0 + KSTACK_PAGES * PAGE_SIZE - PCB_SIZEOF, %l0, %o0)
	sub	%o0, SPOFF + CCFSZ, %sp

	mov	%g1, %o0
	call	sparc64_init
	 mov	%g2, %o1
	call	mi_startup
	 nop
	! NOTREACHED
END(_start)

ENTRY(sigcode)
	call	%o4
	 nop
	add	%sp, SPOFF + CCFSZ + SF_UC, %o0
	mov	SYS_sigreturn, %g1
	ta	%xcc, 9
	mov	SYS_exit, %g1
	ta	%xcc, 9
1:	b	%xcc, 1b
	 nop
	.align 16
esigcode:
END(sigcode)

DATA(szsigcode)
	.long	esigcode - sigcode

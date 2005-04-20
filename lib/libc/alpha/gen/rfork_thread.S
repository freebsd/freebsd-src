/*-
 * Copyright (c) 2000 Peter Wemm <peter@FreeBSD.org>
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include "SYS.h"
#include <sys/syscall.h>
/* #include <machine/pal.h> */

/*
 * This is pretty evil and based mostly on examples from other syscall
 * stubs and code that gcc generated.   Correctness is uncertain, but it
 * seems to work quite happily.
 */
LEAF(rfork_thread, 1)
	br gp,L1			/* XXX profiling */
L1:
        LDGP(pv)
	lda sp,-16(sp)
	stq ra,0(sp)
	mov a3,a5
	CALLSYS_ERROR(rfork)
        beq v0,$child
	addl v0,zero,v0
	ldq ra,0(sp)
	lda sp,16(sp)
	RET
$child:
	mov a1,sp
	lda sp,-16(sp)
	stq zero,0(sp)
	mov a5,a0
	mov a2,t12
	jsr ra,(t12),0
	ldgp gp,0(ra)
	mov v0,a0
#ifdef SYS_exit
	CALLSYS_NOERROR(exit)
#else
	CALLSYS_NOERROR(sys_exit)
#endif
END(rfork_thread)

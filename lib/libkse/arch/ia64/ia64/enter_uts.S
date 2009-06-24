/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * void _ia64_enter_uts(kse_func_t uts, struct kse_mailbox *km, void *stack,
 *     size_t stacksz);
 */
ENTRY(_ia64_enter_uts, 4)
{	.mmi
	ld8		r14=[in0],8
	mov		ar.rsc=0xc
	add		r15=in2,in3
	;;
}
{	.mmi
	flushrs
	ld8		r1=[in0]
	mov		b7=r14
	;;
}
{	.mii
	mov		ar.bspstore=in2
	add		sp=-16,r15
	mov		rp=r14
	;;
}
{	.mib
	mov		ar.rsc=0xf
	mov		in0=in1
	br.cond.sptk	b7
	;;
}
1:	br.cond.sptk	1b
END(_ia64_enter_uts)

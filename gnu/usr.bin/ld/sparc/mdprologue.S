/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *	$FreeBSD$
 */

/*
 * SPARC run-time link editor entry points.
 */

#define CRT_VERSION_SUN		1

	.seg	"text"			! [internal]
	.proc	16
	.global	_rtld_entry
_rtld_entry:
!#PROLOGUE# 0
	save	%sp,-96,%sp
L.1B:
	call	L.2B
	sethi	%hi((__GLOBAL_OFFSET_TABLE_-(L.1B-.))),%l7
L.2B:
!#PROLOGUE# 1
	or	%l7,%lo((__GLOBAL_OFFSET_TABLE_-(L.1B-.))),%l7
	add	%l7,%o7,%l7

	cmp	%i0, CRT_VERSION_SUN	! is crtp passed in Sun style,
	bne	1f			!  ie. relative to stack frame ?
	nop
	add	%i1, %fp, %i1		! if so, adjust to absolute address
1:
	ld	[%i1], %o3		! load base address (crtp->crt_ba)
	ld	[%l7], %o2		! get __DYNAMIC address
					!   from 1st GOT entry
	add	%o2, %o3, %o2		! relocate and make it 3rd arg.

	ld	[%l7 + _rtld], %g1	! get address of rtld()
	add	%g1, %o3, %g1		! relocate

	mov	%i1, %o1		! set up args, #2: crtp
	call	%g1			! rtld(version, crtp, dp)
	mov	%i0, %o0		! arg #1: version

	ret
	restore
	.seg	"data"			! [internal]

	.seg	"text"
	.global	_binder_entry
_binder_entry:
!#PROLOGUE# 0
	save	%sp,-96,%sp
!L.1C:
!	call	L.2C
!	sethi	%hi((__GLOBAL_OFFSET_TABLE_-(L.1C-.))),%l7
!L.2C:
!	or	%l7,%lo((__GLOBAL_OFFSET_TABLE_-(L.1C-.))),%l7
!#PROLOGUE# 1

	sub	%i7, 4, %o0		! get to jmpslot through pc
	ld	[%i7+4], %o1		! get relocation index
	sethi	%hi(0x3fffff), %o2	! -> reloc_index & 0x003fffff
	or	%o2, %lo(0x3fffff), %o2	! [internal]
	call	_binder			! and call binder(jsp, reloc_index)
	and	%o1, %o2, %o1

	mov	%o0, %g1		! return value == function address

	restore				! get rid of our context
	jmp	%g1			! and go.
	restore				! and the jmpslot context
	nop

	.seg	"data"			! [internal]


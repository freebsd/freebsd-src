; Copyright (c) 1988, 1993
;	The Regents of the University of California.  All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions
; are met:
; 1. Redistributions of source code must retain the above copyright
;    notice, this list of conditions and the following disclaimer.
; 2. Redistributions in binary form must reproduce the above copyright
;    notice, this list of conditions and the following disclaimer in the
;    documentation and/or other materials provided with the distribution.
; 3. All advertising materials mentioning features or use of this software
;    must display the following acknowledgement:
;	This product includes software developed by the University of
;	California, Berkeley and its contributors.
; 4. Neither the name of the University nor the names of its contributors
;    may be used to endorse or promote products derived from this software
;    without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
; ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
; OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
; HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
; LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
; OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
; SUCH DAMAGE.
;
;	@(#)spintasm.asm	8.1 (Berkeley) 6/6/93
;

; The code in this file complete the spint calls

spint	struc
; union REGS
spint_ax	dw	1
spint_bx	dw	1
spint_cx	dw	1
spint_dx	dw	1
spint_si	dw	1
spint_di	dw	1
spint_cflag	dw	1
; struct SREGS
spint_es	dw	1
spint_cs	dw	1
spint_ss	dw	1
spint_ds	dw	1
; int intno
spint_intno	dw	1
; int done
spint_done	dw	1
; int rc
spint_rc	dw	1
;
spint	ends


ENTER	MACRO
	; Begin enter
	push	bp
	mov	bp,sp

	push	ax
	push	bx
	push	cx
	push	dx
	push	bp
	push	di
	push	si
	push	ds
	push	es
	pushf

	mov	cs:start_sp, sp
	mov	cs:start_ss, ss
	; End enter
	ENDM

LEAVE	MACRO
	; Begin leave
	cli
	mov	sp, cs:start_sp
	mov	ss, cs:start_ss
	sti

	popf
	pop	es
	pop	ds
	pop	si
	pop	di
	pop	bp
	pop	dx
	pop	cx
	pop	bx
	pop	ax

	mov	sp,bp
	pop	bp
	ret
	; End leave
	ENDM

GETREGS	MACRO	wherefrom
	mov	si, wherefrom
	mov	spint_segment, ds
	mov	spint_offset, si

	mov	ax, spint_ax[si]
	mov	bx, spint_bx[si]
	mov	cx, spint_cx[si]
	mov	dx, spint_dx[si]
	; XXX mov	si, spint_si[si]
	mov	di, spint_di[si]
	mov	es, spint_es[si]
	; Now, need to do DS, SI
	push	spint_ds[si]
	mov	si, spint_si[si]
	pop	ds
	ENDM


SETREGS	MACRO
	mov	cs:old_si, si
	mov	cs:old_ds, ds

	mov	ds, cs:spint_segment
	mov	si, cs:spint_offset

	mov	spint_ax[si], ax
	mov	spint_bx[si], bx
	mov	spint_cx[si], cx
	mov	spint_dx[si], dx

	mov	spint_si[si], si
	mov	spint_di[si], di

	mov	spint_cs[si], cs
	mov	spint_ds[si], ds
	mov	spint_es[si], es
	mov	spint_ss[si], ss
	; now, need to do SI, DS
	mov	ax, old_si
	mov	spint_si[si], ax
	mov	ax, old_ds
	mov	spint_ds[si], ax
	ENDM


_TEXT	segment	byte public 'CODE'
_TEXT	ends

_DATA	segment	word public 'DATA'
_DATA	ends

CONST	segment	word public 'CONST'
CONST	ends

_BSS	segment word public 'BSS'
_BSS	ends

DGROUP	group	CONST, _BSS, _DATA

	assume	cs:_TEXT, ds:DGROUP, ss:DGROUP, es:DGROUP

_TEXT	segment

start_sp	dw	1 dup (?)	; For use in our 'longjmp'
start_ss	dw	1 dup (?)	; For use in our 'longjmp'

spint_segment	dw	1 dup (?)	; Segment of spawn control block
spint_offset	dw	1 dup (?)	; Offset of spawn control block

old_si		dw	1 dup (?)	; SI of interrupt issuer (temporary)
old_ds		dw	1 dup (?)	; DS of interrupt issuer (temporary)

issuer_ss	dw	1 dup (?)	; ss of person who called us (permanent)
issuer_sp	dw	1 dup (?)	; sp of person who called us (permanent)

int21_stack	db	100 dup (?)	; Stack for int21.

;
; _spint_int gets control on an interrupt.  It switches the stack
; and does a 'return' from _spint_start.
;
	public	__spint_int

__spint_int	proc	near
	mov	cs:issuer_sp, sp
	mov	cs:issuer_ss, ss
	sti

	SETREGS

	LEAVE
__spint_int	endp

;
; _spint_start issues the dos interrupt after setting up the passed
; registers.  When control returns to it, it sets spint->done to non-zero.
;
	public	__spint_start

__spint_start	proc	near
	ENTER

	GETREGS	4[bp]

	; Now, switch to a different (short) stack.  This is so
	; that our games won't mess up the stack int 21 (hardware and,
	; possibly, software) stores things on.

	cli
	mov	cs:int21_stack, cs
	mov	ss, cs:int21_stack
	mov	sp, offset int21_stack
	add	sp, (length int21_stack) - 4
	sti

	int	21H		; Issue DOS interrupt

	SETREGS

	mov	ds, cs:spint_segment
	mov	si, cs:spint_offset
	mov	spint_done[si], 1	; We are done

	LEAVE
__spint_start	endp

;
; After _spint_int has faked a return from start_spawn, we come here to
; return to the interrupt issuer.
;
	public	__spint_continue

__spint_continue	proc	near
	ENTER

	GETREGS	4[bp]

	mov	sp, cs:issuer_sp		; Restore SP
	mov	ss, cs:issuer_ss		; Restore SS

	iret
__spint_continue	endp

_TEXT	ends

	end

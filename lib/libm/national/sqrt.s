; Copyright (c) 1985 Regents of the University of California.
; All rights reserved.
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
;	@(#)sqrt.s	5.4 (Berkeley) 10/9/90
;

; double sqrt(x)
; double x;
; IEEE double precision sqrt
; code in NSC assembly by K.C. Ng
; 12/13/85
;
; Method:
; 	Use Kahan's trick to get 8 bits initial approximation
;	by integer shift and add/subtract. Then three Newton
;	iterations to get down error to within one ulp. Finally
;	twiddle the last bit to make to correctly rounded 
;	according to the rounding mode.
;
	.vers	2
	.text
	.align	2
	.globl	_sqrt
_sqrt:
	enter	[r3,r4,r5,r6,r7],44
	movl	f4,tos
	movl	f6,tos
	movd	2146435072,r2	; r2 = 0x7ff00000
	movl	8(fp),f0	; f2 = x 
	movd	12(fp),r3	; r3 = high part of x
	movd	r3,r4		; make a copy of high part of x in r4
	andd	r2,r3		; r3 become the bias exponent of x
	cmpd	r2,r3		; if r3 = 0x7ff00000 then x is INF or NAN
	bne	L22
				; to see if x is INF
	movd	8(fp),r0	; r0 = low part of x
	movd	r4,r1		; r1 is high part of x again
	andd	0xfff00000,r1	; mask off the sign and exponent of x
	ord	r0,r1		; or with low part, if 0 then x is INF
	cmpqd	0,r1		; 
	bne	L1		; not 0; therefore x is NaN; return x.
	cmpqd	0,r4		; now x is Inf, is it +inf?
	blt	L1		; +INF, return x 
				; -INF, return NaN by doing 0/0
nan:	movl	0f0.0,f0	; 
	divl	f0,f0
	br	L1
L22:				; now x is finite
	cmpl	0f0.0,f0	; x = 0 ?
	beq	L1		; return x if x is +0 or -0
	cmpqd	0,r4		; Is x < 0 ?
	bgt	nan		; if x < 0 return NaN
	movqd	0,r5		; r5 == scalx initialize to zero
	cmpqd	0,r3		; is x is subnormal ?  (r3 is the exponent)
	bne	L21		; if x is normal, goto L21
	movl	L30,f2		; f2 = 2**54
	mull	f2,f0		; scale up x by 2**54
	subd	0x1b00000,r5	; off set the scale factor by -27 in exponent
L21:
				; now x is normal
				; notations:
				;    r1 == copy of fsr
				;    r2 == mask of e inexact enable flag
				;    r3 == mask of i inexact flag
				;    r4 == mask of r rounding mode
				;    r5 == x's scale factor (already defined)

	movd	0x20,r2
	movd	0x40,r3
	movd	0x180,r4
	sfsr	r0		; store fsr to r0
	movd	r0,r1		; make a copy of fsr to r1
	bicd	[5,6,7,8],r0	; clear e,i, and set r to round to nearest
	lfsr	r0

				; begin to compute sqrt(x)
	movl	f0,-8(fp)
	movd	-4(fp),r0	; r0 the high part of modified x
	lshd	-1,r0		; r0 >> 1
	addd	0x1ff80000,r0	; add correction to r0  ...got 5 bits approx.
	movd	r0,r6
	lshd	-13,r6		; r6 = r0>>-15
	andd	0x7c,r6		; obtain 4*leading 5 bits of r0
	addrd	L29,r7		; r7 = address of L29 = table[0]
	addd	r6,r7		; r6 = address of L29[r6] = table[r6]
	subd	0(r7),r0	; r0 = r0 - table[r6]
	movd	r0,-4(fp)
	movl	-8(fp),f2	; now f2 = y approximate sqrt(f0) to 8 bits

	movl	0f0.5,f6	; f6 = 0.5
	movl	f0,f4
	divl	f2,f4		; t = x/y
	addl	f4,f2		; y = y + x/y
	mull	f6,f2		; y = 0.5(y+x/y) got 17 bits approx.
	movl	f0,f4
	divl	f2,f4		; t = x/y
	addl	f4,f2		; y = y + x/y
	mull	f6,f2		; y = 0.5(y+x/y) got 35 bits approx.
	movl	f0,f4
	divl	f2,f4		; t = x/y
	subl	f2,f4		; t = x/y - y
	mull	f6,f4		; t = 0.5(x/y-y)
	addl	f4,f2		; y = y + 0.5(x/y -y) 
				; now y approx. sqrt(x) to within 1 ulp

				; twiddle last bit to force y correctly rounded
	movd	r1,r0		; restore the old fsr
	bicd	[6,7,8],r0	; clear inexact bit but retain inexact enable
	ord	0x80,r0		; set rounding mode to round to zero
	lfsr	r0

	movl	f0,f4
	divl	f2,f4		; f4 = x/y
	sfsr	r0
	andd	r3,r0		; get the inexact flag
	cmpqd	0,r0
	bne	L18
				; if x/y exact, then ...
	cmpl	f2,f4		; if y == x/y 
	beq	L2
	movl	f4,-8(fp)
	subd	1,-8(fp)
	subcd	0,-4(fp)	
	movl	-8(fp),f4	; f4 = f4 - ulp
L18:
	bicd	[6],r1
	ord	r3,r1		; set inexact flag in r1

	andd	r1,r4		; r4 = the old rounding mode
	cmpqd	0,r4		; round to nearest?
	bne	L17
	movl	f4,-8(fp)
	addd	1,-8(fp)
	addcd	0,-4(fp)
	movl	-8(fp),f4	; f4 = f4 + ulp
	br	L16
L17:
	cmpd	0x100,r4	; round to positive inf ?
	bne	L16
	movl	f4,-8(fp)	
	addd	1,-8(fp)
	addcd	0,-4(fp)
	movl	-8(fp),f4	; f4 = f4 + ulp

	movl	f2,-8(fp)	
	addd	1,-8(fp)
	addcd	0,-4(fp)
	movl	-8(fp),f2	; f2 = f2 + ulp
L16:
	addl	f4,f2		; y  = y + t
	subd	0x100000,r5	; scalx = scalx - 1
L2:
	movl	f2,-8(fp)	
	addd	r5,-4(fp)
	movl	-8(fp),f0
	lfsr	r1
L1:
	movl	tos,f6
	movl	tos,f4
	exit	[r3,r4,r5,r6,r7]
	ret	0
	.data
L28:	.byte	64,40,35,41,115,113,114,116,46,99
	.byte	9,49,46,49,32,40,117,99,98,46
	.byte	101,108,101,102,117,110,116,41,32,57
	.byte	47,49,57,47,56,53,0
L29:	.blkb	4
	.double	1204
	.double	3062
	.double	5746
	.double	9193
	.double	13348
	.double	18162
	.double	23592
	.double	29598
	.double	36145
	.double	43202
	.double	50740
	.double	58733
	.double	67158
	.double	75992
	.double	85215
	.double	83599
	.double	71378
	.double	60428
	.double	50647
	.double	41945
	.double	34246
	.double	27478
	.double	21581
	.double	16499
	.double	12183
	.double	8588
	.double	5674
	.double	3403
	.double	1742
	.double	661
	.double	130
L30:	.blkb	4
	.double	1129316352 	;L30:	.double 0,0x43500000
L31:	.blkb	4
	.double 0x1ff00000
L32:	.blkb	4
	.double 0x5ff00000

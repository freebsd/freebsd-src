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
;	@(#)support.s	5.4 (Berkeley) 10/9/90
;

; IEEE recommended functions
; 
; double copysign(x,y)
; double x,y;
; IEEE 754 recommended function, return x*sign(y)
; Coded by K.C. Ng in National 32k assembler, 11/9/85.
;
	.vers	2
	.text
	.align	2
	.globl	_copysign
_copysign:
	movl	4(sp),f0
	movd	8(sp),r0
	movd	16(sp),r1
	xord	r0,r1
	andd	0x80000000,r1
	cmpqd	0,r1
	beq	end
	negl	f0,f0
end:	ret	0

; 
; double logb(x)
; double x;
; IEEE p854 recommended function, return the exponent of x (return float(N) 
; such that 1 <= x*2**-N < 2, even for subnormal number.
; Coded by K.C. Ng in National 32k assembler, 11/9/85.
; Note: subnormal number (if implemented) will be taken care of. 
;
	.vers	2
	.text
	.align	2
	.globl	_logb
_logb:
;
; extract the exponent of x
; glossaries:	r0 = high part of x
;		r1 = unbias exponent of x
;		r2 = 20 (first exponent bit position)
;
	movd	8(sp),r0
	movd	20,r2
	extd	r2,r0,r1,11	; extract the exponent of x
	cmpqd	0,r1		; if exponent bits = 0, goto L3
	beq	L3
	cmpd	0x7ff,r1
	beq	L2		; if exponent bits = 0x7ff, goto L2
L1:	subd	1023,r1		; unbias the exponent
	movdl	r1,f0		; convert the exponent to floating value
	ret	0
;
; x is INF or NaN, simply return x
;
L2:
	movl	4(sp),f0	; logb(+inf)=+inf, logb(NaN)=NaN
	ret	0
;
; x is 0 or subnormal
;
L3:
	movl	4(sp),f0
	cmpl	0f0,f0
	beq	L5		; x is 0 , goto L5 (return -inf)
;
; Now x is subnormal
;
	mull	L64,f0		; scale up f0 with 2**64
	movl	f0,tos
	movd	tos,r0
	movd	tos,r0		; now r0 = new high part of x
	extd	r2,r0,r1,11	; extract the exponent of x to r1
	subd	1087,r1		; unbias the exponent with correction 
	movdl	r1,f0		; convert the exponent to floating value
	ret	0
;
; x is 0, return logb(0)= -INF
;
L5:
	movl	0f1.0e300,f0
	mull	0f-1.0e300,f0	; multiply two big numbers to get -INF
	ret	0
; 
; double rint(x)
; double x;
; ... delivers integer nearest x in direction of prevailing rounding
; ... mode
; Coded by K.C. Ng in National 32k assembler, 11/9/85.
; Note: subnormal number (if implemented) will be taken care of. 
;
	.vers	2
	.text
	.align	2
	.globl	_rint
_rint:
;
	movd	8(sp),r0
	movd	20,r2
	extd	r2,r0,r1,11	; extract the exponent of x
	cmpd	0x433,r1
	ble	itself
	movl	L52,f2		; f2 = L = 2**52
	cmpqd	0,r0
	ble	L1
	negl	f2,f2		; f2 = s = copysign(L,x)
L1:	addl	f2,f0		; f0 = x + s
	subl	f2,f0		; f0 = f0 - s
	ret	0
itself:	movl	4(sp),f0
	ret	0
L52:	.double	0x0,0x43300000	; L52=2**52
; 
; int finite(x)
; double x;
; IEEE 754 recommended function, return 0 if x is NaN or INF, else 0
; Coded by K.C. Ng in National 32k assembler, 11/9/85.
;
	.vers	2
	.text
	.align	2
	.globl	_finite
_finite:
	movd	4(sp),r1
	andd	0x800fffff,r1
	cmpd	0x7ff00000,r1
	sned	r0		; r0=0 if exponent(x) = 0x7ff
	ret	0
; 
; double scalb(x,N)
; double x; int N;
; IEEE 754 recommended function, return x*2**N by adjusting 
; exponent of x.
; Coded by K.C. Ng in National 32k assembler, 11/9/85. 
; Note: subnormal number (if implemented) will be taken care of 
;
	.vers	2
	.text
	.align	2
	.globl	_scalb
_scalb:
;
; if x=0 return 0
;
	movl	4(sp),f0
	cmpl	0f0,f0
	beq	end		; scalb(0,N) is x itself
;
; extract the exponent of x
; glossaries:	r0 = high part of x, 
;		r1 = unbias exponent of x,
;		r2 = 20 (first exponent bit position).
;
	movd	8(sp),r0	; r0 = high part of x
	movd	20,r2		; r2 = 20
	extd	r2,r0,r1,11	; extract the exponent of x in r1
	cmpd	0x7ff,r1	
;
; if exponent of x is 0x7ff, then x is NaN or INF; simply return x  
;
	beq	end		
	cmpqd	0,r1
;
; if exponent of x is zero, then x is subnormal; goto L19
;
	beq	L19		
	addd	12(sp),r1	; r1 = (exponent of x) + N
	bfs	inof		; if integer overflows, goto inof
	cmpqd	0,r1		; if new exponent <= 0, goto underflow
	bge	underflow
	cmpd	2047,r1		; if new exponent >= 2047 goto overflow
	ble	overflow
	insd	r2,r1,r0,11	; insert the new exponent 
	movd	r0,tos
	movd	8(sp),tos
	movl	tos,f0		; return x*2**N
end:	ret	0
inof:	bcs	underflow	; negative int overflow if Carry bit is set
overflow:
	andd	0x80000000,r0	; keep the sign of x
	ord	0x7fe00000,r0	; set x to a huge number
	movd	r0,tos
	movqd	0,tos
	movl	tos,f0
	mull	0f1.0e300,f0	; multiply two huge number to get overflow
	ret	0
underflow:
	addd	64,r1		; add 64 to exonent to see if it is subnormal
	cmpqd	0,r1
	bge	zero		; underflow to zero
	insd	r2,r1,r0,11	; insert the new exponent 
	movd	r0,tos
	movd	8(sp),tos
	movl	tos,f0
	mull	L30,f0		; readjust x by multiply it with 2**-64
	ret	0
zero:	andd	0x80000000,r0	; keep the sign of x
	ord	0x00100000,r0	; set x to a tiny number
	movd	r0,tos
	movqd	0,tos
	movl	tos,f0
	mull	0f1.0e-300,f0	; underflow to 0  by multipling two tiny nos.
	ret	0
L19:		; subnormal number
	mull	L32,f0		; scale up x by 2**64
	movl	f0,tos
	movd	tos,r0
	movd	tos,r0		; get the high part of new x
	extd	r2,r0,r1,11	; extract the exponent of x in r1
	addd	12(sp),r1	; exponent of x + N
	subd	64,r1		; adjust it by subtracting 64
	cmpqd	0,r1
	bge	underflow
	cmpd	2047,r1
	ble	overflow
	insd	r2,r1,r0,11	; insert back the incremented exponent 
	movd	r0,tos
	movd	8(sp),tos
	movl	tos,f0
end:	ret	0
L30:	.double	0x0,0x3bf00000	; floating point 2**-64
L32:	.double	0x0,0x43f00000	; floating point 2**64
; 
; double drem(x,y)
; double x,y;
; IEEE double remainder function, return x-n*y, where n=x/y rounded to 
; nearest integer (half way case goes to even). Result exact.
; Coded by K.C. Ng in National 32k assembly, 11/19/85.
;
	.vers	2
	.text
	.align	2
	.globl	_drem
_drem:
;
; glossaries:	
;		r2 = high part of x
;		r3 = exponent of x
;		r4 = high part of y
;		r5 = exponent of y
;		r6 = sign of x
;		r7 = constant 0x7ff00000
;
;  16(fp) : y
;   8(fp) : x
; -12(fp) : adjustment on y when y is subnormal
; -16(fp) : fsr
; -20(fp) : nx
; -28(fp) : t
; -36(fp) : t1
; -40(fp) : nf
; 
;
	enter	[r3,r4,r5,r6,r7],40
	movl	f6,tos
	movl	f4,tos
	movl	0f0,-12(fp)
	movd	0,-20(fp)
	movd	0,-40(fp)
	movd	0x7ff00000,r7	; initialize r7=0x7ff00000
	movd	12(fp),r2	; r2 = high(x)
	movd	r2,r3
	andd	r7,r3		; r3 = xexp
	cmpd	r7,r3
; if x is NaN or INF goto L1
	beq	L1		
	movd	20(fp),r4
	bicd	[31],r4		; r4 = high part of |y|
	movd	r4,20(fp)	; y = |y|
	movd	r4,r5
	andd	r7,r5		; r5 = yexp
	cmpd	r7,r5
	beq	L2		; if y is NaN or INF goto L2
	cmpd	0x04000000,r5	; 
	bgt	L3		; if y is tiny goto L3
;
; now y != 0 , x is finite
;
L10:
	movd	r2,r6
	andd	0x80000000,r6	; r6 = sign(x)
	bicd	[31],r2		; x <- |x|
	sfsr	r1
	movd	r1,-16(fp)	; save fsr in -16(fp)
	bicd	[5],r1
	lfsr	r1		; disable inexact interupt
	movd	16(fp),r0	; r0 = low part of y
	movd	r0,r1		; r1 = r0 = low part of y
	andd	0xf8000000,r1	; mask off the lsb 27 bits of y

	movd	r2,12(fp)	; update x to |x|
	movd	r0,-28(fp)	; 
	movd	r4,-24(fp)	; t  = y
	movd	r4,-32(fp)	; 
	movd	r1,-36(fp)	; t1 = y with trialing 27 zeros
	movd	0x01900000,r1	; r1 = 25 in exponent field
LOOP:
	movl	8(fp),f0	; f0 = x
	movl	16(fp),f2	; f2 = y
	cmpl	f0,f2
	ble	fnad		; goto fnad (final adjustment) if x <= y
	movd	r4,-32(fp)
	movd	r3,r0
	subd	r5,r0		; xexp - yexp
	subd	r1,r0		; r0 = xexp - yexp - m25
	cmpqd	0,r0		; r0 > 0 ?
	bge	1f
	addd	r4,r0		; scale up (high) y
	movd	r0,-24(fp)	; scale up t
	movl	-28(fp),f2	; t
	movd	r0,-32(fp)	; scale up t1
1:
	movl	-36(fp),f4	; t1
	movl	f0,f6
	divl	f2,f6		; f6 = x/t
	floorld	f6,r0		; r0 = [x/t]
	movdl	r0,f6		; f6 = n
	subl	f4,f2		; t = t - t1 (tail of t1)
	mull	f6,f4		; f4 = n*t1	...exact
	subl	f4,f0		; x = x - n*t1
	mull	f6,f2		; n*(t-t1)	...exact
	subl	f2,f0		; x = x - n*(t-t1)
; update xexp
	movl	f0,8(fp)
	movd	12(fp),r3	
	andd	r7,r3
	jump	LOOP
fnad:
	cmpqd	0,-20(fp)	; 0 = nx?
	beq	final
	mull	-12(fp),8(fp)	; scale up x the same amount as y
	movd	0,-20(fp)
	movd	12(fp),r2
	movd	r2,r3
	andd	r7,r3		; update exponent of x
	jump	LOOP

final:
	movl	16(fp),f2	; f2 = y (f0=x, r0=n)
	subd	0x100000,r4	; high y /2
	movd	r4,-24(fp)
	movl	-28(fp),f4	; f4 = y/2
	cmpl	f0,f4		; x > y/2 ?
	bgt	1f
	bne	2f
	andd	1,r0		; n is odd or even
	cmpqd	0,r0
	beq	2f
1:
	subl	f2,f0		; x = x - y
2:
	cmpqd	0,-40(fp)
	beq	3f
	divl	-12(fp),f0	; scale down the answer
3:
	movl	f0,tos
	xord	r6,tos
	movl	tos,f0
	movd	-16(fp),r0
	lfsr	r0		; restore the fsr

end:	movl	tos,f4
	movl	tos,f6
	exit	[r3,r4,r5,r6,r7]
	ret	0
;
; y is NaN or INF
;
L2:	
	movd	16(fp),r0	; r0 = low part of y
	andd	0xfffff,r4	; r4 = high part of y & 0x000fffff
	ord	r4,r0
	cmpqd	0,r0
	beq	L4
	movl	16(fp),f0	; y is NaN, return y
	jump	end
L4:	movl	8(fp),f0	; y is inf, return x
	jump	end
;
; exponent of y is less than 64, y may be zero or subnormal
;
L3:
	movl	16(fp),f0
	cmpl	0f0,f0
	bne	L5
	divl	f0,f0		; y is 0, return NaN by doing 0/0
	jump	end
;
; subnormal y or tiny y
;
L5:	
	movd	0x04000000,-20(fp)	; nx = 64 in exponent field
	movl	L64,f2
	movl	f2,-12(fp)
	mull	f2,f0
	cmpl	f0,LTINY
	bgt	L6
	mull	f2,f0
	addd	0x04000000,-20(fp)	; nx = nx + 64 in exponent field
	mull	f2,-12(fp)
L6:
	movd	-20(fp),-40(fp)
	movl	f0,16(fp)
	movd	20(fp),r4
	movd	r4,r5
	andd	r7,r5		; exponent of new y
	jump	L10
;
; x is NaN or INF, return x-x
;
L1:
	movl	8(fp),f0
	subl	f0,f0		; if x is INF, then INF-INF is NaN
	ret	0
L64:	.double 0x0,0x43f00000	; L64 = 2**64
LTINY:	.double 0x0,0x04000000	; LTINY = 2**-959

! SPARC v7 __udiv_qrnnd division support, used from longlong.h.
! This is for v7 CPUs without a floating-point unit.

! Copyright (C) 1993, 1994, 1996 Free Software Foundation, Inc.

! This file is part of the GNU MP Library.

! The GNU MP Library is free software; you can redistribute it and/or modify
! it under the terms of the GNU Library General Public License as published by
! the Free Software Foundation; either version 2 of the License, or (at your
! option) any later version.

! The GNU MP Library is distributed in the hope that it will be useful, but
! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
! or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
! License for more details.

! You should have received a copy of the GNU Library General Public License
! along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
! the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
! MA 02111-1307, USA.


! INPUT PARAMETERS
! rem_ptr	o0
! n1		o1
! n0		o2
! d		o3

#include "sysdep.h"

	.text
	.align 4
	.global	C_SYMBOL_NAME(__udiv_qrnnd)
C_SYMBOL_NAME(__udiv_qrnnd):
	tst	%o3
	bneg	Largedivisor
	mov	8,%g1

	b	Lp1
	addxcc	%o2,%o2,%o2

Lplop:	bcc	Ln1
	addxcc	%o2,%o2,%o2
Lp1:	addx	%o1,%o1,%o1
	subcc	%o1,%o3,%o4
	bcc	Ln2
	addxcc	%o2,%o2,%o2
Lp2:	addx	%o1,%o1,%o1
	subcc	%o1,%o3,%o4
	bcc	Ln3
	addxcc	%o2,%o2,%o2
Lp3:	addx	%o1,%o1,%o1
	subcc	%o1,%o3,%o4
	bcc	Ln4
	addxcc	%o2,%o2,%o2
Lp4:	addx	%o1,%o1,%o1
	addcc	%g1,-1,%g1
	bne	Lplop
	subcc	%o1,%o3,%o4
	bcc	Ln5
	addxcc	%o2,%o2,%o2
Lp5:	st	%o1,[%o0]
	retl
	xnor	%g0,%o2,%o0

Lnlop:	bcc	Lp1
	addxcc	%o2,%o2,%o2
Ln1:	addx	%o4,%o4,%o4
	subcc	%o4,%o3,%o1
	bcc	Lp2
	addxcc	%o2,%o2,%o2
Ln2:	addx	%o4,%o4,%o4
	subcc	%o4,%o3,%o1
	bcc	Lp3
	addxcc	%o2,%o2,%o2
Ln3:	addx	%o4,%o4,%o4
	subcc	%o4,%o3,%o1
	bcc	Lp4
	addxcc	%o2,%o2,%o2
Ln4:	addx	%o4,%o4,%o4
	addcc	%g1,-1,%g1
	bne	Lnlop
	subcc	%o4,%o3,%o1
	bcc	Lp5
	addxcc	%o2,%o2,%o2
Ln5:	st	%o4,[%o0]
	retl
	xnor	%g0,%o2,%o0

Largedivisor:
	and	%o2,1,%o5	! %o5 = n0 & 1

	srl	%o2,1,%o2
	sll	%o1,31,%g2
	or	%g2,%o2,%o2	! %o2 = lo(n1n0 >> 1)
	srl	%o1,1,%o1	! %o1 = hi(n1n0 >> 1)

	and	%o3,1,%g2
	srl	%o3,1,%g3	! %g3 = floor(d / 2)
	add	%g3,%g2,%g3	! %g3 = ceil(d / 2)

	b	LLp1
	addxcc	%o2,%o2,%o2

LLplop:	bcc	LLn1
	addxcc	%o2,%o2,%o2
LLp1:	addx	%o1,%o1,%o1
	subcc	%o1,%g3,%o4
	bcc	LLn2
	addxcc	%o2,%o2,%o2
LLp2:	addx	%o1,%o1,%o1
	subcc	%o1,%g3,%o4
	bcc	LLn3
	addxcc	%o2,%o2,%o2
LLp3:	addx	%o1,%o1,%o1
	subcc	%o1,%g3,%o4
	bcc	LLn4
	addxcc	%o2,%o2,%o2
LLp4:	addx	%o1,%o1,%o1
	addcc	%g1,-1,%g1
	bne	LLplop
	subcc	%o1,%g3,%o4
	bcc	LLn5
	addxcc	%o2,%o2,%o2
LLp5:	add	%o1,%o1,%o1	! << 1
	tst	%g2
	bne	Oddp
	add	%o5,%o1,%o1
	st	%o1,[%o0]
	retl
	xnor	%g0,%o2,%o0

LLnlop:	bcc	LLp1
	addxcc	%o2,%o2,%o2
LLn1:	addx	%o4,%o4,%o4
	subcc	%o4,%g3,%o1
	bcc	LLp2
	addxcc	%o2,%o2,%o2
LLn2:	addx	%o4,%o4,%o4
	subcc	%o4,%g3,%o1
	bcc	LLp3
	addxcc	%o2,%o2,%o2
LLn3:	addx	%o4,%o4,%o4
	subcc	%o4,%g3,%o1
	bcc	LLp4
	addxcc	%o2,%o2,%o2
LLn4:	addx	%o4,%o4,%o4
	addcc	%g1,-1,%g1
	bne	LLnlop
	subcc	%o4,%g3,%o1
	bcc	LLp5
	addxcc	%o2,%o2,%o2
LLn5:	add	%o4,%o4,%o4	! << 1
	tst	%g2
	bne	Oddn
	add	%o5,%o4,%o4
	st	%o4,[%o0]
	retl
	xnor	%g0,%o2,%o0

Oddp:	xnor	%g0,%o2,%o2
	! q' in %o2. r' in %o1
	addcc	%o1,%o2,%o1
	bcc	LLp6
	addx	%o2,0,%o2
	sub	%o1,%o3,%o1
LLp6:	subcc	%o1,%o3,%g0
	bcs	LLp7
	subx	%o2,-1,%o2
	sub	%o1,%o3,%o1
LLp7:	st	%o1,[%o0]
	retl
	mov	%o2,%o0

Oddn:	xnor	%g0,%o2,%o2
	! q' in %o2. r' in %o4
	addcc	%o4,%o2,%o4
	bcc	LLn6
	addx	%o2,0,%o2
	sub	%o4,%o3,%o4
LLn6:	subcc	%o4,%o3,%g0
	bcs	LLn7
	subx	%o2,-1,%o2
	sub	%o4,%o3,%o4
LLn7:	st	%o4,[%o0]
	retl
	mov	%o2,%o0

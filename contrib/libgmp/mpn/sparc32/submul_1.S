! SPARC __mpn_submul_1 -- Multiply a limb vector with a limb and subtract
! the result from a second limb vector.

! Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.

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
! res_ptr	o0
! s1_ptr	o1
! size		o2
! s2_limb	o3

#include "sysdep.h"

.text
	.align 4
	.global	C_SYMBOL_NAME(__mpn_submul_1)
C_SYMBOL_NAME(__mpn_submul_1):
	! Make S1_PTR and RES_PTR point at the end of their blocks
	! and put (- 4 x SIZE) in index/loop counter.
	sll	%o2,2,%o2
	add	%o0,%o2,%o4	! RES_PTR in o4 since o0 is retval
	add	%o1,%o2,%o1
	sub	%g0,%o2,%o2

	cmp	%o3,0xfff
	bgu	Large
	nop

	ld	[%o1+%o2],%o5
	mov	0,%o0
	b	L0
	 add	%o4,-4,%o4
Loop0:
	subcc	%o5,%g1,%g1
	ld	[%o1+%o2],%o5
	addx	%o0,%g0,%o0
	st	%g1,[%o4+%o2]
L0:	wr	%g0,%o3,%y
	sra	%o5,31,%g2
	and	%o3,%g2,%g2
	andcc	%g1,0,%g1
	mulscc	%g1,%o5,%g1
 	mulscc	%g1,%o5,%g1
 	mulscc	%g1,%o5,%g1
 	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,%o5,%g1
	mulscc	%g1,0,%g1
	sra	%g1,20,%g4
	sll	%g1,12,%g1
 	rd	%y,%g3
	srl	%g3,20,%g3
	or	%g1,%g3,%g1

	addcc	%g1,%o0,%g1
	addx	%g2,%g4,%o0	! add sign-compensation and cy to hi limb
	addcc	%o2,4,%o2	! loop counter
	bne	Loop0
	 ld	[%o4+%o2],%o5

	subcc	%o5,%g1,%g1
	addx	%o0,%g0,%o0
	retl
	st	%g1,[%o4+%o2]


Large:	ld	[%o1+%o2],%o5
	mov	0,%o0
	sra	%o3,31,%g4	! g4 = mask of ones iff S2_LIMB < 0
	b	L1
	 add	%o4,-4,%o4
Loop:
	subcc	%o5,%g3,%g3
	ld	[%o1+%o2],%o5
	addx	%o0,%g0,%o0
	st	%g3,[%o4+%o2]
L1:	wr	%g0,%o5,%y
	and	%o5,%g4,%g2
	andcc	%g0,%g0,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%o3,%g1
	mulscc	%g1,%g0,%g1
	rd	%y,%g3
	addcc	%g3,%o0,%g3
	addx	%g2,%g1,%o0
	addcc	%o2,4,%o2
	bne	Loop
	 ld	[%o4+%o2],%o5

	subcc	%o5,%g3,%g3
	addx	%o0,%g0,%o0
	retl
	st	%g3,[%o4+%o2]

! SPARC v9 __mpn_sub_n -- Subtract two limb vectors of the same length > 0 and
! store difference in a third limb vector.

! Copyright (C) 1995, 1996 Free Software Foundation, Inc.

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
! res_ptr	%o0
! s1_ptr	%o1
! s2_ptr	%o2
! size		%o3

.section	".text"
	.align 4
	.global __mpn_sub_n
	.type	 __mpn_sub_n,#function
	.proc	04
__mpn_sub_n:
	sub %g0,%o3,%g3
	sllx %o3,3,%g1
	add %o1,%g1,%o1			! make s1_ptr point at end
	add %o2,%g1,%o2			! make s2_ptr point at end
	add %o0,%g1,%o0			! make res_ptr point at end
	mov 0,%o4			! clear carry variable
	sllx %g3,3,%o5			! compute initial address index

.Loop:	ldx [%o2+%o5],%g1		! load s2 limb
	add %g3,1,%g3			! increment loop count
	ldx [%o1+%o5],%g2		! load s1 limb
	addcc %g1,%o4,%g1		! add s2 limb and carry variable
	movcc %xcc,0,%o4		! if carry-out, o4 was 1; clear it
	subcc %g1,%g2,%g1		! subtract s1 limb from sum
	stx %g1,[%o0+%o5]		! store result
	add %o5,8,%o5			! increment address index
	brnz,pt %g3,.Loop
	movcs %xcc,1,%o4		! if s1 subtract gave carry, record it

	retl
	mov %o4,%o0
.LLfe1:
	.size	 __mpn_sub_n,.LLfe1-__mpn_sub_n

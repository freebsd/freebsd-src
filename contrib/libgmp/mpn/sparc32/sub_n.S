! SPARC __mpn_sub_n -- Subtract two limb vectors of the same length > 0 and
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
#define res_ptr	%o0
#define s1_ptr	%o1
#define s2_ptr	%o2
#define size	%o3

#include "sysdep.h"

	.text
	.align	4
	.global	C_SYMBOL_NAME(__mpn_sub_n)
C_SYMBOL_NAME(__mpn_sub_n):
	xor	s2_ptr,res_ptr,%g1
	andcc	%g1,4,%g0
	bne	L1			! branch if alignment differs
	nop
! **  V1a  **
	andcc	res_ptr,4,%g0		! res_ptr unaligned? Side effect: cy=0
	be	L_v1			! if no, branch
	nop
/* Add least significant limb separately to align res_ptr and s2_ptr */
	ld	[s1_ptr],%g4
	add	s1_ptr,4,s1_ptr
	ld	[s2_ptr],%g2
	add	s2_ptr,4,s2_ptr
	add	size,-1,size
	subcc	%g4,%g2,%o4
	st	%o4,[res_ptr]
	add	res_ptr,4,res_ptr
L_v1:	addx	%g0,%g0,%o4		! save cy in register
	cmp	size,2			! if size < 2 ...
	bl	Lend2			! ... branch to tail code
	subcc	%g0,%o4,%g0		! restore cy

	ld	[s1_ptr+0],%g4
	addcc	size,-10,size
	ld	[s1_ptr+4],%g1
	ldd	[s2_ptr+0],%g2
	blt	Lfin1
	subcc	%g0,%o4,%g0		! restore cy
/* Add blocks of 8 limbs until less than 8 limbs remain */
Loop1:	subxcc	%g4,%g2,%o4
	ld	[s1_ptr+8],%g4
	subxcc	%g1,%g3,%o5
	ld	[s1_ptr+12],%g1
	ldd	[s2_ptr+8],%g2
	std	%o4,[res_ptr+0]
	subxcc	%g4,%g2,%o4
	ld	[s1_ptr+16],%g4
	subxcc	%g1,%g3,%o5
	ld	[s1_ptr+20],%g1
	ldd	[s2_ptr+16],%g2
	std	%o4,[res_ptr+8]
	subxcc	%g4,%g2,%o4
	ld	[s1_ptr+24],%g4
	subxcc	%g1,%g3,%o5
	ld	[s1_ptr+28],%g1
	ldd	[s2_ptr+24],%g2
	std	%o4,[res_ptr+16]
	subxcc	%g4,%g2,%o4
	ld	[s1_ptr+32],%g4
	subxcc	%g1,%g3,%o5
	ld	[s1_ptr+36],%g1
	ldd	[s2_ptr+32],%g2
	std	%o4,[res_ptr+24]
	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-8,size
	add	s1_ptr,32,s1_ptr
	add	s2_ptr,32,s2_ptr
	add	res_ptr,32,res_ptr
	bge	Loop1
	subcc	%g0,%o4,%g0		! restore cy

Lfin1:	addcc	size,8-2,size
	blt	Lend1
	subcc	%g0,%o4,%g0		! restore cy
/* Add blocks of 2 limbs until less than 2 limbs remain */
Loope1:	subxcc	%g4,%g2,%o4
	ld	[s1_ptr+8],%g4
	subxcc	%g1,%g3,%o5
	ld	[s1_ptr+12],%g1
	ldd	[s2_ptr+8],%g2
	std	%o4,[res_ptr+0]
	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-2,size
	add	s1_ptr,8,s1_ptr
	add	s2_ptr,8,s2_ptr
	add	res_ptr,8,res_ptr
	bge	Loope1
	subcc	%g0,%o4,%g0		! restore cy
Lend1:	subxcc	%g4,%g2,%o4
	subxcc	%g1,%g3,%o5
	std	%o4,[res_ptr+0]
	addx	%g0,%g0,%o4		! save cy in register

	andcc	size,1,%g0
	be	Lret1
	subcc	%g0,%o4,%g0		! restore cy
/* Add last limb */
	ld	[s1_ptr+8],%g4
	ld	[s2_ptr+8],%g2
	subxcc	%g4,%g2,%o4
	st	%o4,[res_ptr+8]

Lret1:	retl
	addx	%g0,%g0,%o0	! return carry-out from most sign. limb

L1:	xor	s1_ptr,res_ptr,%g1
	andcc	%g1,4,%g0
	bne	L2
	nop
! **  V1b  **
	andcc	res_ptr,4,%g0		! res_ptr unaligned? Side effect: cy=0
	be	L_v1b			! if no, branch
	nop
/* Add least significant limb separately to align res_ptr and s1_ptr */
	ld	[s2_ptr],%g4
	add	s2_ptr,4,s2_ptr
	ld	[s1_ptr],%g2
	add	s1_ptr,4,s1_ptr
	add	size,-1,size
	subcc	%g2,%g4,%o4
	st	%o4,[res_ptr]
	add	res_ptr,4,res_ptr
L_v1b:	addx	%g0,%g0,%o4		! save cy in register
	cmp	size,2			! if size < 2 ...
	bl	Lend2			! ... branch to tail code
	subcc	%g0,%o4,%g0		! restore cy

	ld	[s2_ptr+0],%g4
	addcc	size,-10,size
	ld	[s2_ptr+4],%g1
	ldd	[s1_ptr+0],%g2
	blt	Lfin1b
	subcc	%g0,%o4,%g0		! restore cy
/* Add blocks of 8 limbs until less than 8 limbs remain */
Loop1b:	subxcc	%g2,%g4,%o4
	ld	[s2_ptr+8],%g4
	subxcc	%g3,%g1,%o5
	ld	[s2_ptr+12],%g1
	ldd	[s1_ptr+8],%g2
	std	%o4,[res_ptr+0]
	subxcc	%g2,%g4,%o4
	ld	[s2_ptr+16],%g4
	subxcc	%g3,%g1,%o5
	ld	[s2_ptr+20],%g1
	ldd	[s1_ptr+16],%g2
	std	%o4,[res_ptr+8]
	subxcc	%g2,%g4,%o4
	ld	[s2_ptr+24],%g4
	subxcc	%g3,%g1,%o5
	ld	[s2_ptr+28],%g1
	ldd	[s1_ptr+24],%g2
	std	%o4,[res_ptr+16]
	subxcc	%g2,%g4,%o4
	ld	[s2_ptr+32],%g4
	subxcc	%g3,%g1,%o5
	ld	[s2_ptr+36],%g1
	ldd	[s1_ptr+32],%g2
	std	%o4,[res_ptr+24]
	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-8,size
	add	s1_ptr,32,s1_ptr
	add	s2_ptr,32,s2_ptr
	add	res_ptr,32,res_ptr
	bge	Loop1b
	subcc	%g0,%o4,%g0		! restore cy

Lfin1b:	addcc	size,8-2,size
	blt	Lend1b
	subcc	%g0,%o4,%g0		! restore cy
/* Add blocks of 2 limbs until less than 2 limbs remain */
Loope1b:subxcc	%g2,%g4,%o4
	ld	[s2_ptr+8],%g4
	subxcc	%g3,%g1,%o5
	ld	[s2_ptr+12],%g1
	ldd	[s1_ptr+8],%g2
	std	%o4,[res_ptr+0]
	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-2,size
	add	s1_ptr,8,s1_ptr
	add	s2_ptr,8,s2_ptr
	add	res_ptr,8,res_ptr
	bge	Loope1b
	subcc	%g0,%o4,%g0		! restore cy
Lend1b:	subxcc	%g2,%g4,%o4
	subxcc	%g3,%g1,%o5
	std	%o4,[res_ptr+0]
	addx	%g0,%g0,%o4		! save cy in register

	andcc	size,1,%g0
	be	Lret1b
	subcc	%g0,%o4,%g0		! restore cy
/* Add last limb */
	ld	[s2_ptr+8],%g4
	ld	[s1_ptr+8],%g2
	subxcc	%g2,%g4,%o4
	st	%o4,[res_ptr+8]

Lret1b:	retl
	addx	%g0,%g0,%o0	! return carry-out from most sign. limb

! **  V2  **
/* If we come here, the alignment of s1_ptr and res_ptr as well as the
   alignment of s2_ptr and res_ptr differ.  Since there are only two ways
   things can be aligned (that we care about) we now know that the alignment
   of s1_ptr and s2_ptr are the same.  */

L2:	cmp	size,1
	be	Ljone
	nop
	andcc	s1_ptr,4,%g0		! s1_ptr unaligned? Side effect: cy=0
	be	L_v2			! if no, branch
	nop
/* Add least significant limb separately to align s1_ptr and s2_ptr */
	ld	[s1_ptr],%g4
	add	s1_ptr,4,s1_ptr
	ld	[s2_ptr],%g2
	add	s2_ptr,4,s2_ptr
	add	size,-1,size
	subcc	%g4,%g2,%o4
	st	%o4,[res_ptr]
	add	res_ptr,4,res_ptr

L_v2:	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-8,size
	blt	Lfin2
	subcc	%g0,%o4,%g0		! restore cy
/* Add blocks of 8 limbs until less than 8 limbs remain */
Loop2:	ldd	[s1_ptr+0],%g2
	ldd	[s2_ptr+0],%o4
	subxcc	%g2,%o4,%g2
	st	%g2,[res_ptr+0]
	subxcc	%g3,%o5,%g3
	st	%g3,[res_ptr+4]
	ldd	[s1_ptr+8],%g2
	ldd	[s2_ptr+8],%o4
	subxcc	%g2,%o4,%g2
	st	%g2,[res_ptr+8]
	subxcc	%g3,%o5,%g3
	st	%g3,[res_ptr+12]
	ldd	[s1_ptr+16],%g2
	ldd	[s2_ptr+16],%o4
	subxcc	%g2,%o4,%g2
	st	%g2,[res_ptr+16]
	subxcc	%g3,%o5,%g3
	st	%g3,[res_ptr+20]
	ldd	[s1_ptr+24],%g2
	ldd	[s2_ptr+24],%o4
	subxcc	%g2,%o4,%g2
	st	%g2,[res_ptr+24]
	subxcc	%g3,%o5,%g3
	st	%g3,[res_ptr+28]
	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-8,size
	add	s1_ptr,32,s1_ptr
	add	s2_ptr,32,s2_ptr
	add	res_ptr,32,res_ptr
	bge	Loop2
	subcc	%g0,%o4,%g0		! restore cy

Lfin2:	addcc	size,8-2,size
	blt	Lend2
	subcc	%g0,%o4,%g0		! restore cy
Loope2:	ldd	[s1_ptr+0],%g2
	ldd	[s2_ptr+0],%o4
	subxcc	%g2,%o4,%g2
	st	%g2,[res_ptr+0]
	subxcc	%g3,%o5,%g3
	st	%g3,[res_ptr+4]
	addx	%g0,%g0,%o4		! save cy in register
	addcc	size,-2,size
	add	s1_ptr,8,s1_ptr
	add	s2_ptr,8,s2_ptr
	add	res_ptr,8,res_ptr
	bge	Loope2
	subcc	%g0,%o4,%g0		! restore cy
Lend2:	andcc	size,1,%g0
	be	Lret2
	subcc	%g0,%o4,%g0		! restore cy
/* Add last limb */
Ljone:	ld	[s1_ptr],%g4
	ld	[s2_ptr],%g2
	subxcc	%g4,%g2,%o4
	st	%o4,[res_ptr]

Lret2:	retl
	addx	%g0,%g0,%o0	! return carry-out from most sign. limb

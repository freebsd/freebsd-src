 # MIPS3 __mpn_addmul_1 -- Multiply a limb vector with a single limb and
 # add the product to a second limb vector.

 # Copyright (C) 1992, 1994, 1995 Free Software Foundation, Inc.

 # This file is part of the GNU MP Library.

 # The GNU MP Library is free software; you can redistribute it and/or modify
 # it under the terms of the GNU Library General Public License as published by
 # the Free Software Foundation; either version 2 of the License, or (at your
 # option) any later version.

 # The GNU MP Library is distributed in the hope that it will be useful, but
 # WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 # or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 # License for more details.

 # You should have received a copy of the GNU Library General Public License
 # along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
 # the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 # MA 02111-1307, USA.


 # INPUT PARAMETERS
 # res_ptr	$4
 # s1_ptr	$5
 # size		$6
 # s2_limb	$7

	.text
	.align	4
	.globl	__mpn_addmul_1
	.ent	__mpn_addmul_1
__mpn_addmul_1:
	.set    noreorder
	.set    nomacro

 # warm up phase 0
	ld	$8,0($5)

 # warm up phase 1
	daddiu	$5,$5,8
	dmultu	$8,$7

	daddiu	$6,$6,-1
	beq	$6,$0,$LC0
	 move	$2,$0		# zero cy2

	daddiu	$6,$6,-1
	beq	$6,$0,$LC1
	ld	$8,0($5)	# load new s1 limb as early as possible

Loop:	ld	$10,0($4)
	mflo	$3
	mfhi	$9
	daddiu	$5,$5,8
	daddu	$3,$3,$2	# add old carry limb to low product limb
	dmultu	$8,$7
	ld	$8,0($5)	# load new s1 limb as early as possible
	daddiu	$6,$6,-1	# decrement loop counter
	sltu	$2,$3,$2	# carry from previous addition -> $2
	daddu	$3,$10,$3
	sltu	$10,$3,$10
	daddu	$2,$2,$10
	sd	$3,0($4)
	daddiu	$4,$4,8
	bne	$6,$0,Loop
	 daddu	$2,$9,$2	# add high product limb and carry from addition

 # cool down phase 1
$LC1:	ld	$10,0($4)
	mflo	$3
	mfhi	$9
	daddu	$3,$3,$2
	sltu	$2,$3,$2
	dmultu	$8,$7
	daddu	$3,$10,$3
	sltu	$10,$3,$10
	daddu	$2,$2,$10
	sd	$3,0($4)
	daddiu	$4,$4,8
	daddu	$2,$9,$2	# add high product limb and carry from addition

 # cool down phase 0
$LC0:	ld	$10,0($4)
	mflo	$3
	mfhi	$9
	daddu	$3,$3,$2
	sltu	$2,$3,$2
	daddu	$3,$10,$3
	sltu	$10,$3,$10
	daddu	$2,$2,$10
	sd	$3,0($4)
	j	$31
	daddu	$2,$9,$2	# add high product limb and carry from addition

	.end	__mpn_addmul_1

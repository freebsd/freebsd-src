 # MIPS2 __mpn_rshift --

 # Copyright (C) 1995 Free Software Foundation, Inc.

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
 # src_ptr	$5
 # size		$6
 # cnt		$7

	.text
	.align	2
	.globl	__mpn_rshift
	.ent	__mpn_rshift
__mpn_rshift:
	.set	noreorder
	.set	nomacro

	lw	$10,0($5)	# load first limb
	subu	$13,$0,$7
	addiu	$6,$6,-1
	and	$9,$6,4-1	# number of limbs in first loop
	beq	$9,$0,.L0	# if multiple of 4 limbs, skip first loop
	 sll	$2,$10,$13	# compute function result

	subu	$6,$6,$9

.Loop0:	lw	$3,4($5)
	addiu	$4,$4,4
	addiu	$5,$5,4
	addiu	$9,$9,-1
	srl	$11,$10,$7
	sll	$12,$3,$13
	move	$10,$3
	or	$8,$11,$12
	bne	$9,$0,.Loop0
	 sw	$8,-4($4)

.L0:	beq	$6,$0,.Lend
	 nop

.Loop:	lw	$3,4($5)
	addiu	$4,$4,16
	addiu	$6,$6,-4
	srl	$11,$10,$7
	sll	$12,$3,$13

	lw	$10,8($5)
	srl	$14,$3,$7
	or	$8,$11,$12
	sw	$8,-16($4)
	sll	$9,$10,$13

	lw	$3,12($5)
	srl	$11,$10,$7
	or	$8,$14,$9
	sw	$8,-12($4)
	sll	$12,$3,$13

	lw	$10,16($5)
	srl	$14,$3,$7
	or	$8,$11,$12
	sw	$8,-8($4)
	sll	$9,$10,$13

	addiu	$5,$5,16
	or	$8,$14,$9
	bgtz	$6,.Loop
	 sw	$8,-4($4)

.Lend:	srl	$8,$10,$7
	j	$31
	sw	$8,0($4)
	.end	__mpn_rshift

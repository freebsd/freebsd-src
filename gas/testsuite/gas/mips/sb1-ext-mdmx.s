# Source file to test assembly of SB-1 MDMX subset instructions and extensions.
#
# SB-1 implements only the .ob MDMX instructions, and adds three additional
# MDMX-ish instructions (pabsdiff, pabsdiffc, pavg).

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	# The normal MDMX instructions:

	movf.l		$v1, $v12, $fcc5

	movn.l		$v1, $v12, $18

	movt.l		$v1, $v12, $fcc5

	movz.l		$v1, $v12, $18

	add.ob		$v1, $v12, 18
	add.ob		$v1, $v12, $v18
	add.ob		$v1, $v12, $v18[6]

	adda.ob		$v12, 18
	adda.ob		$v12, $v18
	adda.ob		$v12, $v18[6]

	addl.ob		$v12, 18
	addl.ob		$v12, $v18
	addl.ob		$v12, $v18[6]

	alni.ob		$v1, $v12, $v18, 6

	alnv.ob		$v1, $v12, $v18, $21

	and.ob		$v1, $v12, 18
	and.ob		$v1, $v12, $v18
	and.ob		$v1, $v12, $v18[6]

	c.eq.ob		$v12, 18
	c.eq.ob		$v12, $v18
	c.eq.ob		$v12, $v18[6]

	c.le.ob		$v12, 18
	c.le.ob		$v12, $v18
	c.le.ob		$v12, $v18[6]

	c.lt.ob		$v12, 18
	c.lt.ob		$v12, $v18
	c.lt.ob		$v12, $v18[6]

	max.ob		$v1, $v12, 18
	max.ob		$v1, $v12, $v18
	max.ob		$v1, $v12, $v18[6]

	min.ob		$v1, $v12, 18
	min.ob		$v1, $v12, $v18
	min.ob		$v1, $v12, $v18[6]

	mul.ob		$v1, $v12, 18
	mul.ob		$v1, $v12, $v18
	mul.ob		$v1, $v12, $v18[6]

	mula.ob		$v12, 18
	mula.ob		$v12, $v18
	mula.ob		$v12, $v18[6]

	mull.ob		$v12, 18
	mull.ob		$v12, $v18
	mull.ob		$v12, $v18[6]

	muls.ob		$v12, 18
	muls.ob		$v12, $v18
	muls.ob		$v12, $v18[6]

	mulsl.ob	$v12, 18
	mulsl.ob	$v12, $v18
	mulsl.ob	$v12, $v18[6]

	nor.ob		$v1, $v12, 18
	nor.ob		$v1, $v12, $v18
	nor.ob		$v1, $v12, $v18[6]

	or.ob		$v1, $v12, 18
	or.ob		$v1, $v12, $v18
	or.ob		$v1, $v12, $v18[6]

	pickf.ob	$v1, $v12, 18
	pickf.ob	$v1, $v12, $v18
	pickf.ob	$v1, $v12, $v18[6]

	pickt.ob	$v1, $v12, 18
	pickt.ob	$v1, $v12, $v18
	pickt.ob	$v1, $v12, $v18[6]

	rach.ob		$v1

	racl.ob		$v1

	racm.ob		$v1

	rnau.ob		$v1, 18
	rnau.ob		$v1, $v18
	rnau.ob		$v1, $v18[6]

	rneu.ob		$v1, 18
	rneu.ob		$v1, $v18
	rneu.ob		$v1, $v18[6]

	rzu.ob		$v1, 18
	rzu.ob		$v1, $v18
	rzu.ob		$v1, $v18[6]

	shfl.mixh.ob	$v1, $v12, $v18

	shfl.mixl.ob	$v1, $v12, $v18

	shfl.pach.ob	$v1, $v12, $v18

	shfl.upsl.ob	$v1, $v12, $v18

	sll.ob		$v1, $v12, 18
	sll.ob		$v1, $v12, $v18
	sll.ob		$v1, $v12, $v18[6]

	srl.ob		$v1, $v12, 18
	srl.ob		$v1, $v12, $v18
	srl.ob		$v1, $v12, $v18[6]

	sub.ob		$v1, $v12, 18
	sub.ob		$v1, $v12, $v18
	sub.ob		$v1, $v12, $v18[6]

	suba.ob		$v12, 18
	suba.ob		$v12, $v18
	suba.ob		$v12, $v18[6]

	subl.ob		$v12, 18
	subl.ob		$v12, $v18
	subl.ob		$v12, $v18[6]

	wach.ob		$v12

	wacl.ob		$v12, $v18

	xor.ob		$v1, $v12, 18
	xor.ob		$v1, $v12, $v18
	xor.ob		$v1, $v12, $v18[6]


	# The extensions:

	pabsdiff.ob	$v1, $v12, 18
	pabsdiff.ob	$v1, $v12, $v18
	pabsdiff.ob	$v1, $v12, $v18[6]

	pabsdiffc.ob	$v12, 18
	pabsdiffc.ob	$v12, $v18
	pabsdiffc.ob	$v12, $v18[6]

	pavg.ob		$v1, $v12, 18
	pavg.ob		$v1, $v12, $v18
	pavg.ob		$v1, $v12, $v18[6]


# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8

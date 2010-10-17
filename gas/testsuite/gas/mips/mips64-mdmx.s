# source file to test assembly of MIPS64 MDMX ASE instructions

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	movf.l		$v1, $v12, $fcc5

	movn.l		$v1, $v12, $18

	movt.l		$v1, $v12, $fcc5

	movz.l		$v1, $v12, $18

	add.ob		$v1, $v12, 18
	add.ob		$v1, $v12, $v18
	add.ob		$v1, $v12, $v18[6]

	add.qh		$v1, $v12, 18
	add.qh		$v1, $v12, $v18
	add.qh		$v1, $v12, $v18[2]

	adda.ob		$v12, 18
	adda.ob		$v12, $v18
	adda.ob		$v12, $v18[6]

	adda.qh		$v12, 18
	adda.qh		$v12, $v18
	adda.qh		$v12, $v18[2]

	addl.ob		$v12, 18
	addl.ob		$v12, $v18
	addl.ob		$v12, $v18[6]

	addl.qh		$v12, 18
	addl.qh		$v12, $v18
	addl.qh		$v12, $v18[2]

	alni.ob		$v1, $v12, $v18, 6

	alni.qh		$v1, $v12, $v18, 2

	alnv.ob		$v1, $v12, $v18, $21

	alnv.qh		$v1, $v12, $v18, $21

	and.ob		$v1, $v12, 18
	and.ob		$v1, $v12, $v18
	and.ob		$v1, $v12, $v18[6]

	and.qh		$v1, $v12, 18
	and.qh		$v1, $v12, $v18
	and.qh		$v1, $v12, $v18[2]

	c.eq.ob		$v12, 18
	c.eq.ob		$v12, $v18
	c.eq.ob		$v12, $v18[6]

	c.eq.qh		$v12, 18
	c.eq.qh		$v12, $v18
	c.eq.qh		$v12, $v18[2]

	c.le.ob		$v12, 18
	c.le.ob		$v12, $v18
	c.le.ob		$v12, $v18[6]

	c.le.qh		$v12, 18
	c.le.qh		$v12, $v18
	c.le.qh		$v12, $v18[2]

	c.lt.ob		$v12, 18
	c.lt.ob		$v12, $v18
	c.lt.ob		$v12, $v18[6]

	c.lt.qh		$v12, 18
	c.lt.qh		$v12, $v18
	c.lt.qh		$v12, $v18[2]

	max.ob		$v1, $v12, 18
	max.ob		$v1, $v12, $v18
	max.ob		$v1, $v12, $v18[6]

	max.qh		$v1, $v12, 18
	max.qh		$v1, $v12, $v18
	max.qh		$v1, $v12, $v18[2]

	min.ob		$v1, $v12, 18
	min.ob		$v1, $v12, $v18
	min.ob		$v1, $v12, $v18[6]

	min.qh		$v1, $v12, 18
	min.qh		$v1, $v12, $v18
	min.qh		$v1, $v12, $v18[2]

	msgn.qh		$v1, $v12, 18
	msgn.qh		$v1, $v12, $v18
	msgn.qh		$v1, $v12, $v18[2]

	mul.ob		$v1, $v12, 18
	mul.ob		$v1, $v12, $v18
	mul.ob		$v1, $v12, $v18[6]

	mul.qh		$v1, $v12, 18
	mul.qh		$v1, $v12, $v18
	mul.qh		$v1, $v12, $v18[2]

	mula.ob		$v12, 18
	mula.ob		$v12, $v18
	mula.ob		$v12, $v18[6]

	mula.qh		$v12, 18
	mula.qh		$v12, $v18
	mula.qh		$v12, $v18[2]

	mull.ob		$v12, 18
	mull.ob		$v12, $v18
	mull.ob		$v12, $v18[6]

	mull.qh		$v12, 18
	mull.qh		$v12, $v18
	mull.qh		$v12, $v18[2]

	muls.ob		$v12, 18
	muls.ob		$v12, $v18
	muls.ob		$v12, $v18[6]

	muls.qh		$v12, 18
	muls.qh		$v12, $v18
	muls.qh		$v12, $v18[2]

	mulsl.ob	$v12, 18
	mulsl.ob	$v12, $v18
	mulsl.ob	$v12, $v18[6]

	mulsl.qh	$v12, 18
	mulsl.qh	$v12, $v18
	mulsl.qh	$v12, $v18[2]

	nor.ob		$v1, $v12, 18
	nor.ob		$v1, $v12, $v18
	nor.ob		$v1, $v12, $v18[6]

	nor.qh		$v1, $v12, 18
	nor.qh		$v1, $v12, $v18
	nor.qh		$v1, $v12, $v18[2]

	or.ob		$v1, $v12, 18
	or.ob		$v1, $v12, $v18
	or.ob		$v1, $v12, $v18[6]

	or.qh		$v1, $v12, 18
	or.qh		$v1, $v12, $v18
	or.qh		$v1, $v12, $v18[2]

	pickf.ob	$v1, $v12, 18
	pickf.ob	$v1, $v12, $v18
	pickf.ob	$v1, $v12, $v18[6]

	pickf.qh	$v1, $v12, 18
	pickf.qh	$v1, $v12, $v18
	pickf.qh	$v1, $v12, $v18[2]

	pickt.ob	$v1, $v12, 18
	pickt.ob	$v1, $v12, $v18
	pickt.ob	$v1, $v12, $v18[6]

	pickt.qh	$v1, $v12, 18
	pickt.qh	$v1, $v12, $v18
	pickt.qh	$v1, $v12, $v18[2]

	rach.ob		$v1

	rach.qh		$v1

	racl.ob		$v1

	racl.qh		$v1

	racm.ob		$v1

	racm.qh		$v1

	rnas.qh		$v1, 18
	rnas.qh		$v1, $v18
	rnas.qh		$v1, $v18[2]

	rnau.ob		$v1, 18
	rnau.ob		$v1, $v18
	rnau.ob		$v1, $v18[6]

	rnau.qh		$v1, 18
	rnau.qh		$v1, $v18
	rnau.qh		$v1, $v18[2]

	rnes.qh		$v1, 18
	rnes.qh		$v1, $v18
	rnes.qh		$v1, $v18[2]

	rneu.ob		$v1, 18
	rneu.ob		$v1, $v18
	rneu.ob		$v1, $v18[6]

	rneu.qh		$v1, 18
	rneu.qh		$v1, $v18
	rneu.qh		$v1, $v18[2]

	rzs.qh		$v1, 18
	rzs.qh		$v1, $v18
	rzs.qh		$v1, $v18[2]

	rzu.ob		$v1, 18
	rzu.ob		$v1, $v18
	rzu.ob		$v1, $v18[6]

	rzu.qh		$v1, 18
	rzu.qh		$v1, $v18
	rzu.qh		$v1, $v18[2]

	shfl.bfla.qh	$v1, $v12, $v18

	shfl.mixh.ob	$v1, $v12, $v18
	shfl.mixh.qh	$v1, $v12, $v18

	shfl.mixl.ob	$v1, $v12, $v18
	shfl.mixl.qh	$v1, $v12, $v18

	shfl.pach.ob	$v1, $v12, $v18
	shfl.pach.qh	$v1, $v12, $v18

	shfl.repa.qh	$v1, $v12, $v18

	shfl.repb.qh	$v1, $v12, $v18

	shfl.upsl.ob	$v1, $v12, $v18

	sll.ob		$v1, $v12, 18
	sll.ob		$v1, $v12, $v18
	sll.ob		$v1, $v12, $v18[6]

	sll.qh		$v1, $v12, 18
	sll.qh		$v1, $v12, $v18
	sll.qh		$v1, $v12, $v18[2]

	sra.qh		$v1, $v12, 18
	sra.qh		$v1, $v12, $v18
	sra.qh		$v1, $v12, $v18[2]

	srl.ob		$v1, $v12, 18
	srl.ob		$v1, $v12, $v18
	srl.ob		$v1, $v12, $v18[6]

	srl.qh		$v1, $v12, 18
	srl.qh		$v1, $v12, $v18
	srl.qh		$v1, $v12, $v18[2]

	sub.ob		$v1, $v12, 18
	sub.ob		$v1, $v12, $v18
	sub.ob		$v1, $v12, $v18[6]

	sub.qh		$v1, $v12, 18
	sub.qh		$v1, $v12, $v18
	sub.qh		$v1, $v12, $v18[2]

	suba.ob		$v12, 18
	suba.ob		$v12, $v18
	suba.ob		$v12, $v18[6]

	suba.qh		$v12, 18
	suba.qh		$v12, $v18
	suba.qh		$v12, $v18[2]

	subl.ob		$v12, 18
	subl.ob		$v12, $v18
	subl.ob		$v12, $v18[6]

	subl.qh		$v12, 18
	subl.qh		$v12, $v18
	subl.qh		$v12, $v18[2]

	wach.ob		$v12

	wach.qh		$v12

	wacl.ob		$v12, $v18

	wacl.qh		$v12, $v18

	xor.ob		$v1, $v12, 18
	xor.ob		$v1, $v12, $v18
	xor.ob		$v1, $v12, $v18[6]

	xor.qh		$v1, $v12, 18
	xor.qh		$v1, $v12, $v18
	xor.qh		$v1, $v12, $v18[2]

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8

	.text

stuff:
	.ent stuff
	/* Integer instructions.  */

	mul	$4,$5,$6
	mulu	$4,$5,$6
	mulhi	$4,$5,$6
	mulhiu	$4,$5,$6
	muls	$4,$5,$6
	mulsu	$4,$5,$6
	mulshi	$4,$5,$6
	mulshiu	$4,$5,$6
	macc	$4,$5,$6
	maccu	$4,$5,$6
	macchi	$4,$5,$6
	macchiu	$4,$5,$6
	msac	$4,$5,$6
	msacu	$4,$5,$6
	msachi	$4,$5,$6
	msachiu	$4,$5,$6

	ror	$4,$5,25
	rorv	$4,$5,$6
	dror	$4,$5,25
	dror	$4,$5,57	/* Should expand to dror32 $4,$5,25.  */
	dror32	$4,$5,25
	drorv	$4,$5,$6

	/* Debug instructions.  */

	dbreak
	dret
	mfdr	$3,$3
	mtdr	$3,$3

	/* Coprocessor 0 instructions, minus standard ISA 3 ones.
	   That leaves just the performance monitoring registers.  */

	mfpc	$4,1
	mfps	$4,1
	mtpc	$4,1
	mtps	$4,1

	/* Multimedia instructions.  */

	.macro	nsel2 op
	/* Test each form of each vector opcode.  */
	\op	$f0,$f2
	\op	$f4,$f6[2]
	\op	$f6,15
	.if 0	/* Which is right?? */
	/* Test negative numbers in immediate-value slot.  */
	\op	$f4,-3
	.else
	/* Test that it's recognized as an unsigned field.  */
	\op	$f4,31
	.endif
	.endm

	.macro	nsel3 op
	/* Test each form of each vector opcode.  */
	\op	$f0,$f2,$f4
	\op	$f2,$f4,$f6[2]
	\op	$f6,$f4,15
	.if 0	/* Which is right?? */
	/* Test negative numbers in immediate-value slot.  */
	\op	$f4,$f6,-3
	.else
	/* Test that it's recognized as an unsigned field.  */
	\op	$f4,$f6,31
	.endif
	.endm

	nsel3	add.ob
	nsel3	and.ob
	nsel2	c.eq.ob
	nsel2	c.le.ob
	nsel2	c.lt.ob
	nsel3	max.ob
	nsel3	min.ob
	nsel3	mul.ob
	nsel2	mula.ob
	nsel2	mull.ob
	nsel2	muls.ob
	nsel2	mulsl.ob
	nsel3	nor.ob
	nsel3	or.ob
	nsel3	pickf.ob
	nsel3	pickt.ob
	nsel3	sub.ob
	nsel3	xor.ob

	/* ALNI, SHFL: Vector only.  */
	alni.ob		$f0,$f2,$f4,5
	shfl.mixh.ob	$f0,$f2,$f4
	shfl.mixl.ob	$f0,$f2,$f4
	shfl.pach.ob	$f0,$f2,$f4
	shfl.pacl.ob	$f0,$f2,$f4

	/* SLL,SRL: Scalar or immediate.  */
	sll.ob	$f2,$f4,$f6[3]
	sll.ob	$f4,$f6,14
	srl.ob	$f2,$f4,$f6[3]
	srl.ob	$f4,$f6,14

	/* RZU: Immediate, must be 0, 8, or 16.  */
	rzu.ob	$f2,13

	/* No selector.  */
	rach.ob	$f2
	racl.ob	$f2
	racm.ob	$f2
	wach.ob	$f2
	wacl.ob	$f2,$f4

	ror	$4,$5,$6
	rol	$4,$5,15
	dror	$4,$5,$6
	drol	$4,$5,31
	drol	$4,$5,62

	.space	8
	.end stuff

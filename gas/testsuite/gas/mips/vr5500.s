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


	/* Prefetch instructions.  */
        # We don't test pref because currently the disassembler will
        # disassemble it as lwc3.  lwc3 is correct for mips1 to mips3,
        # while pref is correct for mips4.  Unfortunately, the
        # disassembler does not know which architecture it is
        # disassembling for.
        # pref  4,0($4)

        prefx   4,$4($5)

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

	/* Miscellaneous instructions.  */

	wait
	wait	0		# disassembles without code
	wait	0x56789

	ssnop

	clo	$3,$4
	dclo	$3,$4
	clz	$3,$4
	dclz    $3,$4

	luxc1	$f0,$4($2)
	suxc1   $f2,$4($2)

	tlbp
	tlbr

	/* Align to 16-byte boundary.  */
	nop
	nop
	nop
	.end stuff

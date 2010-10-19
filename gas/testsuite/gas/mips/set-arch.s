	.text

	.set noreorder
	.set noat

	.set arch=4010

	flushi
	flushd
	flushid
	madd $4,$5
	maddu $5,$6
	ffc $6,$7
	ffs $7,$8
	msub $8,$9
	msubu $9,$10
	selsl $10,$11,$12
	selsr $11,$12,$13
	waiti
	wb 16($14)
	addciu $14,$15,16

	.set arch=4100

	hibernate
	standby
	suspend

	.set arch=4650

	mad $4,$5
	madu $5,$6
	mul $6,$7,$8

	# test mips4 instructions.

	.set arch=mips4

text_label:	
	bc1f	text_label
	bc1f	$fcc1,text_label
	bc1fl	$fcc1,text_label
	bc1t	$fcc1,text_label
	bc1tl	$fcc2,text_label
	c.f.d	$f4,$f6
	c.f.d	$fcc1,$f4,$f6
	ldxc1	$f2,$4($5)
	lwxc1	$f2,$4($5)
	madd.d	$f0,$f2,$f4,$f6
	madd.s	$f0,$f2,$f4,$f6
	movf	$4,$5,$fcc4
	movf.d	$f4,$f6,$fcc0
	movf.s	$f4,$f6,$fcc0
	movn	$4,$6,$6
	movn.d	$f4,$f6,$6
	movn.s	$f4,$f6,$6
	movt	$4,$5,$fcc4
	movt.d	$f4,$f6,$fcc0
	movt.s	$f4,$f6,$fcc0
	movz	$4,$6,$6
	movz.d	$f4,$f6,$6
	movz.s	$f4,$f6,$6
	msub.d	$f0,$f2,$f4,$f6
	msub.s	$f0,$f2,$f4,$f6
	nmadd.d	$f0,$f2,$f4,$f6
	nmadd.s	$f0,$f2,$f4,$f6
	nmsub.d	$f0,$f2,$f4,$f6
	nmsub.s	$f0,$f2,$f4,$f6

	# We don't test pref because currently the disassembler will
	# disassemble it as lwc3.  lwc3 is correct for mips1 to mips3,
	# while pref is correct for mips4.  Unfortunately, the
	# disassembler does not know which architecture it is
	# disassembling for.
	# pref	4,0($4)

	prefx	4,$4($5)
	recip.d	$f4,$f6
	recip.s	$f4,$f6
	rsqrt.d	$f4,$f6
	rsqrt.s	$f4,$f6
	sdxc1	$f4,$4($5)
	swxc1	$f4,$4($5)

	# test mips5 instructions.

	.set arch=mips5

	abs.ps		$f0, $f2
	add.ps		$f2, $f4, $f6
	alnv.ps		$f6, $f8, $f10, $3
	c.eq.ps		$f8, $f10
	c.eq.ps		$fcc2, $f10, $f12
	c.f.ps	    	$f8, $f10
	c.f.ps	    	$fcc2, $f10, $f12
	c.le.ps		$f8, $f10
	c.le.ps		$fcc2, $f10, $f12
	c.lt.ps		$f8, $f10
	c.lt.ps		$fcc2, $f10, $f12
	c.nge.ps	$f8, $f10
	c.nge.ps	$fcc2, $f10, $f12
	c.ngl.ps	$f8, $f10
	c.ngl.ps	$fcc2, $f10, $f12
	c.ngle.ps	$f8, $f10
	c.ngle.ps	$fcc2, $f10, $f12
	c.ngt.ps	$f8, $f10
	c.ngt.ps	$fcc2, $f10, $f12
	c.ole.ps	$f8, $f10
	c.ole.ps	$fcc2, $f10, $f12
	c.olt.ps	$f8, $f10
	c.olt.ps	$fcc2, $f10, $f12
	c.seq.ps	$f8, $f10
	c.seq.ps	$fcc2, $f10, $f12
	c.sf.ps		$f8, $f10
	c.sf.ps		$fcc2, $f10, $f12
	c.ueq.ps	$f8, $f10
	c.ueq.ps	$fcc2, $f10, $f12
	c.ule.ps	$f8, $f10
	c.ule.ps	$fcc2, $f10, $f12
	c.ult.ps	$f8, $f10
	c.ult.ps	$fcc2, $f10, $f12
	c.un.ps		$f8, $f10
	c.un.ps		$fcc2, $f10, $f12
	cvt.ps.s	$f12, $f14, $f16
	cvt.s.pl	$f16, $f18
	cvt.s.pu	$f18, $f20
	luxc1		$f20, $4($5)
	madd.ps		$f20, $f22, $f24, $f26
	mov.ps		$f24, $f26
	movf.ps		$f26, $f28, $fcc2
	movn.ps		$f26, $f28, $3
	movt.ps		$f28, $f30, $fcc4
	movz.ps		$f28, $f30, $5
	msub.ps		$f30, $f0, $f2, $f4
	mul.ps		$f2, $f4, $f6
	neg.ps		$f6, $f8
	nmadd.ps	$f6, $f8, $f10, $f12
	nmsub.ps	$f6, $f8, $f10, $f12
	pll.ps		$f10, $f12, $f14
	plu.ps		$f14, $f16, $f18
	pul.ps		$f16, $f18, $f20
	puu.ps		$f20, $f22, $f24
	sub.ps		$f22, $f24, $f26
	suxc1		$f26, $6($7)

	c.eq.ps		$fcc3, $f10, $f12	# warns
	movf.ps		$f26, $f28, $fcc3	# warns

	# test assembly of mips32 instructions

	.set arch=mips32

	# unprivileged CPU instructions

	clo     $1, $2
	clz     $3, $4
	madd    $5, $6
	maddu   $7, $8
	msub    $9, $10
	msubu   $11, $12
	mul     $13, $14, $15
	pref    4, ($16)
	pref    4, 32767($17)
	pref    4, -32768($18)
	ssnop

	# unprivileged coprocessor instructions.
	# these tests use cp2 to avoid other (cp0, fpu, prefetch) opcodes.

	bc2f    text_label
	nop
	bc2fl   text_label
	nop
	bc2t    text_label
	nop
	bc2tl   text_label
	nop
	# XXX other BCzCond encodings not currently expressable
	cfc2    $1, $2
	cop2    0x1234567               # disassembles as c2 ...
	ctc2    $2, $3
	mfc2    $3, $4
	mfc2    $4, $5, 0               # disassembles without sel
	mfc2    $5, $6, 7
	mtc2    $6, $7
	mtc2    $7, $8, 0               # disassembles without sel
	mtc2    $8, $9, 7
	
	# privileged instructions

	cache   5, ($1)
	cache   5, 32767($2)
	cache   5, -32768($3)
	eret
	tlbp
	tlbr
	tlbwi
	tlbwr
	wait
	wait    0                       # disassembles without code
	wait    0x56789

	# For a while break for the mips32 ISA interpreted a single argument
	# as a 20-bit code, placing it in the opcode differently to
	# traditional ISAs.  This turned out to cause problems, so it has
	# been removed.  This test is to assure consistent interpretation.
	break
	break   0                       # disassembles without code
	break	0x345
	break	0x48,0x345		# this still specifies a 20-bit code

	# Instructions in previous ISAs or CPUs which are now slightly
	# different.
	sdbbp
	sdbbp   0                       # disassembles without code
	sdbbp   0x56789

	# test assembly of mips32r2 instructions

	.set arch=mips32r2

	# unprivileged CPU instructions

	ehb

	ext	$4, $5, 6, 8

	ins	$4, $5, 6, 8

	jalr.hb	$8
	jalr.hb $20, $9

	jr.hb	$8

	# Note, further testing of rdhwr is done in hwr-names-mips32r2.d
	rdhwr	$10, $0
	rdhwr	$11, $1
	rdhwr	$12, $2
	rdhwr	$13, $3
	rdhwr	$14, $4
	rdhwr	$15, $5

	# This file checks that in fact HW rotate will
	# be used for this arch, and checks assembly
	# of the official MIPS mnemonics.  (Note that disassembly
	# uses the traditional "ror" and "rorv" mnemonics.)
	# Additional rotate tests are done by rol-hw.d.
	rotl	$25, $10, 4
	rotr	$25, $10, 4
	rotl	$25, $10, $4
	rotr	$25, $10, $4
	rotrv	$25, $10, $4

	seb	$7
	seb	$8, $10

	seh	$7
	seh	$8, $10

	synci	0x5555($10)

	wsbh	$7
	wsbh	$8, $10

	# cp0 instructions

	di
	di	$0
	di	$10

	ei
	ei	$0
	ei	$10

	rdpgpr	$10, $25

	wrpgpr	$10, $25

	# FPU (cp1) instructions
	#
	# Even registers are supported w/ 32-bit FPU, odd
	# registers supported only for 64-bit FPU.
	# Only the 32-bit FPU instructions are tested here.
     
	mfhc1	$17, $f0
	mthc1	$17, $f0

	# cp2 instructions

	mfhc2	$17, 0x5555
	mthc2	$17, 0x5555

	.set arch=mips64

	# test assembly of mips64 instructions

	# unprivileged CPU instructions

	dclo    $1, $2
	dclz    $3, $4

	# unprivileged coprocessor instructions.
	# these tests use cp2 to avoid other (cp0, fpu, prefetch) opcodes.

	dmfc2   $3, $4
	dmfc2   $4, $5, 0               # disassembles without sel
	dmfc2   $5, $6, 7
	dmtc2   $6, $7
	dmtc2   $7, $8, 0               # disassembles without sel
	dmtc2   $8, $9, 7

	.set arch=vr4111

	dmadd16	$4,$5
	madd16	$5,$6

	.set arch=vr4120

	# Include mflos to check for nop insertion.
	mflo        $4
	dmacc       $4,$5,$6
	dmacchi     $4,$5,$6
	dmacchis    $4,$5,$6
	dmacchiu    $4,$5,$6
	dmacchius   $4,$5,$6
	dmaccs      $4,$5,$6
	dmaccu      $4,$5,$6
	dmaccus     $4,$5,$6
	mflo        $4
	macc        $4,$5,$6
	macchi      $4,$5,$6
	macchis     $4,$5,$6
	macchiu     $4,$5,$6
	macchius    $4,$5,$6
	maccs       $4,$5,$6
	maccu       $4,$5,$6
	maccus      $4,$5,$6

	.set arch=vr5400

	/* Integer instructions.  */

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

	.set arch=vr5500

	/* Prefetch instructions.  */
        # We don't test pref because currently the disassembler will
        # disassemble it as lwc3.  lwc3 is correct for mips1 to mips3,
        # while pref is correct for mips4.  Unfortunately, the
        # disassembler does not know which architecture it is
        # disassembling for.
        # pref  4,0($4)

        prefx   4,$4($5)

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

.set arch=default

# make objdump print ...
	.space 8

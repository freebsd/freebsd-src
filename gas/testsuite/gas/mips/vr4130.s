	.macro	check2 insn
	mflo	$2
	\insn	$3,$3
	.endm

	.macro	check3 insn
	mfhi	$2
	\insn	$0,$3,$3
	.endm

	.macro	main func

	.ent	\func
	.type	\func,@function
\func:

	# PART A
	#
	# Check that mfhis and mflos in .set noreorder blocks are not
	# considered.

	.set	noreorder
	mfhi	$2
	.set	reorder
	mult	$3,$3

	.set	noreorder
	mflo	$2
	.set	reorder
	mult	$3,$3

	# PART B
	#
	# Check for simple instances.

	mfhi	$2
	mult	$3,$3	# 4 nops

	mfhi	$2
	addiu	$3,1
	mult	$4,$4	# 3 nops

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	mult	$5,$5	# 2 nops

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	mult	$6,$6	# 1 nop

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	addiu	$6,1
	mult	$7,$7	# 0 nops

	# PART C
	#
	# Check that no nops are inserted after the result has been read.

	mfhi	$2
	addiu	$2,1
	addiu	$3,1
	addiu	$4,1
	mult	$5,$5

	mfhi	$2
	addiu	$3,1
	addiu	$2,1
	addiu	$4,1
	mult	$5,$5

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	addiu	$2,1
	mult	$5,$5

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	mult	$2,$2

	# PART D
	#
	# Check that we still insert the usual interlocking nops in cases
	# where the VR4130 errata doesn't apply.

	mfhi	$2
	mult	$2,$2	# 2 nops

	mfhi	$2
	addiu	$2,1
	mult	$3,$3	# 1 nop

	mfhi	$2
	addiu	$3,1
	mult	$2,$2	# 1 nop

	# PART E
	#
	# Check for branches whose targets might be affected.

	mfhi	$2
	bnez	$3,1f	# 2 nops for normal mode, 3 for mips16

	mfhi	$2
	addiu	$3,1
	bnez	$3,1f	# 1 nop for normal mode, 2 for mips16

	mfhi	$2
	addiu	$3,1
	addiu	$3,1
	bnez	$3,1f	# 0 nops for normal mode, 1 for mips16

	mfhi	$2
	addiu	$3,1
	addiu	$3,1
	addiu	$3,1
	bnez	$3,1f	# 0 nops

	# PART F
	#
	# As above, but with no dependencies between the branch and
	# the previous instruction.  The final branch can use the
	# preceding addiu as its delay slot.

	mfhi	$2
	addiu	$3,1
	bnez	$4,1f	# 1 nop for normal mode, 2 for mips16

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	bnez	$5,1f	# 0 nops for normal mode, 1 for mips16

	mfhi	$2
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	bnez	$6,1f	# 0 nops, fill delay slot in normal mode
1:

	# PART G
	#
	# Like part B, but check that intervening .set noreorders don't
	# affect the number of nops.

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	.set	reorder
	mult	$4,$4	# 3 nops

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	.set	reorder
	addiu	$4,1
	mult	$5,$5	# 2 nops

	mfhi	$2
	addiu	$3,1
	.set	noreorder
	addiu	$4,1
	.set	reorder
	mult	$5,$5	# 2 nops

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	addiu	$4,1
	.set	reorder
	mult	$5,$5	# 2 nops

	mfhi	$2
	addiu	$3,1
	.set	noreorder
	addiu	$4,1
	.set	reorder
	addiu	$5,1
	mult	$6,$6	# 1 nop

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	.set	reorder
	mult	$6,$6	# 1 nop

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	addiu	$6,1
	.set	reorder
	mult	$7,$7	# 0 nops

	# PART H
	#
	# Like part B, but the mult occurs in a .set noreorder block.

	mfhi	$2
	.set	noreorder
	mult	$3,$3	# 4 nops
	.set	reorder

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	mult	$4,$4	# 3 nops
	.set	reorder

	mfhi	$2
	addiu	$3,1
	.set	noreorder
	addiu	$4,1
	mult	$5,$5	# 2 nops
	.set	reorder

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	mult	$6,$6	# 1 nop
	.set	reorder

	mfhi	$2
	.set	noreorder
	addiu	$3,1
	addiu	$4,1
	addiu	$5,1
	addiu	$6,1
	mult	$7,$7	# 0 nops
	.set	reorder

	# PART I
	#
	# Check every affected multiplication and division instruction.

	check2	mult
	check2	multu
	check2	dmult
	check2	dmultu

	check3	div
	check3	divu
	check3	ddiv
	check3	ddivu

	.end	\func
	.endm

	.set	nomips16
	main	foo

	# PART J
	#
	# Check every affected multiply-accumulate instruction.

	check3	macc
	check3	macchi
	check3	macchis
	check3	macchiu
	check3	macchius
	check3	maccs
	check3	maccu
	check3	maccus

	check3	dmacc
	check3	dmacchi
	check3	dmacchis
	check3	dmacchiu
	check3	dmacchius
	check3	dmaccs
	check3	dmaccu
	check3	dmaccus

	# PART K
	#
	# Check that mtlo and mthi are exempt from the VR4130 errata,
	# although the usual interlocking delay applies.

	mflo	$2
	mtlo	$3

	mflo	$2
	mthi	$3

	mfhi	$2
	mtlo	$3

	mfhi	$2
	mthi	$3

	.set	mips16
	main	bar

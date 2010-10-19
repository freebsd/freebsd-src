#as: -mfix-vr4130 -march=vr4130 -mabi=o64
#objdump: -dz
#name: MIPS VR4130 workarounds

.*file format.*

Disassembly.*

.* <foo>:
#
# PART A
#
.*	mfhi	.*
.*	mult	.*
#
.*	mflo	.*
.*	mult	.*
#
# PART B
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART C
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART D
#
.*	mfhi	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
# PART E
#
.*	mfhi	.*
.*	nop
.*	nop
.*	bnez	.*
.*	nop
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	bnez	.*
.*	nop
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	bnez	.*
.*	nop
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	bnez	.*
.*	nop
#
# PART F
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	bnez	.*
.*	nop
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	bnez	.*
.*	nop
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	bnez	.*
.*	addiu	.*
#
# PART G
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART H
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	nop
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART I
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	multu	.*
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmult	.*
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmultu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	div	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	divu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	ddiv	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	ddivu	.*
#
# PART J
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	macc	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	macchi	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	macchis	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	macchiu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	macchius	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	maccs	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	maccu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	maccus	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmacc	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmacchi	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmacchis	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmacchiu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmacchius	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmaccs	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmaccu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmaccus	.*
#
# PART K
#
.*	mflo	.*
.*	nop
.*	nop
.*	mtlo	.*
#
.*	mflo	.*
.*	mthi	.*
#
.*	mfhi	.*
.*	mtlo	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	mthi	.*

.* <bar>:
#
# PART A
#
.*	mfhi	.*
.*	mult	.*
#
.*	mflo	.*
.*	mult	.*
#
# PART B
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART C
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART D
#
.*	mfhi	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
# PART E
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	bnez	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	bnez	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	bnez	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	bnez	.*
#
# PART F
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	bnez	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	bnez	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	bnez	.*
#
# PART G
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART H
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	nop
.*	nop
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	nop
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
.*	mfhi	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	addiu	.*
.*	mult	.*
#
# PART I
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	mult	.*
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	multu	.*
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmult	.*
#
.*	mflo	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	dmultu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	div	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	divu	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	ddiv	.*
#
.*	mfhi	.*
.*	nop
.*	nop
.*	nop
.*	nop
.*	ddivu	.*
#pass

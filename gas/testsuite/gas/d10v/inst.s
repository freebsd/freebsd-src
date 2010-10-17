# test all instructions. FIXME: many instructions missing.

start:
	sachi	r1, a0
	sac	r0, a0
	#
	# disassembler test. sachi&sac should not 
	# be confused with rachi&rac
	#
	rachi	r1, a0, -0x2
	rac	r0, a0, -0x2
	slae	a0, r3
	ld	r1, @0x0800
	ld2w	r0, @0x0800
	st2w	r0, @0x0800
	st	r1, @0x0800

# VLIW syntax test
	nop
	nop
	nop	->	nop
	nop	||	nop
	nop	<-	nop

# try changing sections
	not	r1
	.section .foo
	add3	r10,r12,6
	.text
	not	r2
	nop

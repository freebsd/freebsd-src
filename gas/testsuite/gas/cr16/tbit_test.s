        .text
        .global main
main:
	##################
	# tbit uimm4, reg
	#################
	tbit	$0,r0
	tbit	$1,r1
	tbit	$2,r2
	tbit	$3,r3
	tbit	$4,r4
	tbit	$5,r5
	tbit	$6,r6
	tbit	$7,r7
	tbit	$8,r8
	tbit	$9,r9
	tbit	$10,r10
	tbit	$11,r11
	tbit	$12,r12
	tbit	$13,r13
#	tbit	$14,r14 // Add error check for these INST
#	tbit	$15,r15 // Add error check for these INST
	##################
	# tbit reg, reg
	#################
	tbit	r0,r0
	tbit	r1,r1
	tbit	r2,r2
	tbit	r3,r3
	tbit	r4,r4
	tbit	r5,r5
	tbit	r6,r6
	tbit	r7,r7
	tbit	r8,r8
	tbit	r9,r9
	tbit	r10,r10
	tbit	r11,r11
	tbit	r12,r12
	tbit	r13,r13
#	tbit	r14,r14 // Add error check for these INST
#	tbit	r15,r15 // Add error check for these INST

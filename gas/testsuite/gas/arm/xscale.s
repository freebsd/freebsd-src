	.text
	.global foo
foo:
	mia 	acc0, r0, r1
	mialt 	acc0, r14, r13

	miaph	acc0, r2, r4
	miaphne	acc0, r5, r6

	miaBB	acc0, r7, r8
	miaBT	acc0, r9, r10
	miaTB	acc0, r12, r11
	miaTT	acc0, r0, r0
	
	mar	acc0, r1, r1
	margt	acc0, r2, r12
	
	mra	r3, r4, acc0
	mra	r5, r8, acc0

	pld	[r0]
	pld	[r1, #0x789]
	pld	[r2, r3]
	pld	[r4, -r5, lsl #5]

	ldrd	r0, [r1]
	ldreqd	r2, [r3, #0x78]
	ldrltd	r4, [r5, -r6]
	strd	r8, [r10,#-0x89]!
	strald  r0, [r12, +r13]!
	strlod	r2, [r14], #+0x010
	strvcd	r4, [r6], r8

	ldr	r0, [r1]
	str	r2, [r3]

	msr	cpsr_ctl, #0x11

	# Add two nop instructions to ensure that the
	# output is 32-byte aligned as required for arm-aout.
	nop
	nop

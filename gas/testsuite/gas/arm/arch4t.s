.text
.align 0
	
	bx	r0
	bxeq	r1

foo:	
	ldrh	r3, foo
	ldrsh	r4, [r5]
	ldrsb	r4, [r1, r3]
	ldrsh	r1, [r4, r4]!
	ldreqsb	r1, [r5, -r3]
	ldrneh	r2, [r6], r7
	ldrccsh r2, [r7], +r8
	ldrsb	r2, [r3, #255]
	ldrsh	r1, [r4, #-250]
	ldrsb	r1, [r5, #+240]

	strh	r2, bar
	strneh	r3, [r3]

	msr	CPSR_f, #2
	msr	CPSR_c, r3
	msr	CPSR_x, r4
	msr	CPSR_s, r5
	msr	CPSR_f, r6
	msr	CPSR_all, r7
	
	msr	SPSR_f, #4
	msr	SPSR_c, r8
	msr	SPSR_x, r9
	msr	SPSR_s, r10
	msr	SPSR_f, r11
	msr	SPSR_all, r12
bar:

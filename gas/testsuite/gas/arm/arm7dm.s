.text
.align 0
	
	smull	r0, r1, r2, r3
	umull	r0, r1, r2, r3
	smlal	r0, r1, r2, r3
	umlal	r0, r1, r4, r3

	smullne	r0, r1, r3, r4
	smulls	r1, r0, r9, r11
	umlaleqs r2, r9, r4, r9
	smlalge r14, r10, r8, r14

	msr	CPSR_x, #0	@ This used to be illegal, but rev 2 of the ARM ARM allows it.

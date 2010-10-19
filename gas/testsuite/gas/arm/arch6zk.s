.text
.align 0

label:
	# ARMV6K instructions
	clrex
	ldrexb		r4, [r12]
	ldrexbne	r12, [r4]
	ldrexd		r4, [r12]
	ldrexdne	r12, [r4]
	ldrexh		r4, [r12]
	ldrexhne	r12, [r4]
	nop 		{128}
	nopne		{127}
	sev
	strexb		r4, r12, [r7]
	strexbne	r12, r4, [r8]
	strexd		r4, r12, [r7]
	strexdne	r12, r4, [r8]
	strexh		r4, r12, [r7]
	strexhne	r12, r4, [r8]
	wfe
	wfi
	yield
	# ARMV6Z instructions
	smc 0xec31
	smcne 0x13ce

	# Add three nop instructions to ensure that the 
	# output is 32-byte aligned as required for arm-aout.
	nop
	nop
	nop

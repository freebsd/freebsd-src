.text
.align 0
	
	mrs	r8, cpsr
	mrs	r2, spsr

	msr	cpsr, r1
	msrne	cpsr_flg, #0xf0000000
	msr	spsr_flg, r8
	msr	spsr_all, r9

	mrs	r8, CPSR
	mrs	r2, SPSR

	msr	CPSR, r1
	msrne	CPSR_flg, #0xf0000000
	msr	SPSR_flg, r8
	msr	SPSR_all, r9


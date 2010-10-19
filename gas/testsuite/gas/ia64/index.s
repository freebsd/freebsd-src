z == zero
zero == r0

.text
_start:
	ld8	r2 = [ar.lc]
	ld8	r3 = [1]
	ld8	r4 = [-1]
	ld8	r5 = [xyz]
	ld8	r6 = [zero]
	ld8	r7 = [z]

	mov	r2 = cpuid[ar.lc]
	mov	r3 = cpuid[1]
	mov	r4 = cpuid[-1]
	mov	r5 = cpuid[xyz]
	mov	r6 = cpuid[zero]
	mov	r7 = cpuid[z]

	mov	r2 = b0[ar.lc]
	mov	r3 = b0[1]
	mov	r4 = b0[-1]
	mov	r5 = b0[xyz]
	mov	r6 = b0[zero]
	mov	r7 = b0[z]

	mov	r2 = xyz[ar.lc]
	mov	r3 = xyz[1]
	mov	r4 = xyz[-1]
	mov	r5 = xyz[xyz]
	mov	r6 = xyz[zero]
	mov	r7 = xyz[z]

.regstk 0, 8, 0, 8
.rotr reg[8]

	mov	r2 = reg[ar.lc]
	mov	r3 = reg[1]
	mov	r4 = reg[-1]
	mov	r5 = reg[xyz]
	mov	r6 = reg[zero]
	mov	r7 = reg[z]

	mov	r2 = cpuid[ar.lc]
	mov	r3 = cpuid[1]
	mov	r4 = cpuid[-1]
	mov	r5 = cpuid[xyz]
	mov	r6 = cpuid[zero]
	mov	r7 = cpuid[z]

	mov	r2 = b0[ar.lc]
	mov	r3 = b0[1]
	mov	r4 = b0[-1]
	mov	r5 = b0[xyz]
	mov	r6 = b0[zero]
	mov	r7 = b0[z]

	mov	r2 = xyz[ar.lc]
	mov	r3 = xyz[1]
	mov	r4 = xyz[-1]
	mov	r5 = xyz[xyz]
	mov	r6 = xyz[zero]
	mov	r7 = xyz[z]

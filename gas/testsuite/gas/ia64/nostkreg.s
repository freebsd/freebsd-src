_start:
	mov		r5 = in00
	mov		r6 = loc96
	.regstk 2, 6, 2, 8
	.rotr in0I[2], loc1L[2], out2O[2]
	mov		r7 = in0I[0]
	mov		r8 = loc1L[0]
	mov		r9 = out2O[0]
	br.ret.sptk	rp

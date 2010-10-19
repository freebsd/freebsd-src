	movhi	hi(lo16_nocarry),r0,r1
	addi	lo(lo16_nocarry),r1,r2
	ld.bu	lo(lo16_nocarry)[r1],r2
	movhi	hi(lo16_nocarry + 0x18200),r0,r1
	addi	lo(lo16_nocarry + 0x18200),r1,r2
	ld.bu	lo(lo16_nocarry + 0x18200)[r1],r2
	movhi	hi(split_lo16_nocarry),r0,r1
	ld.bu	lo(split_lo16_nocarry)[r1],r2
	addi	lo(split_lo16_nocarry),r1,r2
	movhi	hi(split_lo16_nocarry + 0x18200),r0,r1
	ld.bu	lo(split_lo16_nocarry + 0x18200)[r1],r2
	addi	lo(split_lo16_nocarry + 0x18200),r1,r2
	movhi	hi(lo16_carry),r0,r1
	addi	lo(lo16_carry),r1,r2
	ld.bu	lo(lo16_carry)[r1],r2
	movhi	hi(split_lo16_carry),r0,r1
	ld.bu	lo(split_lo16_carry)[r1],r2
	addi	lo(split_lo16_carry),r1,r2
	movhi	hi(odd),r0,r1
	ld.bu	lo(odd)[r1],r2

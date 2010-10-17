.text
.align 0
	
	bsh	r1, r2
	bsw	r3, r4
	callt	5
	clr1	r7, [r8]
	cmov	nz, 22, r2, r3
	cmov	nz, r1, r2, r3
	ctret	
	dbret
	dbtrap		
	dispose	7, {r24}
	dispose	7, {r25 - r27}, r5
	div	r1, r2, r3
	divh	r4, r5, r6
	divhu	r7, r8, r9
	divu	r10, r11, r12
	hsw	r13, r14
	ld.bu	13 [r1], r2
	ld.hu	16 [r3], r4
	mov	0x12345678, r1
	mul	5, r2, r3
	mul	r1, r2, r3
	mulu	r4, r5, r6
	mulu	35, r5, r6
	not1	r9, [r10]
	prepare	{r24}, 20
	prepare	{r25 - r27}, 20, sp
	set1	r9, [r1]
	sasf	nz, r8
	sld.bu	0 [ep], r4
	sld.hu	14 [ep], r5
	sxb	r1
	sxh	r2
	tst1	r0, [r31]
	zxb	r3
	zxh	r4

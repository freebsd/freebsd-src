	adds r25 = -0x2001, r10
	adds r26 = 0x2000, r10

	addl r37 = -0x200001, r1
	addl r38 = 0x200000, r1
	addl r30 = 0, r10

	sub r2 = 128, r3
	sub r3 = -129, r4

	and r8 = 129, r9
	and r3 = -129, r4
	
	or r8 = 129, r9
	or r3 = -129, r4
	
	xor r8 = 129, r9
	xor r3 = -129, r4
	
	andcm r8 = 129, r9
	andcm r3 = -129, r4

        cmp4.lt.or p2, p3 = r1, r4
        cmp4.lt.or p2, p3 = 1, r4

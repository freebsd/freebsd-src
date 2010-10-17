.text
.align 0
	cdp	p1, 4, cr1, cr2, cr3
	cdpeq	4, 3, c1, c4, cr5, 5

	ldc	5, cr9, [r3]
	ldcl	1, cr14, [r1, #32]
	ldcmi	0, cr0, [r2, #1020]!
	ldcpll	p7, c1, [r3], #64
	ldc	p0, c8, foo
foo:

	stc	5, cr0, [r3]
	stcl	3, cr15, [r0, #8]
	stceq	p4, cr12, [r2, #100]!
	stccc	p6, c8, [r4], #48
	stc	p1, c7, bar
bar:

	mrc	2, 3, r5, c1, c2
	mrcge	p4, 5, r15, cr1, cr2, 7

	mcr	p7, 1, r15, cr1, cr1
	mcrlt	5, 1, r8, cr2, cr9, 0

	@ The following patterns test Addressing Mode 5 "Unindexed"
	
        ldc     3,   c7, [r0], {0}
        stc     p14, c6, [r1], {1}
        ldc2    5,   c5, [r2], {2}
        stc2    p6,  c4, [r3], {3}
        ldcl    7,   c3, [r4], {4}
        stcl    p8,  c2, [r5], {5}
        ldc2l   9,   c1, [r6], {6}
        stc2l   p10, c0, [r7], {7}
        ldcl    11,  c8, [r8], {255}
        stcl    p12, c9, [r9], {254}
        mrrc    13,   0, r7, r0, cr4
        mcrr    p14,  0, r7, r0, cr5
        mrrc    15,  15, r7, r0, cr15
        mcrr    p14, 15, r7, r0, cr14

	# Extra instructions to allow for code alignment in arm-aout target.
	nop
	nop

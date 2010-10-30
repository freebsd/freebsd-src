@ Tests that are supposed to fail during parsing of LDR group relocations.

	.text

@ No NC variants exist for the LDR relocations.

	ldr	r0, [r0, #:pc_g0_nc:(f)]
	ldr	r0, [r0, #:pc_g1_nc:(f)]
	ldr	r0, [r0, #:sb_g0_nc:(f)]
	ldr	r0, [r0, #:sb_g1_nc:(f)]

	str	r0, [r0, #:pc_g0_nc:(f)]
	str	r0, [r0, #:pc_g1_nc:(f)]
	str	r0, [r0, #:sb_g0_nc:(f)]
	str	r0, [r0, #:sb_g1_nc:(f)]

	ldrb	r0, [r0, #:pc_g0_nc:(f)]
	ldrb	r0, [r0, #:pc_g1_nc:(f)]
	ldrb	r0, [r0, #:sb_g0_nc:(f)]
	ldrb	r0, [r0, #:sb_g1_nc:(f)]

	strb	r0, [r0, #:pc_g0_nc:(f)]
	strb	r0, [r0, #:pc_g1_nc:(f)]
	strb	r0, [r0, #:sb_g0_nc:(f)]
	strb	r0, [r0, #:sb_g1_nc:(f)]

@ Instructions with a gibberish relocation code.

	ldr	r0, [r0, #:foo:(f)]
	str	r0, [r0, #:foo:(f)]
	ldrb	r0, [r0, #:foo:(f)]
	strb	r0, [r0, #:foo:(f)]


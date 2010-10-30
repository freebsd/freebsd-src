@ Tests that are supposed to fail during parsing of LDRS group relocations.

	.text

@ No NC variants exist for the LDRS relocations.

	ldrd	r0, [r0, #:pc_g0_nc:(f)]
	ldrd	r0, [r0, #:pc_g1_nc:(f)]
	ldrd	r0, [r0, #:sb_g0_nc:(f)]
	ldrd	r0, [r0, #:sb_g1_nc:(f)]

	strd	r0, [r0, #:pc_g0_nc:(f)]
	strd	r0, [r0, #:pc_g1_nc:(f)]
	strd	r0, [r0, #:sb_g0_nc:(f)]
	strd	r0, [r0, #:sb_g1_nc:(f)]

	ldrh	r0, [r0, #:pc_g0_nc:(f)]
	ldrh	r0, [r0, #:pc_g1_nc:(f)]
	ldrh	r0, [r0, #:sb_g0_nc:(f)]
	ldrh	r0, [r0, #:sb_g1_nc:(f)]

	strh	r0, [r0, #:pc_g0_nc:(f)]
	strh	r0, [r0, #:pc_g1_nc:(f)]
	strh	r0, [r0, #:sb_g0_nc:(f)]
	strh	r0, [r0, #:sb_g1_nc:(f)]

	ldrsh	r0, [r0, #:pc_g0_nc:(f)]
	ldrsh	r0, [r0, #:pc_g1_nc:(f)]
	ldrsh	r0, [r0, #:sb_g0_nc:(f)]
	ldrsh	r0, [r0, #:sb_g1_nc:(f)]

	ldrsb	r0, [r0, #:pc_g0_nc:(f)]
	ldrsb	r0, [r0, #:pc_g1_nc:(f)]
	ldrsb	r0, [r0, #:sb_g0_nc:(f)]
	ldrsb	r0, [r0, #:sb_g1_nc:(f)]

@ Instructions with a gibberish relocation code.
	ldrd	r0, [r0, #:foo:(f)]
	strd	r0, [r0, #:foo:(f)]
	ldrh	r0, [r0, #:foo:(f)]
	strh	r0, [r0, #:foo:(f)]
	ldrsh	r0, [r0, #:foo:(f)]
	ldrsb	r0, [r0, #:foo:(f)]


@ VFP2 Additional instructions
	.text
	.global F
F:
	@ First we test the basic syntax and bit patterns of the opcodes.
	@ Use a combination of r5, r10, s15, s17, d0 and d15 to exercise
	@ the full register bitpatterns

	fmdrr d0, r5, r10
	fmrrd r5, r10, d0
	fmsrr {s15, s16}, r5, r10
	fmrrs r5, r10, {s15, s16}

	fmdrr d15, r10, r5
	fmrrd r10, r5, d15
	fmsrr {s17, s18}, r10, r5
	fmrrs r10, r5, {s17, s18}


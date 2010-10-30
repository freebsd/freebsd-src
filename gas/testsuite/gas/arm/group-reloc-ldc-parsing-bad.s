@ Tests for LDC group relocations that are meant to fail during parsing.

	.macro ldctest insn reg

	\insn	0, \reg, [r0, #:pc_g0_nc:(sym)]
	\insn	0, \reg, [r0, #:pc_g1_nc:(sym)]
	\insn	0, \reg, [r0, #:sb_g0_nc:(sym)]
	\insn	0, \reg, [r0, #:sb_g1_nc:(sym)]

	\insn	0, \reg, [r0, #:foo:(sym)]

	.endm

	.macro ldctest2 insn reg

	\insn	\reg, [r0, #:pc_g0_nc:(sym)]
	\insn	\reg, [r0, #:pc_g1_nc:(sym)]
	\insn	\reg, [r0, #:sb_g0_nc:(sym)]
	\insn	\reg, [r0, #:sb_g1_nc:(sym)]

	\insn	\reg, [r0, #:foo:(sym)]

	.endm

	ldctest ldc c0
	ldctest ldcl c0
	ldctest ldc2 c0
	ldctest ldc2l c0

	ldctest stc c0
	ldctest stcl c0
	ldctest stc2 c0
	ldctest stc2l c0

	.fpu 	fpa

	ldctest2 ldfs f0
	ldctest2 stfs f0
	ldctest2 ldfd f0
	ldctest2 stfd f0
	ldctest2 ldfe f0
	ldctest2 stfe f0
	ldctest2 ldfp f0
	ldctest2 stfp f0

	.fpu	vfp

	ldctest2 flds s0
	ldctest2 fsts s0

	ldctest2 fldd d0
	ldctest2 fstd d0

	ldctest2 vldr d0		FIXME
	ldctest2 vstr d0

	.cpu	ep9312

	ldctest2 cfldrs mvf0
	ldctest2 cfstrs mvf0
	ldctest2 cfldrd mvd0
	ldctest2 cfstrd mvd0
	ldctest2 cfldr32 mvfx0
	ldctest2 cfstr32 mvfx0
	ldctest2 cfldr64 mvdx0
	ldctest2 cfstr64 mvdx0


@ LDC group relocation tests that are supposed to fail during encoding.

	.text

@ LDC/LDCL/LDC2/LDC2L/STC/STCL/STC2/STC2L

	.macro ldctest load store cst

	\load	0, c0, [r0, #:pc_g0:(f + \cst)]
	\load	0, c0, [r0, #:pc_g1:(f + \cst)]
	\load	0, c0, [r0, #:pc_g2:(f + \cst)]

	\load	0, c0, [r0, #:sb_g0:(f + \cst)]
	\load	0, c0, [r0, #:sb_g1:(f + \cst)]
	\load	0, c0, [r0, #:sb_g2:(f + \cst)]

	\store	0, c0, [r0, #:pc_g0:(f + \cst)]
	\store	0, c0, [r0, #:pc_g1:(f + \cst)]
	\store	0, c0, [r0, #:pc_g2:(f + \cst)]

	\store	0, c0, [r0, #:sb_g0:(f + \cst)]
	\store	0, c0, [r0, #:sb_g1:(f + \cst)]
	\store	0, c0, [r0, #:sb_g2:(f + \cst)]

	\load	0, c0, [r0, #:pc_g0:(f - \cst)]
	\load	0, c0, [r0, #:pc_g1:(f - \cst)]
	\load	0, c0, [r0, #:pc_g2:(f - \cst)]

	\load	0, c0, [r0, #:sb_g0:(f - \cst)]
	\load	0, c0, [r0, #:sb_g1:(f - \cst)]
	\load	0, c0, [r0, #:sb_g2:(f - \cst)]

	\store	0, c0, [r0, #:pc_g0:(f - \cst)]
	\store	0, c0, [r0, #:pc_g1:(f - \cst)]
	\store	0, c0, [r0, #:pc_g2:(f - \cst)]

	\store	0, c0, [r0, #:sb_g0:(f - \cst)]
	\store	0, c0, [r0, #:sb_g1:(f - \cst)]
	\store	0, c0, [r0, #:sb_g2:(f - \cst)]

	.endm

	ldctest ldc stc 0x1
	ldctest ldcl stcl 0x1
	ldctest ldc2 stc2 0x1
	ldctest ldc2l stc2l 0x1

	ldctest ldc stc 0x808
	ldctest ldcl stcl 0x808
	ldctest ldc2 stc2 0x808
	ldctest ldc2l stc2l 0x808

@ LDFS/STFS/LDFD/STFD/LDFE/STFE/LDFP/STFP

	.fpu	fpa

	.macro	fpa_test load store cst

	\load	f0, [r0, #:pc_g0:(f + \cst)]
	\load	f0, [r0, #:pc_g1:(f + \cst)]
	\load	f0, [r0, #:pc_g2:(f + \cst)]

	\load	f0, [r0, #:sb_g0:(f + \cst)]
	\load	f0, [r0, #:sb_g1:(f + \cst)]
	\load	f0, [r0, #:sb_g2:(f + \cst)]

	\store	f0, [r0, #:pc_g0:(f + \cst)]
	\store	f0, [r0, #:pc_g1:(f + \cst)]
	\store	f0, [r0, #:pc_g2:(f + \cst)]

	\store	f0, [r0, #:sb_g0:(f + \cst)]
	\store	f0, [r0, #:sb_g1:(f + \cst)]
	\store	f0, [r0, #:sb_g2:(f + \cst)]

	\load	f0, [r0, #:pc_g0:(f - \cst)]
	\load	f0, [r0, #:pc_g1:(f - \cst)]
	\load	f0, [r0, #:pc_g2:(f - \cst)]

	\load	f0, [r0, #:sb_g0:(f - \cst)]
	\load	f0, [r0, #:sb_g1:(f - \cst)]
	\load	f0, [r0, #:sb_g2:(f - \cst)]

	\store	f0, [r0, #:pc_g0:(f - \cst)]
	\store	f0, [r0, #:pc_g1:(f - \cst)]
	\store	f0, [r0, #:pc_g2:(f - \cst)]

	\store	f0, [r0, #:sb_g0:(f - \cst)]
	\store	f0, [r0, #:sb_g1:(f - \cst)]
	\store	f0, [r0, #:sb_g2:(f - \cst)]

	.endm

	fpa_test ldfs stfs 0x1
	fpa_test ldfd stfd 0x1
	fpa_test ldfe stfe 0x1
	fpa_test ldfp stfp 0x1

	fpa_test ldfs stfs 0x808
	fpa_test ldfd stfd 0x808
	fpa_test ldfe stfe 0x808
	fpa_test ldfp stfp 0x808

@ FLDS/FSTS

	.fpu	vfp

	.macro vfp_test load store reg cst

	\load	\reg, [r0, #:pc_g0:(f + \cst)]
	\load	\reg, [r0, #:pc_g1:(f + \cst)]
	\load	\reg, [r0, #:pc_g2:(f + \cst)]

	\load	\reg, [r0, #:sb_g0:(f + \cst)]
	\load	\reg, [r0, #:sb_g1:(f + \cst)]
	\load	\reg, [r0, #:sb_g2:(f + \cst)]

	\store	\reg, [r0, #:pc_g0:(f + \cst)]
	\store	\reg, [r0, #:pc_g1:(f + \cst)]
	\store	\reg, [r0, #:pc_g2:(f + \cst)]

	\store	\reg, [r0, #:sb_g0:(f + \cst)]
	\store	\reg, [r0, #:sb_g1:(f + \cst)]
	\store	\reg, [r0, #:sb_g2:(f + \cst)]

	\load	\reg, [r0, #:pc_g0:(f - \cst)]
	\load	\reg, [r0, #:pc_g1:(f - \cst)]
	\load	\reg, [r0, #:pc_g2:(f - \cst)]

	\load	\reg, [r0, #:sb_g0:(f - \cst)]
	\load	\reg, [r0, #:sb_g1:(f - \cst)]
	\load	\reg, [r0, #:sb_g2:(f - \cst)]

	\store	\reg, [r0, #:pc_g0:(f - \cst)]
	\store	\reg, [r0, #:pc_g1:(f - \cst)]
	\store	\reg, [r0, #:pc_g2:(f - \cst)]

	\store	\reg, [r0, #:sb_g0:(f - \cst)]
	\store	\reg, [r0, #:sb_g1:(f - \cst)]
	\store	\reg, [r0, #:sb_g2:(f - \cst)]

	.endm

	vfp_test flds fsts s0 0x1
	vfp_test flds fsts s0 0x808

@ FLDD/FSTD

	vfp_test fldd fstd d0 0x1
	vfp_test fldd fstd d0 0x808

@ VLDR/VSTR

	vfp_test vldr vstr d0 0x1
	vfp_test vldr vstr d0 0x808

@ CFLDRS/CFLDRD/CFLDR32/CFLDR64/CFSTRS/CFSTRD/CFSTR32/CFSTR64

	.cpu	ep9312

	vfp_test cfldrs cfstrs mvf0 0x1
	vfp_test cfldrd cfstrd mvd0 0x1
	vfp_test cfldr32 cfstr32 mvfx0 0x1
	vfp_test cfldr64 cfstr64 mvdx0 0x1

	vfp_test cfldrs cfstrs mvf0 0x808
	vfp_test cfldrd cfstrd mvd0 0x808
	vfp_test cfldr32 cfstr32 mvfx0 0x808
	vfp_test cfldr64 cfstr64 mvdx0 0x808


# fzchkl, fzchks, faddp, faddz

	.text

	# Pipelined, without dual bit
	pfzchkl	%f0,%f2,%f8
	pfzchkl	%f2,%f4,%f16
	pfzchkl	%f4,%f6,%f13
	pfzchkl	%f8,%f10,%f18

	pfzchks	%f12,%f14,%f30
	pfzchks	%f16,%f18,%f20
	pfzchks	%f20,%f22,%f28
	pfzchks	%f24,%f26,%f30

	pfaddp	%f0,%f2,%f8
	pfaddp	%f2,%f4,%f16
	pfaddp	%f4,%f6,%f13
	pfaddp	%f8,%f10,%f18

	pfaddz	%f12,%f14,%f30
	pfaddz	%f16,%f18,%f20
	pfaddz	%f20,%f22,%f28
	pfaddz	%f24,%f26,%f30

	# Non-pipelined, without dual bit
	fzchkl	%f12,%f2,%f4
	fzchkl	%f22,%f4,%f2
	fzchkl	%f4,%f6,%f18
	fzchkl	%f8,%f10,%f28

	fzchks	%f12,%f14,%f6
	fzchks	%f16,%f18,%f20
	fzchks	%f20,%f22,%f28
	fzchks	%f24,%f26,%f30

	faddp	%f12,%f2,%f4
	faddp	%f22,%f4,%f2
	faddp	%f4,%f6,%f18
	faddp	%f8,%f10,%f28

	faddz	%f12,%f14,%f6
	faddz	%f16,%f18,%f20
	faddz	%f20,%f22,%f28
	faddz	%f24,%f26,%f30

	# Pipelined, with dual bit
	d.pfzchkl	%f0,%f2,%f18
	nop
	d.pfzchkl	%f2,%f4,%f12
	nop
	d.pfzchkl	%f4,%f6,%f30
	nop
	d.pfzchkl	%f8,%f10,%f4
	nop

	d.pfzchks	%f12,%f14,%f14
	nop
	d.pfzchks	%f16,%f18,%f6
	nop
	d.pfzchks	%f20,%f22,%f10
	nop
	d.pfzchks	%f24,%f26,%f8
	nop

	d.pfaddp	%f0,%f2,%f18
	nop
	d.pfaddp	%f2,%f4,%f0
	nop
	d.pfaddp	%f4,%f6,%f30
	nop
	d.pfaddp	%f8,%f10,%f4
	nop

	d.pfaddz	%f12,%f14,%f14
	nop
	d.pfaddz	%f16,%f18,%f6
	nop
	d.pfaddz	%f20,%f22,%f10
	nop
	d.pfaddz	%f24,%f26,%f8
	nop

	# Non-pipelined, with dual bit
	d.fzchkl	%f0,%f2,%f10
	nop
	d.fzchkl	%f2,%f4,%f18
	nop
	d.fzchkl	%f4,%f6,%f12
	nop
	d.fzchkl	%f8,%f10,%f14
	nop

	d.fzchks	%f12,%f14,%f16
	nop
	d.fzchks	%f16,%f18,%f12
	nop
	d.fzchks	%f20,%f22,%f16
	nop
	d.fzchks	%f24,%f26,%f30
	nop

	d.faddp	%f0,%f2,%f10
	nop
	d.faddp	%f2,%f4,%f18
	nop
	d.faddp	%f4,%f6,%f12
	nop
	d.faddp	%f8,%f10,%f14
	nop

	d.faddz	%f12,%f14,%f16
	nop
	d.faddz	%f16,%f18,%f12
	nop
	d.faddz	%f20,%f22,%f16
	nop
	d.faddz	%f24,%f26,%f30
	nop

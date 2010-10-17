# form and pform

	.text

	# pform, no dual bit
	pform	%f0,%f2
	pform	%f2,%f4
	pform	%f4,%f6
	pform	%f8,%f10
	pform	%f12,%f14
	pform	%f16,%f18
	pform	%f20,%f22
	pform	%f24,%f26
	pform	%f28,%f30

	# form, no dual bit
	form	%f0,%f2
	form	%f2,%f4
	form	%f4,%f6
	form	%f8,%f10
	form	%f12,%f14
	form	%f16,%f18
	form	%f20,%f22
	form	%f24,%f26
	form	%f28,%f30

	# pform, with dual bit
	d.pform	%f0,%f2
	nop
	d.pform	%f2,%f4
	nop
	d.pform	%f4,%f6
	nop
	d.pform	%f8,%f10
	nop
	d.pform	%f12,%f14
	nop
	d.pform	%f16,%f18
	nop
	d.pform	%f20,%f22
	nop
	d.pform	%f24,%f26
	nop
	d.pform	%f28,%f30
	nop

	# form, with dual bit
	d.form	%f0,%f2
	nop
	d.form	%f2,%f4
	nop
	d.form	%f4,%f6
	nop
	d.form	%f8,%f10
	nop
	d.form	%f12,%f14
	nop
	d.form	%f16,%f18
	nop
	d.form	%f20,%f22
	nop
	d.form	%f24,%f26
	nop
	d.form	%f28,%f30
	nop


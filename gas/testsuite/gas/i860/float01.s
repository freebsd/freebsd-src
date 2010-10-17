# fadd, fsub, fmul, pfmul3, fmlow

	.text

	# Non-pipelined, without dual bit 
	fadd.ss	%f0,%f1,%f2
	fadd.sd	%f2,%f3,%f4
	fadd.dd	%f6,%f8,%f10

	fsub.ss	%f5,%f6,%f7
	fsub.sd	%f8,%f9,%f10
	fsub.dd	%f12,%f14,%f16

	fmul.ss	%f11,%f12,%f13
	fmul.sd	%f14,%f15,%f16
	fmul.dd	%f18,%f20,%f22

	fmlow.dd	%f22,%f24,%f26

	# Pipelined, without dual bit 
	pfadd.ss	%f14,%f15,%f16
	pfadd.sd	%f17,%f18,%f20
	pfadd.dd	%f22,%f24,%f26

	pfsub.ss	%f20,%f21,%f22
	pfsub.sd	%f23,%f24,%f26
	pfsub.dd	%f28,%f30,%f2

	pfmul.ss	%f27,%f28,%f29
	pfmul.sd	%f30,%f31,%f4
	pfmul.dd	%f6,%f0,%f8

	pfmul3.dd	%f2,%f4,%f30

	# Non-pipelined, with dual bit 
	d.fadd.ss	%f0,%f1,%f2
	nop
	d.fadd.sd	%f2,%f3,%f4
	nop
	d.fadd.dd	%f6,%f8,%f10
	nop

	d.fsub.ss	%f5,%f6,%f7
	nop
	d.fsub.sd	%f8,%f9,%f10
	nop
	d.fsub.dd	%f12,%f14,%f16
	nop

	d.fmul.ss	%f11,%f12,%f13
	nop
	d.fmul.sd	%f14,%f15,%f16
	nop
	d.fmul.dd	%f18,%f20,%f22
	nop

	d.fmlow.dd	%f8,%f10,%f12
	nop

	# Pipelined, with dual bit 
	d.pfadd.ss	%f14,%f15,%f16
	nop
	d.pfadd.sd	%f17,%f18,%f20
	nop
	d.pfadd.dd	%f22,%f24,%f26
	nop

	d.pfsub.ss	%f20,%f21,%f22
	nop
	d.pfsub.sd	%f23,%f24,%f26
	nop
	d.pfsub.dd	%f28,%f30,%f2
	nop

	d.pfmul.ss	%f27,%f28,%f29
	nop
	d.pfmul.sd	%f30,%f31,%f4
	nop
	d.pfmul.dd	%f6,%f0,%f8
	nop

	d.pfmul3.dd	%f2,%f4,%f30
	nop


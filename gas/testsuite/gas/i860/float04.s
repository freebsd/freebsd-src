# fxfr, ixfr, fiadd, fisub

	.text

	# ixfr, fxfr
	fxfr	%f1,%r3
	fxfr	%f8,%r30
	fxfr	%f31,%r18

	ixfr	%r9,%f31
	ixfr	%r23,%f16
	ixfr	%r0,%f0

	# Non-pipelined, without dual bit 
	fiadd.ss	%f0,%f1,%f2
	fiadd.dd	%f6,%f8,%f10

	fisub.ss	%f5,%f6,%f7
	fisub.dd	%f12,%f14,%f16

	# Pipelined, without dual bit 
	pfiadd.ss	%f14,%f15,%f16
	pfiadd.dd	%f22,%f24,%f26

	pfisub.ss	%f20,%f21,%f22
	pfisub.dd	%f28,%f30,%f2

	# Non-pipelined, with dual bit 
	d.fiadd.ss	%f0,%f1,%f2
	nop
	d.fiadd.dd	%f6,%f8,%f10
	nop

	d.fisub.ss	%f5,%f6,%f7
	nop
	d.fisub.dd	%f12,%f14,%f16
	nop

	# Pipelined, with dual bit 
	d.pfiadd.ss	%f14,%f15,%f16
	nop
	d.pfiadd.dd	%f22,%f24,%f26
	nop

	d.pfisub.ss	%f20,%f21,%f22
	nop
	d.pfisub.dd	%f28,%f30,%f2
	nop


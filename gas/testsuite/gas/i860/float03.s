# fix, ftrunc, pfgt, pfle, pfeq

	.text

	# Non-pipelined, without dual bit 
	fix.sd	%f2,%f4
	fix.dd	%f6,%f8

	ftrunc.sd	%f8,%f10
	ftrunc.dd	%f12,%f14

	# Pipelined, without dual bit 
	pfix.sd	%f30,%f14
	pfix.dd	%f24,%f2

	pftrunc.sd	%f8,%f10
	pftrunc.dd	%f12,%f14

	pfgt.ss %f0,%f1,%f2
	pfgt.dd %f6,%f8,%f10

	pfle.ss %f5,%f6,%f7
	pfle.dd %f12,%f14,%f16

	pfeq.ss %f11,%f12,%f13
	pfeq.dd %f18,%f20,%f22

	# Non-pipelined, with dual bit 
	d.fix.sd	%f2,%f30
	nop
	d.fix.dd	%f6,%f8
	nop

	d.ftrunc.sd	%f8,%f24
	nop
	d.ftrunc.dd	%f12,%f14
	nop

	# Pipelined, with dual bit 
	d.pfix.sd	%f2,%f30
	nop
	d.pfix.dd	%f6,%f8
	nop

	d.pftrunc.sd	%f8,%f24
	nop
	d.pftrunc.dd	%f12,%f14
	nop

	d.pfgt.ss %f0,%f1,%f2
	nop
	d.pfgt.dd %f6,%f8,%f10
	nop

	d.pfle.ss %f5,%f6,%f7
	nop
	d.pfle.dd %f12,%f14,%f16
	nop

	d.pfeq.ss %f11,%f12,%f13
	nop
	d.pfeq.dd %f18,%f20,%f22
	nop


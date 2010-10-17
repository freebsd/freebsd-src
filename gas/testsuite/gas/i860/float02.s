# frcp, frsqr, famov

	.text

	# Without dual bit 
	frcp.ss	%f0,%f1
	frcp.sd	%f2,%f4
	frcp.dd	%f6,%f8

	frsqr.ss	%f5,%f6
	frsqr.sd	%f8,%f10
	frsqr.dd	%f12,%f14

	famov.ss	%f1,%f31
	famov.ds	%f2,%f30
	famov.sd	%f7,%f16
	famov.dd	%f24,%f12

	# With dual bit 
	d.frcp.ss	%f0,%f1
	nop
	d.frcp.sd	%f2,%f30
	nop
	d.frcp.dd	%f6,%f8
	nop

	d.frsqr.ss	%f5,%f6
	nop
	d.frsqr.sd	%f8,%f24
	nop
	d.frsqr.dd	%f12,%f14
	nop

	d.famov.ss	%f5,%f13
	nop
	d.famov.ds	%f30,%f21
	nop
	d.famov.sd	%f23,%f22
	nop
	d.famov.dd	%f0,%f12
	nop


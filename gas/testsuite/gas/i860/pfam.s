# pfam.p family (p={ss,sd,dd})

	.text

	# pfam without dual bit.
	r2p1.ss	%f0,%f1,%f2
	r2p1.sd	%f3,%f4,%f5
	r2p1.dd	%f0,%f2,%f4

	r2pt.ss	%f1,%f2,%f3
	r2pt.sd	%f4,%f5,%f6
	r2pt.dd	%f2,%f4,%f6

	r2ap1.ss	%f2,%f3,%f4
	r2ap1.sd	%f6,%f7,%f8
	r2ap1.dd	%f4,%f6,%f8

	r2apt.ss	%f3,%f4,%f5
	r2apt.sd	%f7,%f8,%f9
	r2apt.dd	%f6,%f8,%f10

	i2p1.ss	%f4,%f5,%f6
	i2p1.sd	%f8,%f9,%f10
	i2p1.dd	%f12,%f14,%f16

	i2pt.ss	%f7,%f8,%f9
	i2pt.sd	%f11,%f12,%f13
	i2pt.dd	%f14,%f16,%f18

	i2ap1.ss	%f10,%f11,%f12
	i2ap1.sd	%f14,%f15,%f16
	i2ap1.dd	%f16,%f18,%f20

	i2apt.ss	%f13,%f14,%f15
	i2apt.sd	%f17,%f18,%f19
	i2apt.dd	%f18,%f20,%f22

	rat1p2.ss	%f14,%f15,%f16
	rat1p2.sd	%f20,%f21,%f22
	rat1p2.dd	%f20,%f22,%f24

	m12apm.ss	%f15,%f16,%f17
	m12apm.sd	%f23,%f24,%f25
	m12apm.dd	%f22,%f24,%f26

	ra1p2.ss	%f18,%f19,%f20
	ra1p2.sd	%f26,%f27,%f28
	ra1p2.dd	%f20,%f22,%f24

	m12ttpa.ss	%f19,%f20,%f21
	m12ttpa.sd	%f29,%f30,%f31
	m12ttpa.dd	%f22,%f24,%f26

	iat1p2.ss	%f20,%f21,%f22
	iat1p2.sd	%f0,%f1,%f2
	iat1p2.dd	%f24,%f26,%f28

	m12tpm.ss	%f21,%f22,%f23
	m12tpm.sd	%f3,%f4,%f5
	m12tpm.dd	%f30,%f0,%f2

	ia1p2.ss	%f22,%f23,%f24
	ia1p2.sd	%f6,%f7,%f8
	ia1p2.dd	%f4,%f6,%f8

	m12tpa.ss	%f23,%f24,%f25
	m12tpa.sd	%f9,%f10,%f11
	m12tpa.dd	%f6,%f8,%f10

	# pfam with dual bit.
	d.r2p1.ss	%f0,%f1,%f2
	nop
	d.r2p1.sd	%f3,%f4,%f5
	nop
	d.r2p1.dd	%f0,%f2,%f4
	nop

	d.r2pt.ss	%f1,%f2,%f3
	nop
	d.r2pt.sd	%f4,%f5,%f6
	nop
	d.r2pt.dd	%f2,%f4,%f6
	nop

	d.r2ap1.ss	%f2,%f3,%f4
	nop
	d.r2ap1.sd	%f6,%f7,%f8
	nop
	d.r2ap1.dd	%f4,%f6,%f8
	nop

	d.r2apt.ss	%f3,%f4,%f5
	nop
	d.r2apt.sd	%f7,%f8,%f9
	nop
	d.r2apt.dd	%f6,%f8,%f10
	nop

	d.i2p1.ss	%f4,%f5,%f6
	nop
	d.i2p1.sd	%f8,%f9,%f10
	nop
	d.i2p1.dd	%f12,%f14,%f16
	nop

	d.i2pt.ss	%f7,%f8,%f9
	nop
	d.i2pt.sd	%f11,%f12,%f13
	nop
	d.i2pt.dd	%f14,%f16,%f18
	nop

	d.i2ap1.ss	%f10,%f11,%f12
	nop
	d.i2ap1.sd	%f14,%f15,%f16
	nop
	d.i2ap1.dd	%f16,%f18,%f20
	nop

	d.i2apt.ss	%f13,%f14,%f15
	nop
	d.i2apt.sd	%f17,%f18,%f19
	nop
	d.i2apt.dd	%f18,%f20,%f22
	nop

	d.rat1p2.ss	%f14,%f15,%f16
	nop
	d.rat1p2.sd	%f20,%f21,%f22
	nop
	d.rat1p2.dd	%f20,%f22,%f24
	nop

	d.m12apm.ss	%f15,%f16,%f17
	nop
	d.m12apm.sd	%f23,%f24,%f25
	nop
	d.m12apm.dd	%f22,%f24,%f26
	nop

	d.ra1p2.ss	%f18,%f19,%f20
	nop
	d.ra1p2.sd	%f26,%f27,%f28
	nop
	d.ra1p2.dd	%f20,%f22,%f24
	nop

	d.m12ttpa.ss	%f19,%f20,%f21
	nop
	d.m12ttpa.sd	%f29,%f30,%f31
	nop
	d.m12ttpa.dd	%f22,%f24,%f26
	nop

	d.iat1p2.ss	%f20,%f21,%f22
	nop
	d.iat1p2.sd	%f0,%f1,%f2
	nop
	d.iat1p2.dd	%f24,%f26,%f28
	nop

	d.m12tpm.ss	%f21,%f22,%f23
	nop
	d.m12tpm.sd	%f3,%f4,%f5
	nop
	d.m12tpm.dd	%f30,%f0,%f2
	nop

	d.ia1p2.ss	%f22,%f23,%f24
	nop
	d.ia1p2.sd	%f6,%f7,%f8
	nop
	d.ia1p2.dd	%f4,%f6,%f8
	nop

	d.m12tpa.ss	%f23,%f24,%f25
	nop
	d.m12tpa.sd	%f9,%f10,%f11
	nop
	d.m12tpa.dd	%f6,%f8,%f10
	nop


# pfmam.p family (p={ss,sd,dd})

	.text

	# pfmam without dual bit.
	mr2p1.ss	%f0,%f1,%f2
	mr2p1.sd	%f3,%f4,%f5
	mr2p1.dd	%f0,%f2,%f4

	mr2pt.ss	%f1,%f2,%f3
	mr2pt.sd	%f4,%f5,%f6
	mr2pt.dd	%f2,%f4,%f6

	mr2mp1.ss	%f2,%f3,%f4
	mr2mp1.sd	%f6,%f7,%f8
	mr2mp1.dd	%f4,%f6,%f8

	mr2mpt.ss	%f3,%f4,%f5
	mr2mpt.sd	%f7,%f8,%f9
	mr2mpt.dd	%f6,%f8,%f10

	mi2p1.ss	%f4,%f5,%f6
	mi2p1.sd	%f8,%f9,%f10
	mi2p1.dd	%f12,%f14,%f16

	mi2pt.ss	%f7,%f8,%f9
	mi2pt.sd	%f11,%f12,%f13
	mi2pt.dd	%f14,%f16,%f18

	mi2mp1.ss	%f10,%f11,%f12
	mi2mp1.sd	%f14,%f15,%f16
	mi2mp1.dd	%f16,%f18,%f20

	mi2mpt.ss	%f13,%f14,%f15
	mi2mpt.sd	%f17,%f18,%f19
	mi2mpt.dd	%f18,%f20,%f22

	mrmt1p2.ss	%f14,%f15,%f16
	mrmt1p2.sd	%f20,%f21,%f22
	mrmt1p2.dd	%f20,%f22,%f24

	mm12mpm.ss	%f15,%f16,%f17
	mm12mpm.sd	%f23,%f24,%f25
	mm12mpm.dd	%f22,%f24,%f26

	mrm1p2.ss	%f18,%f19,%f20
	mrm1p2.sd	%f26,%f27,%f28
	mrm1p2.dd	%f20,%f22,%f24

	mm12ttpm.ss	%f19,%f20,%f21
	mm12ttpm.sd	%f29,%f30,%f31
	mm12ttpm.dd	%f22,%f24,%f26

	mimt1p2.ss	%f20,%f21,%f22
	mimt1p2.sd	%f0,%f1,%f2
	mimt1p2.dd	%f24,%f26,%f28

	mm12tpm.ss	%f21,%f22,%f23
	mm12tpm.sd	%f3,%f4,%f5
	mm12tpm.dd	%f30,%f0,%f2

	mim1p2.ss	%f22,%f23,%f24
	mim1p2.sd	%f6,%f7,%f8
	mim1p2.dd	%f4,%f6,%f8

	m12tpa.ss	%f23,%f24,%f25
	m12tpa.sd	%f9,%f10,%f11
	m12tpa.dd	%f6,%f8,%f10

	# pfmam with dual bit.
	d.mr2p1.ss	%f0,%f1,%f2
	nop
	d.mr2p1.sd	%f3,%f4,%f5
	nop
	d.mr2p1.dd	%f0,%f2,%f4
	nop

	d.mr2pt.ss	%f1,%f2,%f3
	nop
	d.mr2pt.sd	%f4,%f5,%f6
	nop
	d.mr2pt.dd	%f2,%f4,%f6
	nop

	d.mr2mp1.ss	%f2,%f3,%f4
	nop
	d.mr2mp1.sd	%f6,%f7,%f8
	nop
	d.mr2mp1.dd	%f4,%f6,%f8
	nop

	d.mr2mpt.ss	%f3,%f4,%f5
	nop
	d.mr2mpt.sd	%f7,%f8,%f9
	nop
	d.mr2mpt.dd	%f6,%f8,%f10
	nop

	d.mi2p1.ss	%f4,%f5,%f6
	nop
	d.mi2p1.sd	%f8,%f9,%f10
	nop
	d.mi2p1.dd	%f12,%f14,%f16
	nop

	d.mi2pt.ss	%f7,%f8,%f9
	nop
	d.mi2pt.sd	%f11,%f12,%f13
	nop
	d.mi2pt.dd	%f14,%f16,%f18
	nop

	d.mi2mp1.ss	%f10,%f11,%f12
	nop
	d.mi2mp1.sd	%f14,%f15,%f16
	nop
	d.mi2mp1.dd	%f16,%f18,%f20
	nop

	d.mi2mpt.ss	%f13,%f14,%f15
	nop
	d.mi2mpt.sd	%f17,%f18,%f19
	nop
	d.mi2mpt.dd	%f18,%f20,%f22
	nop

	d.mrmt1p2.ss	%f14,%f15,%f16
	nop
	d.mrmt1p2.sd	%f20,%f21,%f22
	nop
	d.mrmt1p2.dd	%f20,%f22,%f24
	nop

	d.mm12mpm.ss	%f15,%f16,%f17
	nop
	d.mm12mpm.sd	%f23,%f24,%f25
	nop
	d.mm12mpm.dd	%f22,%f24,%f26
	nop

	d.mrm1p2.ss	%f18,%f19,%f20
	nop
	d.mrm1p2.sd	%f26,%f27,%f28
	nop
	d.mrm1p2.dd	%f20,%f22,%f24
	nop

	d.mm12ttpm.ss	%f19,%f20,%f21
	nop
	d.mm12ttpm.sd	%f29,%f30,%f31
	nop
	d.mm12ttpm.dd	%f22,%f24,%f26
	nop

	d.mimt1p2.ss	%f20,%f21,%f22
	nop
	d.mimt1p2.sd	%f0,%f1,%f2
	nop
	d.mimt1p2.dd	%f24,%f26,%f28
	nop

	d.mm12tpm.ss	%f21,%f22,%f23
	nop
	d.mm12tpm.sd	%f3,%f4,%f5
	nop
	d.mm12tpm.dd	%f30,%f0,%f2
	nop

	d.mim1p2.ss	%f22,%f23,%f24
	nop
	d.mim1p2.sd	%f6,%f7,%f8
	nop
	d.mim1p2.dd	%f4,%f6,%f8
	nop

	d.m12tpa.ss	%f23,%f24,%f25
	nop
	d.m12tpa.sd	%f9,%f10,%f11
	nop
	d.m12tpa.dd	%f6,%f8,%f10
	nop


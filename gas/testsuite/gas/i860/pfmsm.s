# pfmsm.p family (p={ss,sd,dd})

	.text

	# pfmsm without dual bit
	mr2s1.ss	%f0,%f1,%f2
	mr2s1.sd	%f3,%f4,%f5
	mr2s1.dd	%f0,%f2,%f4

	mr2st.ss	%f1,%f2,%f3
	mr2st.sd	%f4,%f5,%f6
	mr2st.dd	%f2,%f4,%f6

	mr2ms1.ss	%f2,%f3,%f4
	mr2ms1.sd	%f6,%f7,%f8
	mr2ms1.dd	%f4,%f6,%f8

	mr2mst.ss	%f3,%f4,%f5
	mr2mst.sd	%f7,%f8,%f9
	mr2mst.dd	%f6,%f8,%f10

	mi2s1.ss	%f4,%f5,%f6
	mi2s1.sd	%f8,%f9,%f10
	mi2s1.dd	%f12,%f14,%f16

	mi2st.ss	%f7,%f8,%f9
	mi2st.sd	%f11,%f12,%f13
	mi2st.dd	%f14,%f16,%f18

	mi2ms1.ss	%f10,%f11,%f12
	mi2ms1.sd	%f14,%f15,%f16
	mi2ms1.dd	%f16,%f18,%f20

	mi2mst.ss	%f13,%f14,%f15
	mi2mst.sd	%f17,%f18,%f19
	mi2mst.dd	%f18,%f20,%f22

	mrmt1s2.ss	%f14,%f15,%f16
	mrmt1s2.sd	%f20,%f21,%f22
	mrmt1s2.dd	%f20,%f22,%f24

	mm12msm.ss	%f15,%f16,%f17
	mm12msm.sd	%f23,%f24,%f25
	mm12msm.dd	%f22,%f24,%f26

	mrm1s2.ss	%f18,%f19,%f20
	mrm1s2.sd	%f26,%f27,%f28
	mrm1s2.dd	%f20,%f22,%f24

	mm12ttsm.ss	%f19,%f20,%f21
	mm12ttsm.sd	%f29,%f30,%f31
	mm12ttsm.dd	%f22,%f24,%f26

	mimt1s2.ss	%f20,%f21,%f22
	mimt1s2.sd	%f0,%f1,%f2
	mimt1s2.dd	%f24,%f26,%f28

	mm12tsm.ss	%f21,%f22,%f23
	mm12tsm.sd	%f3,%f4,%f5
	mm12tsm.dd	%f30,%f0,%f2

	mim1s2.ss	%f22,%f23,%f24
	mim1s2.sd	%f6,%f7,%f8
	mim1s2.dd	%f4,%f6,%f8

	m12tsa.ss	%f23,%f24,%f25
	m12tsa.sd	%f9,%f10,%f11
	m12tsa.dd	%f6,%f8,%f10

	# pfmsm with dual bit
	d.mr2s1.ss	%f0,%f1,%f2
	nop
	d.mr2s1.sd	%f3,%f4,%f5
	nop
	d.mr2s1.dd	%f0,%f2,%f4
	nop

	d.mr2st.ss	%f1,%f2,%f3
	nop
	d.mr2st.sd	%f4,%f5,%f6
	nop
	d.mr2st.dd	%f2,%f4,%f6
	nop

	d.mr2ms1.ss	%f2,%f3,%f4
	nop
	d.mr2ms1.sd	%f6,%f7,%f8
	nop
	d.mr2ms1.dd	%f4,%f6,%f8
	nop

	d.mr2mst.ss	%f3,%f4,%f5
	nop
	d.mr2mst.sd	%f7,%f8,%f9
	nop
	d.mr2mst.dd	%f6,%f8,%f10
	nop

	d.mi2s1.ss	%f4,%f5,%f6
	nop
	d.mi2s1.sd	%f8,%f9,%f10
	nop
	d.mi2s1.dd	%f12,%f14,%f16
	nop

	d.mi2st.ss	%f7,%f8,%f9
	nop
	d.mi2st.sd	%f11,%f12,%f13
	nop
	d.mi2st.dd	%f14,%f16,%f18
	nop

	d.mi2ms1.ss	%f10,%f11,%f12
	nop
	d.mi2ms1.sd	%f14,%f15,%f16
	nop
	d.mi2ms1.dd	%f16,%f18,%f20
	nop

	d.mi2mst.ss	%f13,%f14,%f15
	nop
	d.mi2mst.sd	%f17,%f18,%f19
	nop
	d.mi2mst.dd	%f18,%f20,%f22
	nop

	d.mrmt1s2.ss	%f14,%f15,%f16
	nop
	d.mrmt1s2.sd	%f20,%f21,%f22
	nop
	d.mrmt1s2.dd	%f20,%f22,%f24
	nop

	d.mm12msm.ss	%f15,%f16,%f17
	nop
	d.mm12msm.sd	%f23,%f24,%f25
	nop
	d.mm12msm.dd	%f22,%f24,%f26
	nop

	d.mrm1s2.ss	%f18,%f19,%f20
	nop
	d.mrm1s2.sd	%f26,%f27,%f28
	nop
	d.mrm1s2.dd	%f20,%f22,%f24
	nop

	d.mm12ttsm.ss	%f19,%f20,%f21
	nop
	d.mm12ttsm.sd	%f29,%f30,%f31
	nop
	d.mm12ttsm.dd	%f22,%f24,%f26
	nop

	d.mimt1s2.ss	%f20,%f21,%f22
	nop
	d.mimt1s2.sd	%f0,%f1,%f2
	nop
	d.mimt1s2.dd	%f24,%f26,%f28
	nop

	d.mm12tsm.ss	%f21,%f22,%f23
	nop
	d.mm12tsm.sd	%f3,%f4,%f5
	nop
	d.mm12tsm.dd	%f30,%f0,%f2
	nop

	d.mim1s2.ss	%f22,%f23,%f24
	nop
	d.mim1s2.sd	%f6,%f7,%f8
	nop
	d.mim1s2.dd	%f4,%f6,%f8
	nop

	d.m12tsa.ss	%f23,%f24,%f25
	nop
	d.m12tsa.sd	%f9,%f10,%f11
	nop
	d.m12tsa.dd	%f6,%f8,%f10
	nop


# pfsm.p family (p={ss,sd,dd})

	.text

	# pfsm without dual bit
	r2s1.ss	%f0,%f1,%f2
	r2s1.sd	%f3,%f4,%f5
	r2s1.dd	%f0,%f2,%f4

	r2st.ss	%f1,%f2,%f3
	r2st.sd	%f4,%f5,%f6
	r2st.dd	%f2,%f4,%f6

	r2as1.ss	%f2,%f3,%f4
	r2as1.sd	%f6,%f7,%f8
	r2as1.dd	%f4,%f6,%f8

	r2ast.ss	%f3,%f4,%f5
	r2ast.sd	%f7,%f8,%f9
	r2ast.dd	%f6,%f8,%f10

	i2s1.ss	%f4,%f5,%f6
	i2s1.sd	%f8,%f9,%f10
	i2s1.dd	%f12,%f14,%f16

	i2st.ss	%f7,%f8,%f9
	i2st.sd	%f11,%f12,%f13
	i2st.dd	%f14,%f16,%f18

	i2as1.ss	%f10,%f11,%f12
	i2as1.sd	%f14,%f15,%f16
	i2as1.dd	%f16,%f18,%f20

	i2ast.ss	%f13,%f14,%f15
	i2ast.sd	%f17,%f18,%f19
	i2ast.dd	%f18,%f20,%f22

	rat1s2.ss	%f14,%f15,%f16
	rat1s2.sd	%f20,%f21,%f22
	rat1s2.dd	%f20,%f22,%f24

	m12asm.ss	%f15,%f16,%f17
	m12asm.sd	%f23,%f24,%f25
	m12asm.dd	%f22,%f24,%f26

	ra1s2.ss	%f18,%f19,%f20
	ra1s2.sd	%f26,%f27,%f28
	ra1s2.dd	%f20,%f22,%f24

	m12ttsa.ss	%f19,%f20,%f21
	m12ttsa.sd	%f29,%f30,%f31
	m12ttsa.dd	%f22,%f24,%f26

	iat1s2.ss	%f20,%f21,%f22
	iat1s2.sd	%f0,%f1,%f2
	iat1s2.dd	%f24,%f26,%f28

	m12tsm.ss	%f21,%f22,%f23
	m12tsm.sd	%f3,%f4,%f5
	m12tsm.dd	%f30,%f0,%f2

	ia1s2.ss	%f22,%f23,%f24
	ia1s2.sd	%f6,%f7,%f8
	ia1s2.dd	%f4,%f6,%f8

	m12tsa.ss	%f23,%f24,%f25
	m12tsa.sd	%f9,%f10,%f11
	m12tsa.dd	%f6,%f8,%f10

	# pfsm with dual bit
	d.r2s1.ss	%f0,%f1,%f2
	nop
	d.r2s1.sd	%f3,%f4,%f5
	nop
	d.r2s1.dd	%f0,%f2,%f4
	nop

	d.r2st.ss	%f1,%f2,%f3
	nop
	d.r2st.sd	%f4,%f5,%f6
	nop
	d.r2st.dd	%f2,%f4,%f6
	nop

	d.r2as1.ss	%f2,%f3,%f4
	nop
	d.r2as1.sd	%f6,%f7,%f8
	nop
	d.r2as1.dd	%f4,%f6,%f8
	nop

	d.r2ast.ss	%f3,%f4,%f5
	nop
	d.r2ast.sd	%f7,%f8,%f9
	nop
	d.r2ast.dd	%f6,%f8,%f10
	nop

	d.i2s1.ss	%f4,%f5,%f6
	nop
	d.i2s1.sd	%f8,%f9,%f10
	nop
	d.i2s1.dd	%f12,%f14,%f16
	nop

	d.i2st.ss	%f7,%f8,%f9
	nop
	d.i2st.sd	%f11,%f12,%f13
	nop
	d.i2st.dd	%f14,%f16,%f18
	nop

	d.i2as1.ss	%f10,%f11,%f12
	nop
	d.i2as1.sd	%f14,%f15,%f16
	nop
	d.i2as1.dd	%f16,%f18,%f20
	nop

	d.i2ast.ss	%f13,%f14,%f15
	nop
	d.i2ast.sd	%f17,%f18,%f19
	nop
	d.i2ast.dd	%f18,%f20,%f22
	nop

	d.rat1s2.ss	%f14,%f15,%f16
	nop
	d.rat1s2.sd	%f20,%f21,%f22
	nop
	d.rat1s2.dd	%f20,%f22,%f24
	nop

	d.m12asm.ss	%f15,%f16,%f17
	nop
	d.m12asm.sd	%f23,%f24,%f25
	nop
	d.m12asm.dd	%f22,%f24,%f26
	nop

	d.ra1s2.ss	%f18,%f19,%f20
	nop
	d.ra1s2.sd	%f26,%f27,%f28
	nop
	d.ra1s2.dd	%f20,%f22,%f24
	nop

	d.m12ttsa.ss	%f19,%f20,%f21
	nop
	d.m12ttsa.sd	%f29,%f30,%f31
	nop
	d.m12ttsa.dd	%f22,%f24,%f26
	nop

	d.iat1s2.ss	%f20,%f21,%f22
	nop
	d.iat1s2.sd	%f0,%f1,%f2
	nop
	d.iat1s2.dd	%f24,%f26,%f28
	nop

	d.m12tsm.ss	%f21,%f22,%f23
	nop
	d.m12tsm.sd	%f3,%f4,%f5
	nop
	d.m12tsm.dd	%f30,%f0,%f2
	nop

	d.ia1s2.ss	%f22,%f23,%f24
	nop
	d.ia1s2.sd	%f6,%f7,%f8
	nop
	d.ia1s2.dd	%f4,%f6,%f8
	nop

	d.m12tsa.ss	%f23,%f24,%f25
	nop
	d.m12tsa.sd	%f9,%f10,%f11
	nop
	d.m12tsa.dd	%f6,%f8,%f10
	nop


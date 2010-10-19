_start:
	alloc		r8 = 0, 0, 0, 0
	cmp.eq		p6 = r0, r0
	cmp.eq		p7 = 0, r0
	cmp4.eq		p8 = r0, r0
	cmp4.eq		p9 = 0, r0
	cmp8xchg16.acq	r9 = [r0], r0
	cmpxchg4.acq	r10 = [r0], r0
	fclass.m	p10 = f0, @pos
	fcmp.eq		p11 = f0, f0
	ld16		r11 = [r0]
	mov		pr = r0
	st16		[r0] = r0
	tbit.nz		p12 = r0, 0
	tnat.nz		p13 = r0

	# instructions added by SDM2.2:

	tf.nz p2, p3 = 33

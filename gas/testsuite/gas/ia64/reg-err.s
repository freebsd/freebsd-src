	.text
_start:
	mov	r0 = r0
	ld1	r1 = [r0], 1
	ld1	r1 = [r1], 1
	st1	[r0] = r0, 1
	cmp.eq	p1, p1 = 0, r0
	mov	f0 = f0
	mov	f1 = f1
	ldfs	f0 = [r0]
	ldfs	f1 = [r0]
	ldfps	f0, f1 = [r0]
	ldfps	f2, f4 = [r0]
	ldfps	f31, f32 = [r0]

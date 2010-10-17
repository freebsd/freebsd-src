	.text
	.p2align 2

	movli.l @r1,r0
	movco.l	r0,@r0

	movli.l @r6,r0
	movco.l r0,@r3

	movli.l @r10,r0
	movco.l r0,@r12

	movua.l @r0,r0
	movua.l @r13,r0
	movua.l @r7,r0

	movua.l @r5+,r0
	movua.l @r2+,r0
	movua.l @r11+,r0

	icbi	@r4
	icbi	@r15
	icbi	@r2

	prefi	@r5
	prefi	@r10

	synco

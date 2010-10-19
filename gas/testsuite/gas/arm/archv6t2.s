	.text
x:
	bfi	r0, r0, #0, #1
	bfine	r0, r0, #0, #1

	bfi	r9, r0, #0, #1
	bfi	r0, r9, #0, #1
	bfi	r0, r0, #0, #18
	bfi	r0, r0, #17, #1

	bfi	r0, #0, #0, #1
	bfc	r0, #0, #1
	bfcne	r0, #0, #1
	bfc	r9, #0, #1
	bfc	r0, #0, #18
	bfc	r0, #17, #1

	sbfx	r0, r0, #0, #1
	sbfxne	r0, r0, #0, #1
	ubfx	r0, r0, #0, #1
	sbfx	r9, r0, #0, #1
	sbfx	r0, r9, #0, #1
	sbfx	r0, r0, #17, #1
	sbfx	r0, r0, #0, #18
	
	rbit	r0, r0
	rbitne	r0, r0
	rbit	r9, r0
	rbit	r0, r9

	mls	r0, r0, r0, r0
	mlsne	r0, r0, r0, r0
	mls	r9, r0, r0, r0
	mls	r0, r9, r0, r0
	mls	r0, r0, r9, r0
	mls	r0, r0, r0, r9
	
	movw	r0, #0
	movt	r0, #0
	movwne	r0, #0
	movw	r9, #0
	movw	r0, #0x0999
	movw	r0, #0x9000

	@ for these, we must avoid write-back warnings
	ldrht	r0, [r9]
	ldrsht	r0, [r9]
	ldrsbt	r0, [r9]
	strht	r0, [r9]
	ldrneht	r0, [r9]

	ldrht	r9, [r0], r9
	ldrht	r9, [r0], -r9
	ldrht	r9, [r0], #0x99
	ldrht	r9, [r0], #-0x99

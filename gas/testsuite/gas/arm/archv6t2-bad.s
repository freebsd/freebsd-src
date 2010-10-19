	@ We do not bother testing simple cases, e.g. immediates where
	@ registers belong, trailing junk at end of line.
	.text
x:
	@ pc not allowed
	bfc	pc,#0,#1
	bfi	pc,r0,#0,#1
	movw	pc,#0
	movt	pc,#0

	@ bitfield range limits
	bfc	r0,#0,#0
	bfc	r0,#32,#0
	bfc	r0,#0,#33
	bfc	r0,#33,#1
	bfc	r0,#32,#1
	bfc	r0,#28,#10

	bfi	r0,r1,#0,#0
	bfi	r0,r1,#32,#0
	bfi	r0,r1,#0,#33
	bfi	r0,r1,#33,#1
	bfi	r0,r1,#32,#1
	bfi	r0,r1,#28,#10

	sbfx	r0,r1,#0,#0
	sbfx	r0,r1,#32,#0
	sbfx	r0,r1,#0,#33
	sbfx	r0,r1,#33,#1
	sbfx	r0,r1,#32,#1
	sbfx	r0,r1,#28,#10

	ubfx	r0,r1,#0,#0
	ubfx	r0,r1,#32,#0
	ubfx	r0,r1,#0,#33
	ubfx	r0,r1,#33,#1
	ubfx	r0,r1,#32,#1
	ubfx	r0,r1,#28,#10

	@ bfi accepts only #0 in Rm position
	bfi	r0,#1,#2,#3

	@ mov16 range limits
	movt	r0,#65537
	movw	r0,#65537
	movt	r0,#-1
	movw	r0,#-1

	@ ldsttv4 Rd == Rn (warning)
	ldrht	r0,[r0]
	ldrsbt	r0,[r0]
	ldrsht	r0,[r0]
	strht	r0,[r0]

	@ Bug reported by user.  GAS used to issue an error message
	@ "r15 not allowed here" for these two instructions because
	@ it thought that the "r2" operand was a PC-relative branch
	@ to a label called "r2".
	ldrex	r0, r2
	strex	r1, r0, r2
	
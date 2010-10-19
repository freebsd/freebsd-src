	.text
	.syntax unified
	.arch armv7a
	.thumb
	.thumb_func
thumb2_it_bad:
	itttt	eq
	beq	foo
	bleq	foo
	blxeq	r0
	cbzeq	r0, foo
	ittt	eq
	bxeq	r0
	tbbeq	[r0, r1]
	cpsieeq	f
	it	eq
	cpseq	#0x10
	itt	eq
	bkpteq	0
	setendeq	le
	it	eq
	iteq	eq
	nop
foo:

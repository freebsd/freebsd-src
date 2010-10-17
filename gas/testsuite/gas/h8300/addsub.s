	.text
h8300_add_sub:
	add.b #16,r1l
	add.b r1h,r1l
	add.w r1,r2
	adds #1,r4
	adds #2,r5
	addx r0l,r1l
	addx #16,r2h
	sub.b r0l,r1l
	sub.w r0,r1
	subs #1,r4
	subs #2,r5
	subx r0l,r1l
	subx #16,r2h


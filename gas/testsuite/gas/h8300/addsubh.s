	.h8300h
	.text
h8300h_add_sub:
	add.b #16,r1l
	add.b r1h,r1l
	add.w #32,r1
	add.w r1,r2
	add.l #64,er1
	add.l er1,er2
	adds #1,er4
	adds #2,er5
	adds #4,er6
	addx r0l,r1l
	addx #16,r2h
	sub.b r0l,r1l
	sub.w #16,r1
	sub.w r0,r1
	sub.l #64,er1
	sub.l er1,er2
	subs #1,er4
	subs #2,er5
	subs #4,er6
	subx r0l,r1l
	subx #16,r2h


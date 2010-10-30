	.arm
	.syntax unified

fish	.qn	q2
cow	.dn	d2[1]
chips	.dn	d2
banana	.dn	d3

	vmul fish.s16, fish.s16, fish.s16

	vmul banana, banana, cow.s32
	vmul d3.s32, d3.s32, d2.s32
	vadd d2.s32, d3.s32
	vmull fish.u32, chips.u16, chips.u16[1]

X	.dn D0.S16
Y	.dn D1.S16
Z 	.dn Y[2]

	VMLA X, Y, Z
	VMLA X, Y, Y[2]

foo	.dn d5
bar	.dn d7
foos	.dn foo[1]

	vadd foo, foo, foo.u32

	vmov foo, bar
	vmov d2.s16[1], r1
	vmov d5.s32[1], r1
	vmov foo, r2, r3
	vmov r4, foos.s8
	vmov r5, r6, foo

baa	.qn	q5
moo	.dn	d6
sheep	.dn	d7
chicken	.dn	d8

	vabal baa, moo.u16, sheep.u16

	vcvt q1.s32, q2.f32
	vcvt d4.f, d5.u32, #5

	vdup bar, foos.32
	vtbl d1, {baa}, d4.8

el1	.dn	d4.16[1]
el2	.dn	d6.16[1]
el3	.dn	d8.16[1]
el4	.dn	d10.16[1]

	vld2 {moo.32[1], sheep.32[1]}, [r10]
	vld4 {el1, el2, el3, el4}, [r10]
	vld3 {moo.16[], sheep.16[], chicken.16[]}, [r10]

	vmov r0,d0.s16[0]

el5	.qn	q3.16
el6	.qn	q4.16

	vld4 {el5,el6}, [r10]

	vld3 {d2.s16[1], d4.s16[1], d6.s16[1]}, [r10]

chicken8	.dn	chicken.8

	vtbl d7.8, {d4, d5}, chicken8

	vbsl q1.8, q2.16, q3.8

	vcge d2.32, d3.f, d4.f
	vcge d2.16, d3.s16, #0

dupme	.dn	d2.s16

	vdup dupme, r3

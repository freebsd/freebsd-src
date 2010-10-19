	.text
	.global add_with_shift
add_with_shift:
	P0 = (P0 + p1) << 1;
	P2 = (p2 + p5) << 2;
	r7 = (R7 + r1) << 2;
	r3 = (r3 + R0) << 1;

	.text
	.global shift_with_add
shift_with_add:
	P5 = p4 + (P0 << 2);
	P0 = p2 + (p1 << 1);

	.text
	.global arithmetic_shift
arithmetic_shift:
	A0 = A0 >> 31;
	a0 = a0 << 31;
	a1 = a1 >> 0;
	A1 = A1 << 0;
	r7 = r5 << 31 (s);
	R3 = r2 >>> 22;
	r1.L = R2.H << 15 (S);
	r5.h = r2.l >>> 2;
	r0 <<= 0;
	r1 >>>= 31;

	r0 >>>= R1;
	R2 <<= R1;
	r3.l = Ashift  r4.h by r2.l;
	R7.H = ASHIFT R7.L by R0.L (S);
	r7.h = ashift  r7.l by r0.l (s);
	r6 = AShiFT R5 by R2.L;
	R0 = Ashift R4 by r1.l (s);
	r2 = ashift r6 BY r3.L (S);
	A0 = Ashift a0 by r1.l;
	a1 = ASHIFT a1 by r0.L;


	.text
	.global logical_shift
logical_shift:
	p0 = p0 >> 1;
	P1 = p2 >> 2;
	P3 = P1 << 1;
	p4 = p5 << 2;

	r0 >>= 31;
	R7 <<= 31;
	r1.H = r2.l >> 15;
	r7.l = r0.L << 0;
	r5 = r5 >> 31;
	r0 = r0 << 12;
	A0 = A0 >> 1;
	A0 = A0 << 0;
	a1 = A1 << 31;
	a1 = a1 >> 16;
	
	r5 >>= R7;
	R6 <<= r0;
	R1.H = LShift r2.h by r0.l;
	r0.l = LSHIFT r0.h by r1.l;
	r7.L = lshift r6.L BY r2.l;
	r5 = LShIft R4 bY r3.L;
	A0 = Lshift a0 By R6.L;
	A1 = LsHIFt a1 by r5.l;

	.text
	.global rotate
rotate:
	r7 = ROT r7 by -32;
	R6 = Rot r7 by -31;
	R5 = RoT R7 by 31;
	R4 = Rot r7 by 30;
	a0 = rot A0 by 0;
	A0 = ROT a0 BY 10;
	A1 = ROT A1 by -20;
	A1 = ROT a1 By -32;

	r0 = rot r1 by r2.L;
	R0 = Rot R4 BY R3.L;
	A0 = ROT A0 by r7.l;
	A1 = rot a1 bY r6.l;




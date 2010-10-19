	.section .text;
	R7 = Align8 (r5, r2) || [i0] = r0;
	R5 = ALIGN16 (R0, R1) || [i0++] = r0;
	r2 = ALIGN24 (r5, r0) || [i0--] = r0;

	DISAlgnExcpt || [i1] = r0;

	R5 = Byteop3p (r1:0, r3:2) (lO) 
		|| [i1++] = r0;
	R0 = BYTEOP3P (R1:0, R3:2) (HI) || // comment test
		[i1--] = r0;
	R1 = byteop3p (r1:0, r3:2) (LO, r) || [i2] = r0;
	r2 = ByteOp3P (r1:0, R3:2) (hi, R) || [i2++] = r0;

	R5 = A1.l + A1.h, R2 = a0.l + a0.h || [i2--] = r0;

	(r2, r3) = BYTEOP16P (R1:0, R3:2) || [i3] = r0;
	(R6, R0) = byteop16p (r1:0, r3:2) (r) || [i3++] = r0;

	R7 = BYTEOP1P (R1:0, R3:2) (t) || [i3--] = r0;
	r2 = byteop1p (r1:0, r3:2) (t) || [p0] = r0;
	R3 = ByteOp1P (r1:0, R3:2) (R) || [p0++] = r0;
	r7 = byteOP1P (R1:0, r3:2) (T, r) || [p0--] = r0;

	R0 = BYTEOP2P (R1:0, R3:2) (RNDL) || [p1] = r0;
	r1 = byteop2p (r1:0, r3:2) (rndh) || [p1++] = r0;
	R2 = Byteop2p (R1:0, R3:2) (tL) || [p1--] = r0;
	R3 = Byteop2p (r1:0, r3:2) (TH) || [p2] = r0;
	r4 = ByTEOP2P (r1:0, R3:2) (Rndl, R) || [p2++] = r0;
	R5 = byTeOp2p (R1:0, r3:2) (rndH, r) || [p2--] = r0;
	r6 = BYTEop2p (r1:0, r3:2) (tl, R) || [p3] = r0;
	R7 = byteop2p (r1:0, R3:2) (TH, r) || [p3++] = r0;

	R5 = BytePack (R0, R3) || [p3--] = r0;

	(R6, R2) = ByteOp16M (r1:0, r3:2) || [p4] = r0;
	(r0, r5) = byteop16m (R1:0, R3:2) (r) || [p4++] = r0;

	saa (r1:0, r3:2) || [p4--] = r0;
	SAA (R1:0, R3:2) (r) || [p5] = r0;

	(R7, R2) = byteunpack R1:0 || [p5++] = r0;
	(R6, R4) = BYTEUNPACK r3:2 (R) || [p5--] = r0;

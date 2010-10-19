	.section .text;
	A0 = A1 || P0 = [sp+20];
	a1 = a0 || P0 = [p5+24];
	a0 = R0 || P0 = [P4+28];
	A1 = r1 || P0 = [P3+32];

	R4 = A0 (fu) || P0 = [p3+36];
	r5 = A1 (ISS2) || P0 = [P3+40];
	R6 = a0 || P0 = [P4+44];
	R7 = A1 || P0 = [P4+48];
	R6 = A0, R7 = a1 || P0 = [P4+52];
	r1 = a1, r0 = a0 (fu) || P0 = [P4+56];

	A0.X = r5.l || p0 = [p4+60];
	a1.X = r2.L || r0 = [i0 ++ m0];
	r0.l = a0.x || r1 = [i0 ++ m1];
	R7.l = A1.X || r0 = [i0 ++ m2];
	A0.L = r3.l || r0 = [i0 ++ m3];
	a1.l = r4.l || r0 = [i1 ++ m3];
	A0.h = r6.H || r0 = [i1 ++ m2];
	A1.H = r5.h || r0 = [i1 ++ m1];
	r0.l = A0 (iu) || r4 = [i1 ++ m0];
	R1.H = A1 (s2rnd) || r0 = [i2 ++ m0];
	r1.h = a1 || r0 = [i2 ++ m1];
	R2.l = A0, r2.H = A1 (IH) || r0 = [i2 ++ m2];
	R2.l = A0, r2.H = A1 || r0 = [i2 ++ m3];
	r0.H = A1, R0.L = a0 (t) || r5 = [i3 ++ m0];
	r0.H = A1, R0.L = a0 (fu) || r5 = [i3 ++ m1];
	r0.H = A1, R0.L = a0 (is) || r5 = [i3 ++ m2];
	r0.H = A1, R0.L = a0 || r5 = [i3 ++ m3];

	A0 = A0 >> 31 || r0 = [fp - 32];
	a0 = a0 << 31 || r0 = [fp - 28];
	a1 = a1 >> 0 || r0 = [fp - 24];
	A1 = A1 << 0 || r0 = [fp - 20];
	r7 = r5 << 31 (s) || r0 = [fp - 16];
	R3 = r2 >>> 22 || r0 = [fp - 12];
	r1.L = R2.H << 15 (S) || r0 = [fp - 8];
	r5.h = r2.l >>> 2 || r0 = [fp - 4];

	r3.l = Ashift  r4.h by r2.l || r0 = [fp - 100];
	R7.H = ASHIFT R7.L by R0.L (S) || r0 = [fp - 104];
	r7.h = ashift  r7.l by r0.l (s) || r0 = [fp - 108];
	r6 = AShiFT R5 by R2.L || r0 = [fp - 112];
	R0 = Ashift R4 by r1.l (s) || r3 = [fp - 116];
	r2 = ashift r6 BY r3.L (S) || r0 = [fp - 120];
	A0 = Ashift a0 by r1.l || r0 = [fp - 124];
	a1 = ASHIFT a1 by r0.L || r0 = [fp - 128];

	r1.H = r2.l >> 15 || R5 = W [P1--] (z);
	r7.l = r0.L << 0 || R5 = W [P2] (z);
	r5 = r5 >> 31 || R7 = W [P2++] (z);
	r0 = r0 << 12 || R5 = W [P2--] (z);
	A0 = A0 >> 1 || R5 = W [P2+0] (z);
	A0 = A0 << 0 || R5 = W [P2+2] (z);
	a1 = A1 << 31 || R5 = W [P2+4] (z);
	a1 = a1 >> 16 || R5 = W [P2+30] (z);
	
	R1.H = LShift r2.h by r0.l || R5 = W [P2+24] (z);
	r0.l = LSHIFT r0.h by r1.l || R5 = W [P2+22] (z);
	r7.L = lshift r6.L BY r2.l || R5 = W [P2+20] (z);
	r5 = LShIft R4 bY r3.L || R4 = W [P2+18] (z);
	A0 = Lshift a0 By R6.L || R5 = W [P2+16] (z);
	A1 = LsHIFt a1 by r5.l || R5 = W [P2+14] (z);

	r7 = ROT r7 by -32 || R5 = W [P2+12] (z);
	R6 = Rot r7 by -31 || R5 = W [P2+10] (z);
	R5 = RoT R7 by 31 || R6 = W [P2+8] (z);
	R4 = Rot r7 by 30 || R5 = W [P2+6] (z);
	a0 = rot A0 by 0 || R5 = W [P3] (z);
	A0 = ROT a0 BY 10 || R5 = W [P3++] (z);
	A1 = ROT A1 by -20 || R5 = W [P3--] (z);
	A1 = ROT a1 By -32 || R5 = W [P4] (z);

	r0 = rot r1 by r2.L || R5 = W [P4++] (z);
	R0 = Rot R4 BY R3.L || R5 = W [P4--] (z);
	A0 = ROT A0 by r7.l || R5 = W [P5] (z);
	A1 = rot a1 bY r6.l || R5 = W [P5++] (z);

	NOp || R5 = W [P5--] (z);

	.section .text;
	R5 = Deposit (r3, r2) || I0 += 2;
	r0 = DEPOSIT (r7, R6) (X) || I1 += 4;
	r4 = extract (r2, r1.L) (z) || I2 -= M0;
	R2 = EXTRACT (r0, r2.l) (Z) || i3 += m1;

	r7 = ExtracT (r3, r4.L) (X) || I3 += M1 (breV);
	r5 = ExtRACt (R6, R1.L) (x) || i0 -= 2;

	BITMUX(R1, R0, A0) (ASR) || I1 -= 4;
	Bitmux (r2, R3, a0) (aSr) || I0 += 2;

	bitmux (r4, r5, a0) (asl) || Sp = [P0];
	BiTMux (R7, r6, a0) (ASl) || FP = [P1++];

	R5.l = ones r0 || P0 = [fp--];
	r7.L = Ones R2 || p1 = [P5 + 24];

	a0 = abs a0 || p2 = [Sp+60] || r0 = [i0];
	A0 = ABS A1 || P3 = [FP-60] || R1 = [I1++M0];
	A1 = Abs a0 || P4 = [fp-4] || r2 = [i1++];
	a1 = aBs A1 || fp = [sp] || r3 = [I2--];
	A1 = abs a1, a0 = ABS A0 || R4=[p5+56] || r0.h = w [I0];
	r0 = abs r2 || B[sp] = r0 || R1.H = W[I1++];

	r4.L = R2.h + r0.L (s) || b [fp] = r0 || r2.H = w [i2--];
	r5.H = R1.H + R1.L (S) || b [p0] = r0 || R3.l = W[I3];
	r6.L = R6.L + r5.l (NS) || b [p1] = r0 || r4.L =w [i3++];

	r4.l = r0 + r1 (RND20) || b [p2] = r0 || R5.l = W [i2--];
	R3.H = r5 + r0 (rnd20) || r0 = b [p0] (x) || [i0] = R6;
	r1.L = r7 - R5 (rND20) || r0 = b [p4] (z) || [I1++] = R7;

	r2.L = R0 + R1 (rnd12) || r1 = b [sp] (x) || [I2--]= r7;
	r7.H = r7 + r6 (RND12) || r1 = b [p0] (x)|| [I3++m1]=r6;
	r5.l = r3 - R2 (rNd12) || r1 = b [p1] (z) || W [ i3 ] = r5.h;
	r2.h = R1 - R2 (Rnd12) || r1 = b [p2] (z) || w [I2++] = R4.H;


	r6.L = EXPADJ (r5, r4.l) || r1 = b [p3] (z) || W[I1--]=r3.h;
	R5.l = ExpAdj (r0.h, r1.l) || r1 = b [p4] (z) || w[i0]=r2.l;
	R4.L = expadj (R3, R5.L) (V) || r1 = b [p5] (z) || W [I0++] = R1.L;

	R6 = MAX (r5, R2) || r2 = b [p0] (x) || W[i1--]=R0.l;
	r0 = max (r1, r3) || b [p1] = r2 || NoP;

	r5 = mIn (r2, R3) || b [p2] = r2 || r0 = [i1++];
	R4 = Min (r7, R0) || b [p3] = r2 || r1 = [i1++];


	A0 -= A1 || b [p4] = r2 || r2 = [i1++];
	a0 -= a1 (w32) || b [p5] = r2 || r3 = [i1++];

	a0 += a1 || b [sp] = r2 || r4 = [i1++];
	A0 += A1 (w32) || b [fp] = r2 || r5 = [i1++];
	r7 = ( a0 += a1) || b [sp] = r3 || r6 = [i1++];
	r6.l = (A0 += a1) || b [fp] = r3 || r7 = [i1++];
	R0.H = (a0 += A1) || b [p0] = r3 || r7 = [i0++];


	R0.l = r1.h * r2.l || b [p1] = r3 || r1 = [i0++];
	r1.L = r5.H * r0.H (s2rnd) || b [p2] = r3 || r2 = [i0++];
	r7.l = r3.l * r3.H (FU) || b [p3] = r3 || r3 = [i0++];
	r4 = r2.H * r5.H (iSS2) || b [p4] = r3 || r0 = [i0++];
	r0 = r1.l * r3.l (is) || b [p5] = r3 || r5 = [i0++];
	r6 = R5.H * r0.l || b [fp] = r4 || r7 = [i0++];

	r2.h = r7.l * r6.H (M, iu) || b [sp] = r4 || r6 = [i0++];
	r3.H = r5.H * r0.L || r4 = b [p0] (x) ||  [I0++M0] = R0;
	R0.H = r1.L * r1.H (M) || r4 = b [p1] (x) || [i0++M0] = R1;
	r1 = r7.H * r6.L (M, is) || r4 = b [p2] (x) || [i0++M0] = R2;
	R5 = r0.l * r2.h || r4 = b [p3] (x) || [i0++m0] = R3;
	r3 = r6.H * r0.H (m) || r4 = b [p4] (z) || [i0++m0] = R4;

	a0 = r5.l * R7.H (w32) || r4 = b [p5] (z) || [i0++m0] = R5;
	a0 = r0.h * r0.l || r5 = b [p0] (x) || [i0++M0] =R6;
	A0 += R2.L * r3.H (FU) || r5 = b [p1] (z) || [i0++M0]=R7;
	A0 += r4.h * r1.L || r5 = b [p2] (z) || [I1++M1] = R7;
	a0 -= r7.l * r6.H (Is) || r5 = b [p3] (x) || [i1++m1] = r6;
	A0 -= R5.H * r2.H || r5 = b [p4] (z) || [i1++m1]=r5;

	a1 = r1.L * r0.H (M) || r5 = b [p5] (x) || [i1++m1]=r4;
	A1 = r2.h * r0.L || r5 = b [sp] (z)  || [i1++m1] = r3;
	A1 = R7.H * R6.L (M, W32) || r5 = b [fp] (x)  || [i1++m1] =r2;

	a1 += r3.l * r2.l (fu) || r0.l = w [i0]  || [i1++m1] = r1;
	a1 += R6.H * r1.L || r1.l = w [i0]  || [i1++m1] = R0;
	A1 -= r0.L * R3.H (is) || r2.l = w [i0]  || [i2++m2] = R0;
	a1 -= r2.l * r7.h || r3.l = w [i0]  || [I2++M2] =R1;

	r7.l = (a0 = r6.H * r5.L) || r4.l = w [i0]  || [i2++m2] = r2;
	r0.L = (A0 = r1.h * R2.l) (tfu) || r5.l = w [i0]  || [I2++m2] = R3;
	R2.L = (a0 += r5.L * r4.L) || r6.l = w [i0]  || [I2++m2] = R4;
	r3.l = (A0 += r7.H * r6.h) (T) || r7.l = w [i0]  ||  [ i2 ++ m2] = R5;
	r0.l = (a0 -= r3.h * r2.h) || r7.l = w [i1++]  || [i2++m2] = r6;
	r1.l = (a0 -= r5.L * r4.L) (iH) || r6.l = w [i1++]  || [i2++m2] = R7;

	r1.H = (a1 = r1.l * R0.H) || r2.l = w [i1++]  || [i3++m3] = R7;
	r2.h = (A1 = r0.H * r3.L) (M, Iss2) || r3.l = w [i1++]  || [i3++m3] = r6;
	R6.H = (a1 += r7.l * r7.H) || r4.l = w [i1++]  || [i3++m3] = R5;
	r7.h = (a1 += R2.L * R3.L) (S2rnd) || r5.l = w [i1++]  ||  [i3++m3] = r4;
	r6.H = (A1 -= R4.h * r2.h) || r6.l = w [i1++]  || [i3++m3] = r3;
	r5.h = (a1 -= r3.H * r7.L) (M, tFu) || r7.l = w [i1++]  || [i3++m3] = r2;

	R0 = (A0 = R1.L * R2.L) || R1.L = W [I2--]  || [i3++m3] = r1;
	R2 = (A0 = r1.l * r2.l) (is) || R1.L = W [I2--]  || [i3++m3] = r0;
	r4 = (a0 += r7.h * r6.L) || R2.L = W [I2--]  || r0.h = w[i0];
	r6 = (A0 += R5.L * r3.h) (s2RND) || R3.L = W [I2--]  || R1.H = w[i1];
	R6 = (a0 -= r2.h * r7.l) || R4.L = W [I2--]  || r2.h = w[i2];
	r4 = (A0 -= R0.L * r6.H) (FU) || R5.L = W [I2--]  || r3.h = w[i3];

	r7 = (a1 = r0.h * r1.l) || R6.L = W [I2--]  || r4.h = w[i3];
	R5 = (A1 = r2.H * r3.H) (M, fu) || R7.L = W [I2--]  || r4.h = W[i2];
	R3 = (A1 += r7.l * r5.l) || w [p0] = r0.L || r6.h = W[i1];
	r1 = (a1 += r2.h * r7.h) (iss2) || w [p0] = r1.L || r7.h = w[i0];
	r3 = (A1 -= r0.l * R0.H) || w [p0] = r2.L || r7.L = w[I0++];
	R5 = (a1 -= R2.l * R7.h) (m, is) || w [p0] = r3.L || R6.L = W [i1++];

	r7 = -R2(s) || w [p0] = r4.L || r5.l = w[i2++];
	A0 = -A0 || w [p0] = r5.L || r4.l = w[i3++];
	a0 = -a1 || w [p0] = r6.L || r3.L = w [i3--];
	A1 = -A0 || w [p0] = r7.L || r2.l = W [i1++];
	a1 = -A1 || w [p1] = r0 || r1.L = w [i2--];
	a1 = -a1, a0 = -a0 || w [p1] = r1 || r0.l = w [i1--];

	R5.L = r3 (rnd) || w [p1] = r2 || r0 = [i0++m3];
	r6.H = r0 (RND) || w [p1] = r3 || r1 = [i1++m2];

	A0 = A0 (S) || w [p1] = r4  || r2 = [i2++m1];
	a1 = a1 (s) || w [p1] = r5  || r3 = [i3++m0];
	A1 = a1 (S), a0 = A0 (s) || r6 = w [p1] (z) || [i0] = r0;

	R5.l = signbits r0 || r7 = w [p1] (z) || [i1] = R0;
	r0.L = SIGNbits r7.H || r1 = w [p2++](x) || [I2] = r0;
	r3.l = signBits A0 || r2 = w [p2++] (x) || [I3] = R0;
	r7.L = SIGNBITS a1 || r3 = w [p2++] (z) || [i0] = R1;

	r5.l = R6.H - R7.h (s) || r4 = w [p2++] (x) || [i1] = r1;
	r0.H = r3.l - r3.h (NS) || r5 = w [p2++] (x) || [i2] = r2;

	R1 = [I0++] || R2 = ABS R2 || NOP;

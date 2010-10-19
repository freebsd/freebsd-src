	.section .text;
	r4.h = r4.l = Sign (R1.h) * R5.h + Sign(r1.L) * R5.L|| [p0] = P0;

	R7 = Vit_Max (R5, r2) (ASL)|| [p0++] = P0;
	r0 = VIT_MAX (r0, r6) (asr)|| [p0--] = P0;
	r5.l = vit_max (R3) (asL)|| [p0+4] = P0;
	r2.L = VIT_Max (r2) (Asr)|| [p0+8] = P0;

	R5 = ABS R5 (V)|| [p0+60] = P0;
	r2 = abs r0 (v)|| [p0+56] = P0;

	R5 = r3 +|+ R2|| [p0+52] = P0;
	r5 = r3 +|+ r2 (Sco)|| [p1] = P0;
	r7 = R0 -|+ r6|| [p1++] = P0;
	r2 = R1 -|+ R3 (S)|| [p1--] = P0;
	R4 = R0 +|- R2|| [p1+48] = P0;
	R5 = r1 +|- r2 (CO)|| [p1+44] = P0;
	r6 = r3 -|- R4|| [p1+40] = P0;
	r7 = R5 -|- R6 (co)|| [p2] = P0;

	r5 = r4 +|+ r3, R7 = r4 -|- r3 (Sco, ASR)|| [p2++] = P0;
	R0 = R3 +|+ r6, R1 = R3 -|- R6 (ASL)|| [p2--] = P0;
	R7 = R1 +|- R2, R6 = R1 -|+ R2 (S)|| [p2+36] = P0;
	r1 = r2 +|- r3, r5 = r2 -|+ r3|| [p2+32] = P0;

	R5 = R0 + R1, R6 = R0 - R1|| [p3] = P0;
	r0 = r7 + r1, r3 = r7 - r1 (s)|| [p3++] = P0;

	r7 = A1 + A0, r5 = A1 - A0|| [p3--] = P0;
	r3 = a0 + a1, r6 = a0 - a1 (s)|| [p3+28] = P0;

	R1 = R3 >>> 15 (V)|| [p3+24] = P0;
	r4 = r0 >>> 4 (v)|| [p4] = P0;
	r5 = r0 << 0 (v,s)|| [p4++] = P0;
	r2 = r2 << 12 (v, S)|| [p4--] = P0;

	R7 = ASHIFT R5 BY R2.L (V)|| [p4+24] = P0;
	r0 = Ashift r2 by r0.L (v, s)|| [p4+20] = P0;

	R5 = r2 >> 15 (V)|| [p4+16] = P0;
	r0 = R1 << 2 (v)|| [p4+12] = P0;

	R4 = lshift r1 by r2.L (v)|| [p5] = P0;

	R6 = MAX (R0, R1) (V)|| [p5++] = P0;
	r0 = min (r2, r7) (v)|| [p5--] = P0;

	r2.h = r7.l * r6.h, r2.l = r7.h * r6.h|| [p5+8] = P0;
	R4.L = R1.L * R0.L, R4.H = R1.H * R0.H|| [p5+4] = P0;
	R0.h = R3.H * r2.l, r0.l=r3.l * r2.l|| [p5] = P0;
	r5.h = r3.h * r2.h (M), r5.l = r3.L * r2.L (fu)|| [sp] = P0;
	R0 = r4.l * r7.l, r1 = r4.h * r7.h (s2rnd)|| [sp++] = P0;
	R7 = R2.l * r5.l, r6 = r2.h * r5.h|| [sp--] = P0;
	R0.L = R7.L * R6.L, R0.H = R7.H * R6.H (ISS2)|| [sp+60] = P0;
	r3.h = r0.h * r1.h, r3.l = r0.l * r1.l (is)|| [fp] = P0;

	a1 = r2.l * r3.h, a0 = r2.h * R3.H|| [fp++] = P0;
	A0 = R1.l * R0.L, A1 += R1.h * R0.h|| [fp--] = P0;
	A1 = R5.h * R7.H, A0 += r5.L * r7.l (w32)|| [fp+0] = P0;
	a1 += r0.H * r1.H, A0 = R0.L * R1.l (is)|| [fp+60] = P0;
	a1 = r3.h * r4.h (m), a0 += r3.l * R4.L (FU)|| [p0] = P1;
	A1 += r4.H * R4.L, a0 -= r4.h * r4.h|| [p0] = P2;

	r0.l = (a0 += R7.l * R6.L), R0.H = (A1 += R7.H * R6.H) (Iss2)|| [p0] = P3;
	r2.H = A1, r2.l = (a0 += r0.L * r1.L) (s2rnd)|| [p0] = P4;
	r7.h = (a1 = r2.h * r1.h), a0 += r2.l * r1.l|| [p0] = P5;
	R2.H = (A1 = R7.L * R6.H), R2.L = (A0 = R7.H * R6.h)|| [p0] = fp;
	r6.L = (A0 = R3.L * r2.L), R6.H = (A1 += R3.H * R2.H)|| [p0] = sp;
	R7.h = (a1 += r6.h * r5.l), r7.l = (a0=r6.h * r5.h)|| [p0] = r1;
	r0.h = (A1 = r7.h * R4.l) (M), R0.l = (a0 += r7.l * r4.l)|| [p0++] = r2;
	R5.H = (a1 = r3.h * r2.h) (m), r5.l= (a0 += r3.l * r2.l) (fu)|| [p1--] = r3;
	r0.h = (A1 += R3.h * R2.h), R0.L = ( A0 = R3.L * R2.L) (is)|| [i0] = r0;

	R3 = (A1 = R6.H * R7.H) (M), A0 -= R6.L * R7.L|| [i0++] = r1;
	r1 = (a1 = r7.l * r4.l) (m), r0 = (a0 += r7.h * r4.h)|| [i0--] = r2;
	R0 = (a0 += r7.l * r6.l), r1 = (a1+= r7.h * r6.h) (ISS2)|| [i1] = r3;
	r4 = (a0 = r6.l * r7.l), r5 = (a1 += r6.h * r7.h)|| [i1++] = r3;
	R7 = (A1 += r3.h * r5.H), R6 = (A0 -= r3.l * r5.l)|| [i1--] = r3;
	r5 = (a1 -= r6.h * r7.h), a0 += r6.l * r7.l|| [i2] = r0;
	R3 = (A1 = r6.h * R7.h), R2 = (A0 = R6.l * r7.l)|| [i2++] = r0;
	R5 = (A1 = r3.h * r7.h) (M), r4 = (A0 += R3.l * r7.l) (fu)|| [i2--] = R0;
	R3 = a1, r2 = (a0 += r0.l *r1.l) (s2rnd)|| [i3] = R7;
	r1 = (a1 += r3.h * r2.h), r0 = (a0 = r3.l * r2.l) (is)|| [i3++] = R7;

	R0 = - R1 (V)|| [i3--] = R6;
	r7 = - r2 (v)|| [p0++p1] = R0;

	R7 = Pack (r0.h, r1.l)|| [p0++p1] = R3;
	r6 = PACK (r1.H, r6.H)|| [p0++p2] = r0;
	R5 = pack (R2.L, R2.H)|| [p0++p3] = r4;
	
	(R0, R1) = search R2 (lt)|| r2 = [p0+4];
	(r6, r7) = Search r0 (LE)|| r5 = [p0--];
	(r3, r6) = SEARCH r1 (Gt)|| r0 = [p0+20];
	(r4, R5) = sEARch r3 (gE)|| r1 = [p0++];

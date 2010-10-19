	.text
	.global add_on_sign
add_on_sign:
	r4.h = r4.l = Sign (R1.h) * R5.h + Sign(r1.L) * R5.L;

	.text
	.global vit_max
vit_max:
	R7 = Vit_Max (R5, r2) (ASL);
	r0 = VIT_MAX (r0, r6) (asr);
	r5.l = vit_max (R3) (asL);
	r2.L = VIT_Max (r2) (Asr);

	.text
	.global vector_abs
vector_abs:
	R5 = ABS R5 (V);
	r2 = abs r0 (v);

	.text
	.global vector_add_sub
vector_add_sub:
	R5 = r3 +|+ R2;
	r5 = r3 +|+ r2 (Sco);
	r7 = R0 -|+ r6;
	r2 = R1 -|+ R3 (S);
	R4 = R0 +|- R2;
	R5 = r1 +|- r2 (CO);
	r6 = r3 -|- R4;
	r7 = R5 -|- R6 (co);

	r5 = r4 +|+ r3, R7 = r4 -|- r3 (Sco, ASR);
	R0 = R3 +|+ r6, R1 = R3 -|- R6 (ASL);
	R7 = R1 +|- R2, R6 = R1 -|+ R2 (S);
	r1 = r2 +|- r3, r5 = r2 -|+ r3;

	R5 = R0 + R1, R6 = R0 - R1;
	r0 = r7 + r1, r3 = r7 - r1 (s);

	r7 = A1 + A0, r5 = A1 - A0;
	r3 = a0 + a1, r6 = a0 - a1 (s);

	.text
	.global vector_ashift
vector_ashift:
	R1 = R3 >>> 15 (V);
	r4 = r0 >>> 4 (v);
	r5 = r0 << 0 (v,s);
	r2 = r2 << 12 (v, S);

	R7 = ASHIFT R5 BY R2.L (V);
	r0 = Ashift r2 by r0.L (v, s);

	.text
	.global vector_lshift
vector_lshift:
	R5 = r2 >> 15 (V);
	r0 = R1 << 2 (v);

	R4 = lshift r1 by r2.L (v);

	.text
	.global vector_max
vector_max:
	R6 = MAX (R0, R1) (V);

	.text
	.global vector_min
vector_min:
	r0 = min (r2, r7) (v);

	.text
	.global vector_mul
vector_mul:
	r2.h = r7.l * r6.h, r2.l = r7.h * r6.h;
	R4.L = R1.L * R0.L, R4.H = R1.H * R0.H;
	R0.h = R3.H * r2.l, r0.l=r3.l * r2.l;
	r5.h = r3.h * r2.h (M), r5.l = r3.L * r2.L (fu);
	R0 = r4.l * r7.l, r1 = r4.h * r7.h (s2rnd);
	R7 = R2.l * r5.l, r6 = r2.h * r5.h;
	R0.L = R7.L * R6.L, R0.H = R7.H * R6.H (ISS2);
	r3.h = r0.h * r1.h, r3.l = r0.l * r1.l (is);

	a1 = r2.l * r3.h, a0 = r2.h * R3.H;
	A0 = R1.l * R0.L, A1 += R1.h * R0.h;
	A1 = R5.h * R7.H, A0 += r5.L * r7.l (w32);
	a1 += r0.H * r1.H, A0 = R0.L * R1.l (is);
	a1 = r3.h * r4.h (m), a0 += r3.l * R4.L (FU);
	A1 += r4.H * R4.L, a0 -= r4.h * r4.h;

	r0.l = (a0 += R7.l * R6.L), R0.H = (A1 += R7.H * R6.H) (Iss2);
	r2.H = A1, r2.l = (a0 += r0.L * r1.L) (s2rnd);
	r7.h = (a1 = r2.h * r1.h), a0 += r2.l * r1.l;
	R2.H = (A1 = R7.L * R6.H), R2.L = (A0 = R7.H * R6.h);
	r6.L = (A0 = R3.L * r2.L), R6.H = (A1 += R3.H * R2.H);
	R7.h = (a1 += r6.h * r5.l), r7.l = (a0=r6.h * r5.h);
	r0.h = (A1 = r7.h * R4.l) (M), R0.l = (a0 += r7.l * r4.l);
	R5.H = (a1 = r3.h * r2.h) (m), r5.l= (a0 += r3.l * r2.l) (fu);
	r0.h = (A1 += R3.h * R2.h), R0.L = ( A0 = R3.L * R2.L) (is);

	R3 = (A1 = R6.H * R7.H) (M), A0 -= R6.L * R7.L;
	r1 = (a1 = r7.l * r4.l) (m), r0 = (a0 += r7.h * r4.h);
	R0 = (a0 += r7.l * r6.l), r1 = (a1+= r7.h * r6.h) (ISS2);
	r4 = (a0 = r6.l * r7.l), r5 = (a1 += r6.h * r7.h);
	R7 = (A1 += r3.h * r5.H), R6 = (A0 -= r3.l * r5.l);
	r5 = (a1 -= r6.h * r7.h), a0 += r6.l * r7.l;
	R3 = (A1 = r6.h * R7.h), R2 = (A0 = R6.l * r7.l);
	R5 = (A1 = r3.h * r7.h) (M), r4 = (A0 += R3.l * r7.l) (fu);
	R3 = a1, r2 = (a0 += r0.l *r1.l) (s2rnd);
	r1 = (a1 += r3.h * r2.h), r0 = (a0 = r3.l * r2.l) (is);

	.text
	.global vector_negate
vector_negate:
	R0 = - R1 (V);
	r7 = - r2 (v);

	.text
	.global vector_pack
vector_pack:
	R7 = Pack (r0.h, r1.l);
	r6 = PACK (r1.H, r6.H);
	R5 = pack (R2.L, R2.H);
	
	.text
	.global vector_search
vector_search:
	(R0, R1) = search R2 (lt);
	(r6, r7) = Search r0 (LE);
	(r3, r6) = SEARCH r1 (Gt);
	(r4, R5) = sEARch r3 (gE);

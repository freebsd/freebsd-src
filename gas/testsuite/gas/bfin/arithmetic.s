	.text
	.global abs
abs:
	a0 = abs a0;
	A0 = ABS A1;
	A1 = Abs a0;
	a1 = aBs A1;
	A1 = abs a1, a0 = ABS A0;
	r0 = abs r2;

	.text
	.global add
add:
	sp = sp + P0;
	SP = SP + P2;
	FP = p1 + fp;

	R7 = r7 + r2 (NS);
	r6 = r6 + r0 (s);
	
	r4.L = R2.h + r0.L (s);
	r5.H = R1.H + R1.L (S);
	r6.L = R6.L + r5.l (NS);

	.text
	.global add_sub_prescale_down
add_sub_prescale_down:
	r4.l = r0 + r1 (RND20);
	R3.H = r5 + r0 (rnd20);
	r1.L = r7 - R5 (rND20);

	.text
	.global add_sub_prescale_up
add_sub_prescale_up:
	r2.L = R0 + R1 (rnd12);
	r7.H = r7 + r6 (RND12);
	r5.l = r3 - R2 (rNd12);
	r2.h = R1 - R2 (Rnd12);

	.text
	.global add_immediate
add_immediate:
	R5 += -64;
	r2 += 63;
	i0 += 2;
	I3 += 2;
	I2 += 4;
	i1 += 4;
	P0 += 4;
	sp += 16;
	FP += -32;

	.text
	.global divide_primitive
divide_primitive:
	divs (r3, r5);
	divq (R3, R5);

	.text
	.global expadj
expadj:
	r6.L = EXPADJ (r5, r4.l);
	R5.l = ExpAdj (r0.h, r1.l);
	R4.L = expadj (R3, R5.L) (V);

	.text
	.global max
max:
	R6 = MAX (r5, R2);
	r0 = max (r1, r3);

	.text 
	.global min
min:
	r5 = mIn (r2, R3);
	R4 = Min (r7, R0);


	.text
	.global modify_decrement
modify_decrement:
	A0 -= A1;
	a0 -= a1 (w32);
	fp -= p2;
	SP -= P0;
	I3 -= M0;
	i1 -= m1;

	.text
	.global modify_increment
modify_increment:
	a0 += a1;
	A0 += A1 (w32);
	Sp += P1 (Brev);
	P5 += Fp (BREV);
	i2 += M2;
	I0 += m0 (brev);
	r7 = ( a0 += a1);
	r6.l = (A0 += a1);
	R0.H = (a0 += A1);

	.text
	.global multiply16
multiply16:
	R0.l = r1.h * r2.l;
	r1.L = r5.H * r0.H (s2rnd);
	r7.l = r3.l * r3.H (FU);
	r4 = r2.H * r5.H (iSS2);
	r0 = r1.l * r3.l (is);
	r6 = R5.H * r0.l;

	r2.h = r7.l * r6.H (M, iu);
	r3.H = r5.H * r0.L;
	R0.H = r1.L * r1.H (M);
	r1 = r7.H * r6.L (M, is);
	R5 = r0.l * r2.h;
	r3 = r6.H * r0.H (m);

	.text
	.global multiply32
multiply32:
	R4 *= r0;
	r7 *= R2;

	.text
	.global multiply_accumulate
multiply_accumulate:
	a0 = r5.l * R7.H (w32);
	a0 = r0.h * r0.l;
	A0 += R2.L * r3.H (FU);
	A0 += r4.h * r1.L;
	a0 -= r7.l * r6.H (Is);
	A0 -= R5.H * r2.H;

	a1 = r1.L * r0.H (M);
	A1 = r2.h * r0.L;
	A1 = R7.H * R6.L (M, W32);
	a1 += r3.l * r2.l (fu);
	a1 += R6.H * r1.L;
	A1 -= r0.L * R3.H (is);
	a1 -= r2.l * r7.h;

	.text
	.global multiply_accumulate_half
multiply_accumulate_half:
	r7.l = (a0 = r6.H * r5.L);
	r0.L = (A0 = r1.h * R2.l) (tfu);
	R2.L = (a0 += r5.L * r4.L);
	r3.l = (A0 += r7.H * r6.h) (T);
	r0.l = (a0 -= r3.h * r2.h);
	r1.l = (a0 -= r5.L * r4.L) (iH);

	r1.H = (a1 = r1.l * R0.H);
	r2.h = (A1 = r0.H * r3.L) (M, Iss2);
	R6.H = (a1 += r7.l * r7.H);
	r7.h = (a1 += R2.L * R3.L) (S2rnd);
	r6.H = (A1 -= R4.h * r2.h);
	r5.h = (a1 -= r3.H * r7.L) (M, tFu);

	.text
	.global multiply_accumulate_data_reg
multiply_accumulate_data_reg:
	R0 = (A0 = R1.L * R2.L);
	R2 = (A0 = r1.l * r2.l) (is);
	r4 = (a0 += r7.h * r6.L);
	r6 = (A0 += R5.L * r3.h) (s2RND);
	R6 = (a0 -= r2.h * r7.l);
	r4 = (A0 -= R0.L * r6.H) (FU);

	r7 = (a1 = r0.h * r1.l);
	R5 = (A1 = r2.H * r3.H) (M, fu);
	R3 = (A1 += r7.l * r5.l);
	r1 = (a1 += r2.h * r7.h) (iss2);
	r3 = (A1 -= r0.l * R0.H);
	R5 = (a1 -= R2.l * R7.h) (m, is);

	.text
	.global negate
negate:
	R5 = - r0;
	r7 = -R2(s);
	R7 = -r2(Ns);
	A0 = -A0;
	a0 = -a1;
	A1 = -A0;
	a1 = -A1;
	a1 = -a1, a0 = -a0;

	.text
	.global round_half
round_half:
	R5.L = r3 (rnd);
	r6.H = r0 (RND);

	.text
	.global saturate
saturate:
	A0 = A0 (S);
	a1 = a1 (s);
	A1 = a1 (S), a0 = A0 (s);

	.text
	.global signbits
signbits:
	R5.l = signbits r0;
	r0.L = SIGNbits r7.H;
	r3.l = signBits A0;
	r7.L = SIGNBITS a1;

	.text
	.global subtract
subtract:
	R5 = R3 - R0;
	R7 = R7 - r0 (S);
	r3 = r2 - r1 (ns);

	r5.l = R6.H - R7.h (s);
	r0.H = r3.l - r3.h (NS);

	.text
	.global subtract_immediate
subtract_immediate:
	I2 -= 2;
	i0 -= 4;


	.text
	.global move_register
move_register:
	r7 = A0.X;
	Fp = B3;
	l2 = R5;
	M2 = i2;
	a1.w = usp;
	r0 = astat;
	r1 = sEQstat;
	R2 = SYScfg;
	R3 = reti;
	R4 = RETX;
	r5 = reTN;
	r6 = rETe;
	R7 = RETS;
	R5 = lc0;
	r4 = Lc1;
	r3 = Lt0;
	r2 = LT1;
	r1 = Lb0;
	r0 = LB1;
	R2 = Cycles;
	R3 = Cycles2;
	r1 = emudat;
	CYCLES = A0.W;
	Rets = Fp;
	Lt1 = USP;
	ASTAT = P2; 
	A0 = A1;
	a1 = a0;
	a0 = R0;
	A1 = r1;

	R4 = A0 (fu);
	r5 = A1 (ISS2);
	R6 = a0;
	R7 = A1;
	R6 = A0, R7 = a1;
	r1 = a1, r0 = a0 (fu);

	.text
	.global move_conditional
move_conditional:
	if cc R5 = P2;
	if !cc Sp = R0;
	
	.text
	.global move_half_to_full_zero_extend
move_half_to_full_zero_extend:
	R2 = r7.L (Z);
	r0 = R1.L (z);
	
	.text
	.global move_half_to_full_sign_extend
move_half_to_full_sign_extend:
	R5 = R1.L (x);
	r3 = r2.L (X);

	.text
	.global move_register_half
move_register_half:
	A0.X = r5.l;
	a1.X = r2.L;
	r0.l = a0.x;
	R7.l = A1.X;
	A0.L = r3.l;
	a1.l = r4.l;
	A0.h = r6.H;
	A1.H = r5.h;
	r0.l = A0 (iu);
	R1.H = A1 (s2rnd);
	r1.h = a1;
	R2.l = A0, r2.H = A1 (IH);
	R2.l = A0, r2.H = A1;
	r0.H = A1, R0.L = a0 (t);
	r0.H = A1, R0.L = a0 (fu);
	r0.H = A1, R0.L = a0 (is);
	r0.H = A1, R0.L = a0;

	.text
	.global move_byte_zero_extend
move_byte_zero_extend:
	R7 = r2.b (z);
	r0 = R1.B (Z);

	.text
	.global move_byte_sign_extend
move_byte_sign_extend:
	r6 = r1.b (Z);
	R5 = R4.B (z);

	.text
	.global compare_data_register
compare_data_register:
	cc = r6 == r0;
	Cc = R7 == r2;
	CC = R3 == -2;
	cc = r0 < r1;
	cC = r4 < -4;
	Cc = r4 <= R5;
	cc = r5 <= 3;
	cc = r6 < r7 (iu);
	cc = R7 < 4 (iu);
	CC = r5 <= R3 (Iu);
	Cc = R2 <= 5 (iU);

	.text
	.global compare_pointer
compare_pointer:
	cc = sp == p0;
	cC = FP == 0;
	CC = FP < SP;
	Cc = r1 < -4;
	CC = R1 <= R2;
	cc = r3 <= 3;
	cC = r5 < R6 (iu);
	Cc = R7 < 7 (Iu);
	cC = r0 <= r1 (iU);
	cc = r2 <= 0 (IU);

	.global compare_accumulator
	.text
compare_accumulator:
	CC = A0 == A1;
	cc = A0 < a1;
	cc = a0 <= a1;

	.text
	.global move_cc
move_cc:
	R0 = cc;
	ac0 |= cc;
	AZ = Cc;
	an = Cc;
	AC1 &= cC;
	v ^= cc;
	V = CC;
	VS |= cC;
	aV0 = cc;
	Av1 &= CC;
	AV1s = cc;
	AQ |= cc;

	CC = R4;
	cc = AZ;
	cc |= An;
	CC &= Ac0;
	Cc ^= aC1;
	CC = V;
	cC |= vS;
	Cc &= AV0;
	cc ^= av1;
	cc = av1s;
	cC |= aQ;


	.text
	.global negate_cc
negate_cc:
	cc = !cc;


	.text

	p0.H = 0x12345678;
	P0.l = 0x12345678;

	CC = R3 < 4;
	CC = R3 < 7;
	CC = R3 < 8;
	CC = R3 <= 4;
	CC = R3 <= 7;
	CC = R3 <= 8;

	A1 -= M2.h * R3.L, A0 -= M2.l * R3.L;

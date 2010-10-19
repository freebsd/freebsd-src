	.text
	.global bitclr
bitclr:
	bitclr(r4, 31);
	bitCLR (r0, 0);

	.text
	.global bitset
bitset:
	BITSET(R2, 30);
	BiTsET (r3, 29);

	.text
	.global bittgl
bittgl:
	bitTGL(r7, 22);
	BITtgl (r6, 16);

	.text
	.global bittst
bittst:
	cc = bittst (r0, 31);
	CC = BITTST (r1, 0);
	cC = BittST (r7, 15);

	.text
	.global deposit
deposit:
	R5 = Deposit (r3, r2);
	r0 = DEPOSIT (r7, R6) (X);

	.text
	.global extract
extract:
	r4 = extract (r2, r1.L) (z);
	R2 = EXTRACT (r0, r2.l) (Z);

	r7 = ExtracT (r3, r4.L) (X);
	r5 = ExtRACt (R6, R1.L) (x);

	.text
	.global bitmux
bitmux:
	BITMUX(R1, R0, A0) (ASR);
	Bitmux (r2, R3, a0) (aSr);

	bitmux (r4, r5, a0) (asl);
	BiTMux (R7, r6, a0) (ASl);

	.text
	.global ones
ones:
	R5.l = ones r0;
	r7.L = Ones R2;

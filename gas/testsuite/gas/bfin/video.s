	.text
	.global align
align:
	R7 = Align8 (r5, r2);
	R5 = ALIGN16 (R0, R1);
	r2 = ALIGN24 (r5, r0);

	.global disalgnexcpt
disalgnexcpt:
	DISAlgnExcpt;

	.text
	.global byteop3p
byteop3p:
	R5 = Byteop3p (r1:0, r3:2) (lO);
	R0 = BYTEOP3P (R1:0, R3:2) (HI);
	R1 = byteop3p (r1:0, r3:2) (LO, r);
	r2 = ByteOp3P (r1:0, R3:2) (hi, R);

	.text
	.global dual16
dual16:
	R5 = A1.l + A1.h, R2 = a0.l + a0.h;

	.text
	.global byteop16p
byteop16p:
	(r2, r3) = BYTEOP16P (R1:0, R3:2);
	(R6, R0) = byteop16p (r1:0, r3:2) (r);

	.text
	.global byteop1p
byteop1p:
	R7 = BYTEOP1P (R1:0, R3:2);
	r2 = byteop1p (r1:0, r3:2) (t);
	R3 = ByteOp1P (r1:0, R3:2) (R);
	r7 = byteOP1P (R1:0, r3:2) (T, r);

	.text
	.global byteop2p
byteop2p:
	R0 = BYTEOP2P (R1:0, R3:2) (RNDL);
	r1 = byteop2p (r1:0, r3:2) (rndh);
	R2 = Byteop2p (R1:0, R3:2) (tL);
	R3 = Byteop2p (r1:0, r3:2) (TH);
	r4 = ByTEOP2P (r1:0, R3:2) (Rndl, R);
	R5 = byTeOp2p (R1:0, r3:2) (rndH, r);
	r6 = BYTEop2p (r1:0, r3:2) (tl, R);
	R7 = byteop2p (r1:0, R3:2) (TH, r);

	.text
	.global bytepack
bytepack:
	R5 = BytePack (R0, R3);

	.text
	.global byteop16m
byteop16m:
	(R6, R2) = ByteOp16M (r1:0, r3:2);
	(r0, r5) = byteop16m (R1:0, R3:2) (r);

	.text
	.global saa
saa:
	saa(r1:0, r3:2);
	SAA (R1:0, R3:2) (r);

	.text
	.global byteunpack
byteunpack:
	(R7, R2) = byteunpack R1:0;
	(R6, R4) = BYTEUNPACK r3:2 (R);



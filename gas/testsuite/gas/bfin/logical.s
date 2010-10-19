	.text
	.global and
and:
	r7 = r0 & r1;
	R2 = R3 & R3;
	r6 = r1 & R2;

	.text
	.global not
not:
	r0 = ~R1;
	R1 = ~r2;
	r3 = ~r4;
	R4 = ~R5;

	.text
	.global or
or:
	r0 = r0 | r1;
	r2 = R3 | R4;
	R5 = r6 | R7;

	.text
	.global xor
xor:
	r5 = r5 ^ r3;
	r4 = R2 ^ r0;
	R0 = R1 ^ R0;


	.text
	.global bxor
bxor:
	R7.l = CC = bxor (a0, r0);
	r7.l = cc = BXOR (A0, R1);

	r5.L = Cc = BxoR (A0, A1, CC);
	R4.L = cC = bXor (a0, a1, cc);

	.text
	.global bxorshift
bxorshift:
	r3.l = cc = bxorshift (a0, R7);
	R2.l = cC = BxoRsHIft (A0, R2);

	A0 = BXORSHIFT (A0, A1, CC);
	a0 = BxorShift (a0, A1, Cc);



	

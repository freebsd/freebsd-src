	.text
	.global store_pointer_register
store_pointer_register:
	[FP] = P0;
	[Sp ++] = p1;
	[fp --] = P5;
	[p2 + 60] = Sp;
	[P5 + 131068] = P0;
	[Fp -0]= p2;
	[fp -4] = P1;
	[Fp - 128] = p0;

	.text
	.global store_data_register
store_data_register:
	[p2] = r0;
	[P5 ++] = R2;
	[fp--] = R7;
	[SP + 56] = R5;
	[sp+0xeff0]=R3;
	[FP - 0xfffc] = R0;
	[fp ++ P1] = r1;
	[FP - 96] = r6;

	[i0] = r1;
	[I2++] = R2;
	[i3--] = R4;	
	[i1 ++ m0] = r7;

	.text
	.global store_data_register_half
store_data_register_half:
	w [ i3] = R4.h;
	W[I0++] = r0.h;
	W [ i2--] = r7.H;
	w[Sp] = R6.h;
	W [ Fp++P0] = r4.h;

	.text
	.global store_low_data_register_half
store_low_data_register_half:
	W [I0] = r0.l;
	w [i1++] = r7.L;
	W[I2--] = R1.l;
	w [SP] = r2.l;
	W[P2] = r3;
	w [p3 ++ ] = R5;
	W [fp--] = R4;
	W [P1+30]=r7;
	w[p2+0xfffe] = R6;
	w [FP-0xbcd0] = r1;
	W [sp ++ P2] = r5.L;

	.text
	.global store_byte
store_byte:
	b [Fp] = R1;
	B[P0++] = r0;
	B [fp --] = r2;
	B [ p2 + 25] = R7;
	b[FP - 0x7FFF] = r6;

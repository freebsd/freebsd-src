	.extern f001
	.extern F002
	.text
	.global load_immediate
load_immediate:
	/* Half-Word Load.  */
	M3.l = 0xffff;
	b2.l = 0xfffe;
	Sp.l = 0;
	FP.L = 0xfedc;
	r0.h = 2;
	p5.H = 32;
	I2.h = 0xf204;
	b1.H = 64;
	l0.h = 0xffff;
	R5.h = load_data1;
	B2.H = F002;

	/* Zero Extended.  */
	fp = 0xff20 (Z);
	l2 = 32 (z);
	R5 = foo2 (Z);
	A0 = 0;
	A1 = 0;
	a1 = a0 = 0;

	/* Sign Extended.  */
	r2 = -64 (x);
	R0 = 0x7f (X);
	P2 = 0 (x);
	sp = -32 (x);
	fp = 44 (X);
	l3 = 0x800 (x);
	m2 = 0x7fff (X);
	R1 = 16 (X);
	L0 = foo1;
	r7 = load_data2;

	.text
	.global load_pointer_register
load_pointer_register:
	Sp = [ fp];
	FP = [ p0++ ];
	p1 = [sp--];
	SP = [P2 +56];
	p3 = [fp + 0];
	P4 = [FP + 0x0001FFFC];
	sp = [fp-0x0001fffc];
	sp = [p4-0];
	P5 = [FP-128];
	

	.text
	.global load_data_register
load_data_register:
	R7 = [p0];
	r6 = [p5++];
	r5 = [P4 --];
	R4 = [Fp + 40];
	r3 = [sp+131068];
	r2 = [sp-0];
	r1 = [fp - 0x0001fffc];
	R0 = [sp ++ p0];
	R5 = [Fp-128];
	r2 = [i0];
	r1 = [I1++];
	R3 = [I2--];
	R4 = [i3 ++ M0];

	.text
	.global load_half_word_zero_extend
load_half_word_zero_extend:
	r7 = w [sp] (z);
	R6 = W [FP ++] (Z);
	R5 = W [P0 --] (z);
	R4 = w [p1 + 30] (Z);
	r3 = w [sp + 0xfffc] (z);
	r2 = w [fp - 0xfffc] (Z);
	R0 = W [ P0 ++ P5] (z);

	.text
	.global load_half_word_sign_extend
load_half_word_sign_extend:
	r7 = w [sp] (x);
	R6 = W [FP ++] (X);
	R5 = W [P0 --] (X);
	r5 = w [p1 + 24] (x);
	R3 = w [sp + 0xfffc] (X);
	r7 = w [fp - 0xfffc] (x);
	R1 = W [ P1 ++ P2] (X);
	
	.text
	.global load_high_data_register_half
load_high_data_register_half:
	r0.h = w [i0];
	R1.H = W [I1 ++];
	R2.h = w [I2 --];
	r3.H = W [sp];
	R4.h = W [Fp ++ p0];

	.text
	.global load_low_data_register_half
load_low_data_register_half:
	r7.l = w [i3];
	R6.L = W [I2++];
	R5.l = w [i1 --];
	r4.L = w [P0];
	r3.l = W [p2 ++ p3];

	.text
	.global load_byte_zero_extend
load_byte_zero_extend:
	r5 = b [p0] (z);
	R4 = B [P1++] (Z);
	r0 = b [p2--] (z);
	R3 = B [sp + 0x7fff] (Z);
	r7 = b [SP - 32767] (z);

	.text
	.global load_byte_sign_extend
load_byte_sign_extend:
	r5 = b [ P0 ] (X);
	r2 = B [ p1++ ] (x);
	R3 = b [ FP--] (x);
	r7 = B [ sp+0] (x);
	r6 = b [fp-0x7fff] (X);

	.global load_data
load_data1:	.byte 0
load_data2:	.word 16


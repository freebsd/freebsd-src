#objdump: -dr
#name: move
.*: +file format .*

Disassembly of section .text:

00000000 <move_register>:
   0:	38 31       	R7=A0.x;
   2:	fb 32       	FP=B3;
   4:	35 36       	L2=R5;
   6:	b2 34       	M2=I2;
   8:	d8 39       	A1.w=USP;
   a:	06 31       	R0=ASTAT;
   c:	c9 31       	R1=SEQSTAT;
   e:	d2 31       	R2=SYSCFG;
  10:	db 31       	R3=RETI;
  12:	e4 31       	R4=RETX;
  14:	ed 31       	R5=RETN;
  16:	f6 31       	R6=RETE;
  18:	3f 31       	R7=RETS;
  1a:	a8 31       	R5=LC0;
  1c:	a3 31       	R4=LC1;
  1e:	99 31       	R3=LT0;
  20:	94 31       	R2=LT1;
  22:	8a 31       	R1=LB0;
  24:	85 31       	R0=LB1;
  26:	96 31       	R2=CYCLES;
  28:	9f 31       	R3=CYCLES2;
  2a:	cf 31       	R1=EMUDAT;
  2c:	31 3d       	CYCLES=A0.w;
  2e:	7f 38       	RETS=FP;
  30:	e0 3d       	LT1=USP;
  32:	72 38       	ASTAT=P2;
  34:	08 c4 [0|3][0|f] c0 	A0=A1;
  38:	08 c4 [0|3][0|f] e0 	A1=A0;
  3c:	09 c4 00 20 	A0=R0;
  40:	09 c4 08 a0 	A1=R1;
  44:	8b c0 00 39 	R4 = A0 \(FU\);
  48:	2f c1 00 19 	R5 = A1 \(ISS2\);
  4c:	0b c0 80 39 	R6 = A0;
  50:	0f c0 80 19 	R7 = A1;
  54:	0f c0 80 39 	R7 = A1, R6 = A0;
  58:	8f c0 00 38 	R1 = A1, R0 = A0 \(FU\);

0000005c <move_conditional>:
  5c:	6a 07       	IF CC R5 = P2;
  5e:	b0 06       	IF ! CC SP = R0;

00000060 <move_half_to_full_zero_extend>:
  60:	fa 42       	R2=R7.L\(Z\);
  62:	c8 42       	R0=R1.L\(Z\);

00000064 <move_half_to_full_sign_extend>:
  64:	8d 42       	R5=R1.L\(X\);
  66:	93 42       	R3=R2.L\(X\);

00000068 <move_register_half>:
  68:	09 c4 28 40 	A0.x=R5.L;
  6c:	09 c4 10 c0 	A1.x=R2.L;
  70:	0a c4 [0|3][0|6] 00 	R0.L=A0.x;
  74:	0a c4 [0|3][0|6] 4e 	R7.L=A1.x;
  78:	09 c4 18 00 	A0.L=R3.L;
  7c:	09 c4 20 80 	A1.L=R4.L;
  80:	29 c4 30 00 	A0.H=R6.H;
  84:	29 c4 28 80 	A1.H=R5.H;
  88:	83 c1 00 38 	R0.L = A0 \(IU\);
  8c:	27 c0 40 18 	R1.H = A1 \(S2RND\);
  90:	07 c0 40 18 	R1.H = A1;
  94:	67 c1 80 38 	R2.H = A1, R2.L = A0 \(IH\);
  98:	07 c0 80 38 	R2.H = A1, R2.L = A0;
  9c:	47 c0 00 38 	R0.H = A1, R0.L = A0 \(T\);
  a0:	87 c0 00 38 	R0.H = A1, R0.L = A0 \(FU\);
  a4:	07 c1 00 38 	R0.H = A1, R0.L = A0 \(IS\);
  a8:	07 c0 00 38 	R0.H = A1, R0.L = A0;

000000ac <move_byte_zero_extend>:
  ac:	57 43       	R7=R2.B\(Z\);
  ae:	48 43       	R0=R1.B\(Z\);

000000b0 <move_byte_sign_extend>:
  b0:	4e 43       	R6=R1.B\(Z\);
  b2:	65 43       	R5=R4.B\(Z\);

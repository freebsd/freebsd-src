#objdump: -d
#name: load
.*: +file format .*

Disassembly of section .text:

00000000 <load_immediate>:
   0:	17 e1 ff ff 	M3.L=ffff.*
   4:	1a e1 fe ff 	B2.L=fffe.*
   8:	0e e1 00 00 	SP.L=0.*
   c:	0f e1 dc fe 	FP.L=fedc.*
  10:	40 e1 02 00 	R0.H=0x2;
  14:	4d e1 20 00 	P5.H=20.*
  18:	52 e1 04 f2 	I2.H=f204.*
  1c:	59 e1 40 00 	B1.H=40.*
  20:	5c e1 ff ff 	L0.H=ffff.*
  24:	45 e1 00 00 	R5.H=0x0;
  28:	5a e1 00 00 	B2.H=0 <load_immediate>;
  2c:	8f e1 20 ff 	FP=ff20.*
  30:	9e e1 20 00 	L2=20.*
  34:	85 e1 00 00 	R5=0 <load_immediate>\(Z\);
  38:	08 c4 [0-3][[:xdigit:]] 00 	A0=0;
  3c:	08 c4 [0-3][[:xdigit:]] 40 	A1=0;
  40:	08 c4 [0-3][[:xdigit:]] 80 	A1=A0=0;
  44:	02 62       	R2=-64\(x\);
  46:	20 e1 7f 00 	R0=0x7f \(X\);
  4a:	02 68       	P2=0x0;
  4c:	06 6b       	SP=-32;
  4e:	67 69       	FP=0x2c;
  50:	3f e1 00 08 	L3=0x800 \(X\);
  54:	36 e1 ff 7f 	M2=0x7fff \(X\);
  58:	81 60       	R1=0x10\(x\);
  5a:	3c e1 00 00 	L0=0x0 \(X\);
  5e:	27 e1 f3 00 	R7=0xf3 \(X\);
  62:	00 e1 03 00 	R0.L=0x3;
  66:	01 e1 0f 00 	R1.L=0xf;

0000006a <load_pointer_register>:
  6a:	7e 91       	SP=\[FP\];
  6c:	47 90       	FP=\[P0\+\+\];
  6e:	f1 90       	P1=\[SP--\];
  70:	96 af       	SP=\[P2\+0x38\];
  72:	3b ac       	P3=\[FP\+0x0\];
  74:	3c e5 ff 7f 	P4=\[FP\+0x1fffc\];
  78:	3e e5 01 80 	SP=\[FP\+-131068\];
  7c:	26 ac       	SP=\[P4\+0x0\];
  7e:	0d b8       	P5=\[FP-128\];

00000080 <load_data_register>:
  80:	07 91       	R7=\[P0\];
  82:	2e 90       	R6=\[P5\+\+\];
  84:	a5 90       	R5=\[P4--\];
  86:	bc a2       	R4=\[FP\+0x28\];
  88:	33 e4 ff 7f 	R3=\[SP\+0x1fffc\];
  8c:	32 a0       	R2=\[SP\+0x0\];
  8e:	39 e4 01 80 	R1=\[FP\+-131068\];
  92:	06 80       	R0=\[SP\+\+P0\];
  94:	05 b8       	R5=\[FP-128\];
  96:	02 9d       	R2=\[I0\];
  98:	09 9c       	R1=\[I1\+\+\];
  9a:	93 9c       	R3=\[I2--\];
  9c:	9c 9d       	R4=\[I3\+\+M0\];

0000009e <load_half_word_zero_extend>:
  9e:	37 95       	R7=W\[SP\] \(Z\);
  a0:	3e 94       	R6=W\[FP\+\+\] \(Z\);
  a2:	85 94       	R5=W\[P0--\] \(Z\);
  a4:	cc a7       	R4=W\[P1\+0x1e\] \(Z\);
  a6:	73 e4 fe 7f 	R3=W\[SP\+0xfffc\] \(Z\);
  aa:	7a e4 02 80 	R2=W\[FP\+-65532\] \(Z\);
  ae:	28 86       	R0=W\[P0\+\+P5\] \(Z\);

000000b0 <load_half_word_sign_extend>:
  b0:	77 95       	R7=W\[SP\]\(X\);
  b2:	7e 94       	R6=W\[FP\+\+\]\(X\);
  b4:	c5 94       	R5=W\[P0--\]\(X\);
  b6:	0d ab       	R5=W\[P1\+0x18\]\(X\);
  b8:	73 e5 fe 7f 	R3=W\[SP\+0xfffc\]\(X\);
  bc:	7f e5 02 80 	R7=W\[FP\+-65532\]\(X\);
  c0:	51 8e       	R1=W\[P1\+\+P2\]\(X\);

000000c2 <load_high_data_register_half>:
  c2:	40 9d       	R0.H=W\[I0\];
  c4:	49 9c       	R1.H=W\[I1\+\+\];
  c6:	d2 9c       	R2.H=W\[I2--\];
  c8:	f6 84       	R3.H=W\[SP\];
  ca:	07 85       	R4.H=W\[FP\+\+P0\];

000000cc <load_low_data_register_half>:
  cc:	3f 9d       	R7.L=W\[I3\];
  ce:	36 9c       	R6.L=W\[I2\+\+\];
  d0:	ad 9c       	R5.L=W\[I1--\];
  d2:	00 83       	R4.L=W\[P0\];
  d4:	da 82       	R3.L=W\[P2\+\+P3\];

000000d6 <load_byte_zero_extend>:
  d6:	05 99       	R5=B\[P0\] \(Z\);
  d8:	0c 98       	R4=B\[P1\+\+\] \(Z\);
  da:	90 98       	R0=B\[P2--\] \(Z\);
  dc:	b3 e4 ff 7f 	R3=B\[SP\+0x7fff\] \(Z\);
  e0:	b7 e4 01 80 	R7=B\[SP\+-32767\] \(Z\);

000000e4 <load_byte_sign_extend>:
  e4:	45 99       	R5=B\[P0\]\(X\);
  e6:	4a 98       	R2=B\[P1\+\+\]\(X\);
  e8:	fb 98       	R3=B\[FP--\]\(X\);
  ea:	b7 e5 00 00 	R7=B\[SP\+0x0\]\(X\);
  ee:	be e5 01 80 	R6=B\[FP\+-32767\]\(X\);

000000f2 <load_data1>:
	...

000000f3 <load_data2>:
  f3:	10 00       	IF ! CC JUMP f3 <load_data2>;
  f5:	00 00       	NOP;
	...

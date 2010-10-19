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
  5e:	27 e1 eb 00 	R7=0xeb \(X\);

00000062 <load_pointer_register>:
  62:	7e 91       	SP=\[FP\];
  64:	47 90       	FP=\[P0\+\+\];
  66:	f1 90       	P1=\[SP--\];
  68:	96 af       	SP=\[P2\+0x38\];
  6a:	3b ac       	P3=\[FP\+0x0];
  6c:	3c e5 ff 7f 	P4=\[FP\+0x1fffc\];
  70:	3e e5 01 80 	SP=\[FP\+-131068\];
  74:	26 ac       	SP=\[P4\+0x0\];
  76:	0d b8       	P5=\[FP-128\];

00000078 <load_data_register>:
  78:	07 91       	R7=\[P0\];
  7a:	2e 90       	R6=\[P5\+\+\];
  7c:	a5 90       	R5=\[P4--\];
  7e:	bc a2       	R4=\[FP\+0x28\];
  80:	33 e4 ff 7f 	R3=\[SP\+0x1fffc\];
  84:	32 a0       	R2=\[SP\+0x0\];
  86:	39 e4 01 80 	R1=\[FP\+-131068\];
  8a:	06 80       	R0=\[SP\+\+P0\];
  8c:	05 b8       	R5=\[FP-128\];
  8e:	02 9d       	R2=\[I0\];
  90:	09 9c       	R1=\[I1\+\+\];
  92:	93 9c       	R3=\[I2--\];
  94:	9c 9d       	R4=\[I3\+\+M0\];

00000096 <load_half_word_zero_extend>:
  96:	37 95       	R7=W\[SP\] \(Z\);
  98:	3e 94       	R6=W\[FP\+\+\] \(Z\);
  9a:	85 94       	R5=W\[P0--\] \(Z\);
  9c:	cc a7       	R4=W\[P1\+0x1e\] \(Z\);
  9e:	73 e4 fe 7f 	R3=W\[SP\+0xfffc\] \(Z\);
  a2:	7a e4 02 80 	R2=W\[FP\+-65532\] \(Z\);
  a6:	28 86       	R0=W\[P0\+\+P5\] \(Z\);

000000a8 <load_half_word_sign_extend>:
  a8:	77 95       	R7=W\[SP\]\(X\);
  aa:	7e 94       	R6=W\[FP\+\+\]\(X\);
  ac:	c5 94       	R5=W\[P0--\]\(X\);
  ae:	0d ab       	R5=W\[P1\+0x18\]\(X\);
  b0:	73 e5 fe 7f 	R3=W\[SP\+0xfffc\]\(X\);
  b4:	7f e5 02 80 	R7=W\[FP\+-65532\]\(X\);
  b8:	51 8e       	R1=W\[P1\+\+P2\]\(X\);

000000ba <load_high_data_register_half>:
  ba:	40 9d       	R0.H=W\[I0\];
  bc:	49 9c       	R1.H=W\[I1\+\+\];
  be:	d2 9c       	R2.H=W\[I2--\];
  c0:	f6 84       	R3.H=W\[SP\];
  c2:	07 85       	R4.H=W\[FP\+\+P0\];

000000c4 <load_low_data_register_half>:
  c4:	3f 9d       	R7.L=W\[I3\];
  c6:	36 9c       	R6.L=W\[I2\+\+\];
  c8:	ad 9c       	R5.L=W\[I1--\];
  ca:	00 83       	R4.L=W\[P0\];
  cc:	da 82       	R3.L=W\[P2\+\+P3\];

000000ce <load_byte_zero_extend>:
  ce:	05 99       	R5=B\[P0\] \(Z\);
  d0:	0c 98       	R4=B\[P1\+\+\] \(Z\);
  d2:	90 98       	R0=B\[P2--\] \(Z\);
  d4:	b3 e4 ff 7f 	R3=B\[SP\+0x7fff\] \(Z\);
  d8:	b7 e4 01 80 	R7=B\[SP\+-32767\] \(Z\);

000000dc <load_byte_sign_extend>:
  dc:	45 99       	R5=B\[P0\]\(X\);
  de:	4a 98       	R2=B\[P1\+\+\]\(X\);
  e0:	fb 98       	R3=B\[FP--\]\(X\);
  e2:	b7 e5 00 00 	R7=B\[SP\+0x0\]\(X\);
  e6:	be e5 01 80 	R6=B\[FP\+-32767\]\(X\);

000000ea <load_data1>:
	...

000000eb <load_data2>:
  eb:	10 00       	IF ! CC JUMP eb <load_data2>;
  ed:	00 00       	NOP;
	...

#objdump: -dr
#name: shift
.*: +file format .*

Disassembly of section .text:

00000000 <add_with_shift>:
   0:	88 45       	P0=\(P0\+P1\)<<1;
   2:	ea 45       	P2=\(P2\+P5\)<<2;
   4:	4f 41       	R7=\(R7\+R1\)<<2;
   6:	03 41       	R3=\(R3\+R0\)<<1;

00000008 <shift_with_add>:
   8:	44 5f       	P5=P4\+\(P0<<2\);
   a:	0a 5c       	P0=P2\+\(P1<<1\);

0000000c <arithmetic_shift>:
   c:	83 c6 08 41 	A0=A0>>0x1f;
  10:	83 c6 f8 00 	A0=A0<<0x1f;
  14:	83 c6 00 50 	A1=A1>>0x0;
  18:	83 c6 00 10 	A1=A1<<0x0;
  1c:	82 c6 fd 4e 	R7=R5<<0x1f\(S\);
  20:	82 c6 52 07 	R3=R2>>>0x16;
  24:	80 c6 7a 52 	R1.L = R2.H << 0xf \(S\);
  28:	80 c6 f2 2b 	R5.H = R2.L >>> 0x2;
  2c:	00 4f       	R0<<=0x0;
  2e:	f9 4d       	R1>>>=0x1f;
  30:	08 40       	R0>>>=R1;
  32:	8a 40       	R2<<=R1;
  34:	00 c6 14 16 	R3.L= ASHIFT R4.H BY R2.L;
  38:	00 c6 07 6e 	R7.H= ASHIFT R7.L BY R0.L\(S\);
  3c:	00 c6 07 6e 	R7.H= ASHIFT R7.L BY R0.L\(S\);
  40:	02 c6 15 0c 	R6= ASHIFT R5 BY R2.L;
  44:	02 c6 0c 40 	R0= ASHIFT R4 BY R1.L\(S\);
  48:	02 c6 1e 44 	R2= ASHIFT R6 BY R3.L\(S\);
  4c:	03 c6 08 00 	A0= ASHIFT A0 BY R1.L;
  50:	03 c6 00 10 	A1= ASHIFT A1 BY R0.L;

00000054 <logical_shift>:
  54:	00 45       	P0=P0>>1;
  56:	d1 44       	P1=P2>>2;
  58:	c9 5a       	P3=P1<<1;
  5a:	6c 44       	P4=P5<<2;
  5c:	f8 4e       	R0>>=0x1f;
  5e:	ff 4f       	R7<<=0x1f;
  60:	80 c6 8a a3 	R1.H = R2.L >> 0xf;
  64:	80 c6 00 8e 	R7.L = R0.L << 0x0;
  68:	82 c6 0d 8b 	R5=R5>>0x1f;
  6c:	82 c6 60 80 	R0=R0<<0xc;
  70:	83 c6 f8 41 	A0=A0>>0x1;
  74:	83 c6 00 00 	A0=A0<<0x0;
  78:	83 c6 f8 10 	A1=A1<<0x1f;
  7c:	83 c6 80 51 	A1=A1>>0x10;
  80:	7d 40       	R5>>=R7;
  82:	86 40       	R6<<=R0;
  84:	00 c6 02 b2 	R1.H= LSHIFT R2.H BY R0.L;
  88:	00 c6 08 90 	R0.L= LSHIFT R0.H BY R1.L;
  8c:	00 c6 16 8e 	R7.L= LSHIFT R6.L BY R2.L;
  90:	02 c6 1c 8a 	R5=SHIFT R4 BY R3.L;
  94:	03 c6 30 40 	A0= LSHIFT A0 BY R6.L;
  98:	03 c6 28 50 	A1= LSHIFT A1 BY R5.L;

0000009c <rotate>:
  9c:	82 c6 07 cf 	R7= ROT R7 BY -32;
  a0:	82 c6 0f cd 	R6= ROT R7 BY -31;
  a4:	82 c6 ff ca 	R5= ROT R7 BY 0x1f;
  a8:	82 c6 f7 c8 	R4= ROT R7 BY 0x1e;
  ac:	83 c6 00 80 	A0= ROT A0 BY 0x0;
  b0:	83 c6 50 80 	A0= ROT A0 BY 0xa;
  b4:	83 c6 60 91 	A1= ROT A1 BY -20;
  b8:	83 c6 00 91 	A1= ROT A1 BY -32;
  bc:	02 c6 11 c0 	R0= ROT R1 BY R2.L;
  c0:	02 c6 1c c0 	R0= ROT R4 BY R3.L;
  c4:	03 c6 38 80 	A0= ROT A0 BY R7.L;
  c8:	03 c6 30 90 	A1= ROT A1 BY R6.L;

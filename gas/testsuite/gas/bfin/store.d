#objdump: -dr
#name: store
.*: +file format .*
Disassembly of section .text:

00000000 <store_pointer_register>:
   0:	78 93       	\[FP\]=P0;
   2:	71 92       	\[SP\+\+\]=P1;
   4:	fd 92       	\[FP--\]=P5;
   6:	d6 bf       	\[P2\+0x3c\]=SP;
   8:	28 e7 ff 7f 	\[P5\+0x1fffc\]=P0;
   c:	3a bc       	\[FP\+0x0\]=P2;
   e:	f9 bb       	\[FP-4\]=P1;
  10:	08 ba       	\[FP-128\]=P0;

00000012 <store_data_register>:
  12:	10 93       	\[P2\]=R0;
  14:	2a 92       	\[P5\+\+\]=R2;
  16:	bf 92       	\[FP--\]=R7;
  18:	b5 b3       	\[SP\+0x38\]=R5;
  1a:	33 e6 fc 3b 	\[SP\+0xeff0\]=R3;
  1e:	38 e6 01 c0 	\[FP\+-65532\]=R0;
  22:	4f 88       	\[FP\+\+P1\]=R1;
  24:	86 ba       	\[FP-96\]=R6;
  26:	01 9f       	\[I0\]=R1;
  28:	12 9e       	\[I2\+\+\]=R2;
  2a:	9c 9e       	\[I3--\]=R4;
  2c:	8f 9f       	\[I1\+\+M0\]=R7;

0000002e <store_data_register_half>:
  2e:	5c 9f       	W\[I3\]=R4.H;
  30:	40 9e       	W\[I0\+\+\]=R0.H;
  32:	d7 9e       	W\[I2--\]=R7.H;
  34:	b6 8d       	W\[SP\]=R6.H;
  36:	07 8d       	W\[FP\+\+P0\]=R4.H;

00000038 <store_low_data_register_half>:
  38:	20 9f       	W\[I0\]=R0.L;
  3a:	2f 9e       	W\[I1\+\+\]=R7.L;
  3c:	b1 9e       	W\[I2--\]=R1.L;
  3e:	b6 8a       	W\[SP\]=R2.L;
  40:	13 97       	W\[P2\]=R3;
  42:	1d 96       	W\[P3\+\+\]=R5;
  44:	bc 96       	W\[FP--\]=R4;
  46:	cf b7       	W\[P1\+0x1e\]=R7;
  48:	56 e6 ff 7f 	W\[P2\+0xfffe\]=R6;
  4c:	79 e6 98 a1 	W\[FP\+-48336\]=R1;
  50:	56 8b       	W\[SP\+\+P2\]=R5.L;

00000052 <store_byte>:
  52:	39 9b       	B\[FP\]=R1;
  54:	00 9a       	B\[P0\+\+\]=R0;
  56:	ba 9a       	B\[FP--\]=R2;
  58:	97 e6 19 00 	B\[P2\+0x19\]=R7;
  5c:	be e6 01 80 	B\[FP\+-32767\]=R6;

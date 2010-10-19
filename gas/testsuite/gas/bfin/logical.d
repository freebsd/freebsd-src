#objdump: -dr
#name: logical
.*: +file format .*

Disassembly of section .text:

00000000 <and>:
   0:	c8 55       	R7=R0&R1;
   2:	9b 54       	R2=R3&R3;
   4:	91 55       	R6=R1&R2;

00000006 <not>:
   6:	c8 43       	R0=~R1;
   8:	d1 43       	R1=~R2;
   a:	e3 43       	R3=~R4;
   c:	ec 43       	R4=~R5;

0000000e <or>:
   e:	08 56       	R0=R0\|R1;
  10:	a3 56       	R2=R3\|R4;
  12:	7e 57       	R5=R6\|R7;

00000014 <xor>:
  14:	5d 59       	R5=R5\^R3;
  16:	02 59       	R4=R2\^R0;
  18:	01 58       	R0=R1\^R0;

0000001a <bxor>:
  1a:	0b c6 00 4e 	R7.L=CC=BXOR\(A0,R0\);
  1e:	0b c6 08 4e 	R7.L=CC=BXOR\(A0,R1\);
  22:	0c c6 00 4a 	R5.L=CC=BXOR\( A0,A1 ,CC \);
  26:	0c c6 00 48 	R4.L=CC=BXOR\( A0,A1 ,CC \);

0000002a <bxorshift>:
  2a:	0b c6 38 06 	R3.L=CC=BXORSHIFT\(A0,R7\);
  2e:	0b c6 10 04 	R2.L=CC=BXORSHIFT\(A0,R2\);
  32:	0c c6 00 00 	A0=BXORSHIFT\(A0,A1 ,CC\);
  36:	0c c6 00 00 	A0=BXORSHIFT\(A0,A1 ,CC\);
	...

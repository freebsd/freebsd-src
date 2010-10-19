#objdump: -dr
#name: logical2
.*: +file format .*


Disassembly of section .text:

00000000 <.text>:
   0:	ff 55       	R7=R7&R7;
   2:	c7 55       	R7=R7&R0;
   4:	cf 55       	R7=R7&R1;
   6:	7f 54       	R1=R7&R7;
   8:	87 54       	R2=R7&R0;
   a:	cf 54       	R3=R7&R1;
   c:	ff 43       	R7=~R7;
   e:	c7 43       	R7=~R0;
  10:	f8 43       	R0=~R7;
  12:	d0 43       	R0=~R2;
  14:	ff 57       	R7=R7\|R7;
  16:	cf 57       	R7=R7\|R1;
  18:	c7 57       	R7=R7\|R0;
  1a:	7f 56       	R1=R7\|R7;
  1c:	8f 56       	R2=R7\|R1;
  1e:	c7 56       	R3=R7\|R0;
  20:	ff 59       	R7=R7\^R7;
  22:	cf 59       	R7=R7\^R1;
  24:	c7 59       	R7=R7\^R0;
  26:	7f 58       	R1=R7\^R7;
  28:	8f 58       	R2=R7\^R1;
  2a:	c7 58       	R3=R7\^R0;
  2c:	0b c6 00 00 	R0.L=CC=BXORSHIFT\(A0,R0\);
  30:	0b c6 08 00 	R0.L=CC=BXORSHIFT\(A0,R1\);
  34:	0b c6 00 06 	R3.L=CC=BXORSHIFT\(A0,R0\);
  38:	0b c6 08 06 	R3.L=CC=BXORSHIFT\(A0,R1\);
  3c:	0b c6 00 40 	R0.L=CC=BXOR\(A0,R0\);
  40:	0b c6 08 40 	R0.L=CC=BXOR\(A0,R1\);
  44:	0b c6 00 46 	R3.L=CC=BXOR\(A0,R0\);
  48:	0b c6 08 46 	R3.L=CC=BXOR\(A0,R1\);
  4c:	0c c6 00 40 	R0.L=CC=BXOR\( A0,A1 ,CC \);
  50:	0c c6 00 40 	R0.L=CC=BXOR\( A0,A1 ,CC \);
  54:	0c c6 00 46 	R3.L=CC=BXOR\( A0,A1 ,CC \);
  58:	0c c6 00 46 	R3.L=CC=BXOR\( A0,A1 ,CC \);
  5c:	0c c6 00 00 	A0=BXORSHIFT\(A0,A1 ,CC\);

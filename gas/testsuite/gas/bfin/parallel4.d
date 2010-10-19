#objdump: -d
#name: parallel4
.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	0d ce 15 0e 	R7=ALIGN8\(R5,R2\) \|\| \[I0\]=R0 \|\| NOP;
   4:	00 9f 00 00 
   8:	0d ce 08 4a 	R5=ALIGN16\(R0,R1\) \|\| \[I0\+\+\]=R0 \|\| NOP;
   c:	00 9e 00 00 
  10:	0d ce 05 84 	R2=ALIGN24\(R5,R0\) \|\| \[I0--\]=R0 \|\| NOP;
  14:	80 9e 00 00 
  18:	12 cc 00 c0 	DISALGNEXCPT \|\| \[I1\]=R0 \|\| NOP;
  1c:	08 9f 00 00 
  20:	17 cc 02 0a 	R5=BYTEOP3P\(R1:0x0,R3:0x2\)\(LO\) \|\| \[I1\+\+\]=R0 \|\| NOP;
  24:	08 9e 00 00 
  28:	37 cc 02 00 	R0=BYTEOP3P\(R1:0x0,R3:0x2\)\(HI\) \|\| \[I1--\]=R0 \|\| NOP;
  2c:	88 9e 00 00 
  30:	17 cc 02 22 	R1=BYTEOP3P\(R1:0x0,R3:0x2\)\(LO, R\) \|\| \[I2\]=R0 \|\| NOP;
  34:	10 9f 00 00 
  38:	37 cc 02 24 	R2=BYTEOP3P\(R1:0x0,R3:0x2\)\(HI, R\) \|\| \[I2\+\+\]=R0 \|\| NOP;
  3c:	10 9e 00 00 
  40:	0c cc 40 45 	R5=A1.L\+A1.H,R2=A0.L\+A0.H \|\| \[I2--\]=R0 \|\| NOP;
  44:	90 9e 00 00 
  48:	15 cc 82 06 	\(R2,R3\)=BYTEOP16P\(R1:0x0,R3:0x2\)  \|\| \[I3\]=R0 \|\| NOP;
  4c:	18 9f 00 00 
  50:	15 cc 82 21 	\(R6,R0\)=BYTEOP16P\(R1:0x0,R3:0x2\) \(R\) \|\| \[I3\+\+\]=R0 \|\| NOP;
  54:	18 9e 00 00 
  58:	14 cc 02 4e 	R7=BYTEOP1P\(R1:0x0,R3:0x2\)\(T\) \|\| \[I3--\]=R0 \|\| NOP;
  5c:	98 9e 00 00 
  60:	14 cc 02 44 	R2=BYTEOP1P\(R1:0x0,R3:0x2\)\(T\) \|\| \[P0\]=R0 \|\| NOP;
  64:	00 93 00 00 
  68:	14 cc 02 26 	R3=BYTEOP1P\(R1:0x0,R3:0x2\)\(R\) \|\| \[P0\+\+\]=R0 \|\| NOP;
  6c:	00 92 00 00 
  70:	14 cc 02 6e 	R7=BYTEOP1P\(R1:0x0,R3:0x2\)\(T, R\) \|\| \[P0--\]=R0 \|\| NOP;
  74:	80 92 00 00 
  78:	16 cc 02 00 	R0=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDL\) \|\| \[P1\]=R0 \|\| NOP;
  7c:	08 93 00 00 
  80:	36 cc 02 02 	R1=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDH\) \|\| \[P1\+\+\]=R0 \|\| NOP;
  84:	08 92 00 00 
  88:	16 cc 02 44 	R2=BYTEOP2P\(R1:0x0,R3:0x2\)\(TL\) \|\| \[P1--\]=R0 \|\| NOP;
  8c:	88 92 00 00 
  90:	36 cc 02 46 	R3=BYTEOP2P\(R1:0x0,R3:0x2\)\(TH\) \|\| \[P2\]=R0 \|\| NOP;
  94:	10 93 00 00 
  98:	16 cc 02 28 	R4=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDL, R\) \|\| \[P2\+\+\]=R0 \|\| NOP;
  9c:	10 92 00 00 
  a0:	36 cc 02 2a 	R5=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDH, R\) \|\| \[P2--\]=R0 \|\| NOP;
  a4:	90 92 00 00 
  a8:	16 cc 02 6c 	R6=BYTEOP2P\(R1:0x0,R3:0x2\)\(TL, R\) \|\| \[P3\]=R0 \|\| NOP;
  ac:	18 93 00 00 
  b0:	36 cc 02 6e 	R7=BYTEOP2P\(R1:0x0,R3:0x2\)\(TH, R\) \|\| \[P3\+\+\]=R0 \|\| NOP;
  b4:	18 92 00 00 
  b8:	18 cc 03 0a 	R5=BYTEPACK\(R0,R3\) \|\| \[P3--\]=R0 \|\| NOP;
  bc:	98 92 00 00 
  c0:	15 cc 82 45 	\(R6,R2\)=BYTEOP16M\(R1:0x0,R3:0x2\)  \|\| \[P4\]=R0 \|\| NOP;
  c4:	20 93 00 00 
  c8:	15 cc 02 6a 	\(R0,R5\)=BYTEOP16M\(R1:0x0,R3:0x2\) \(R\) \|\| \[P4\+\+\]=R0 \|\| NOP;
  cc:	20 92 00 00 
  d0:	12 cc 02 00 	SAA\(R1:0x0,R3:0x2\)  \|\| \[P4--\]=R0 \|\| NOP;
  d4:	a0 92 00 00 
  d8:	12 cc 02 20 	SAA\(R1:0x0,R3:0x2\) \(R\) \|\| \[P5\]=R0 \|\| NOP;
  dc:	28 93 00 00 
  e0:	18 cc c0 45 	\(R7,R2\) = BYTEUNPACK R1:0x0  \|\| \[P5\+\+\]=R0 \|\| NOP;
  e4:	28 92 00 00 
  e8:	18 cc 90 69 	\(R6,R4\) = BYTEUNPACK R3:0x2 \(R\) \|\| \[P5--\]=R0 \|\| NOP;
  ec:	a8 92 00 00 

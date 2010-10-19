#objdump: -dr
#name: video
.*: +file format .*
Disassembly of section .text:

00000000 <align>:
   0:	0d c6 15 0e 	R7=ALIGN8\(R5,R2\);
   4:	0d c6 08 4a 	R5=ALIGN16\(R0,R1\);
   8:	0d c6 05 84 	R2=ALIGN24\(R5,R0\);

0000000c <disalgnexcpt>:
   c:	12 c4 00 c0 	DISALGNEXCPT;

00000010 <byteop3p>:
  10:	17 c4 02 0a 	R5=BYTEOP3P\(R1:0x0,R3:0x2\)\(LO\);
  14:	37 c4 02 00 	R0=BYTEOP3P\(R1:0x0,R3:0x2\)\(HI\);
  18:	17 c4 02 22 	R1=BYTEOP3P\(R1:0x0,R3:0x2\)\(LO, R\);
  1c:	37 c4 02 24 	R2=BYTEOP3P\(R1:0x0,R3:0x2\)\(HI, R\);

00000020 <dual16>:
  20:	0c c4 [4-7][[:xdigit:]] 45 	R5=A1.L\+A1.H,R2=A0.L\+A0.H;

00000024 <byteop16p>:
  24:	15 c4 82 06 	\(R2,R3\)=BYTEOP16P\(R1:0x0,R3:0x2\) ;
  28:	15 c4 82 21 	\(R6,R0\)=BYTEOP16P\(R1:0x0,R3:0x2\) \(R\);

0000002c <byteop1p>:
  2c:	14 c4 02 0e 	R7=BYTEOP1P\(R1:0x0,R3:0x2\);
  30:	14 c4 02 44 	R2=BYTEOP1P\(R1:0x0,R3:0x2\)\(T\);
  34:	14 c4 02 26 	R3=BYTEOP1P\(R1:0x0,R3:0x2\)\(R\);
  38:	14 c4 02 6e 	R7=BYTEOP1P\(R1:0x0,R3:0x2\)\(T, R\);

0000003c <byteop2p>:
  3c:	16 c4 02 00 	R0=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDL\);
  40:	36 c4 02 02 	R1=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDH\);
  44:	16 c4 02 44 	R2=BYTEOP2P\(R1:0x0,R3:0x2\)\(TL\);
  48:	36 c4 02 46 	R3=BYTEOP2P\(R1:0x0,R3:0x2\)\(TH\);
  4c:	16 c4 02 28 	R4=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDL, R\);
  50:	36 c4 02 2a 	R5=BYTEOP2P\(R1:0x0,R3:0x2\)\(RNDH, R\);
  54:	16 c4 02 6c 	R6=BYTEOP2P\(R1:0x0,R3:0x2\)\(TL, R\);
  58:	36 c4 02 6e 	R7=BYTEOP2P\(R1:0x0,R3:0x2\)\(TH, R\);

0000005c <bytepack>:
  5c:	18 c4 03 0a 	R5=BYTEPACK\(R0,R3\);

00000060 <byteop16m>:
  60:	15 c4 82 45 	\(R6,R2\)=BYTEOP16M\(R1:0x0,R3:0x2\) ;
  64:	15 c4 02 6a 	\(R0,R5\)=BYTEOP16M\(R1:0x0,R3:0x2\) \(R\);

00000068 <saa>:
  68:	12 c4 02 00 	SAA\(R1:0x0,R3:0x2\) ;
  6c:	12 c4 02 20 	SAA\(R1:0x0,R3:0x2\) \(R\);

00000070 <byteunpack>:
  70:	18 c4 c0 45 	\(R7,R2\) = BYTEUNPACK R1:0x0 ;
  74:	18 c4 90 69 	\(R6,R4\) = BYTEUNPACK R3:0x2 \(R\);

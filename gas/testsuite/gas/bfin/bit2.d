#objdump: -dr
#name: bit2
.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	07 4c       	BITCLR \(R7,0x0\);
   2:	ff 4c       	BITCLR \(R7,0x1f\);
   4:	7f 4c       	BITCLR \(R7,0xf\);
   6:	01 4c       	BITCLR \(R1,0x0\);
   8:	0a 4c       	BITCLR \(R2,0x1\);
   a:	9b 4c       	BITCLR \(R3,0x13\);
   c:	07 4a       	BITSET \(R7,0x0\);
   e:	ff 4a       	BITSET \(R7,0x1f\);
  10:	7f 4a       	BITSET \(R7,0xf\);
  12:	01 4a       	BITSET \(R1,0x0\);
  14:	0a 4a       	BITSET \(R2,0x1\);
  16:	9b 4a       	BITSET \(R3,0x13\);
  18:	07 4b       	BITTGL \(R7,0x0\);
  1a:	ff 4b       	BITTGL \(R7,0x1f\);
  1c:	7f 4b       	BITTGL \(R7,0xf\);
  1e:	01 4b       	BITTGL \(R1,0x0\);
  20:	0a 4b       	BITTGL \(R2,0x1\);
  22:	9b 4b       	BITTGL \(R3,0x13\);
  24:	07 49       	CC = BITTST \(R7,0x0\);
  26:	ff 49       	CC = BITTST \(R7,0x1f\);
  28:	7f 49       	CC = BITTST \(R7,0xf\);
  2a:	01 49       	CC = BITTST \(R1,0x0\);
  2c:	0a 49       	CC = BITTST \(R2,0x1\);
  2e:	9b 49       	CC = BITTST \(R3,0x13\);
  30:	07 48       	CC = ! BITTST \(R7,0x0\);
  32:	ff 48       	CC = ! BITTST \(R7,0x1f\);
  34:	7f 48       	CC = ! BITTST \(R7,0xf\);
  36:	01 48       	CC = ! BITTST \(R1,0x0\);
  38:	0a 48       	CC = ! BITTST \(R2,0x1\);
  3a:	9b 48       	CC = ! BITTST \(R3,0x13\);
  3c:	0a c6 08 8e 	R7=DEPOSIT\(R0,R1\);
  40:	0a c6 0f 8e 	R7=DEPOSIT\(R7,R1\);
  44:	0a c6 3f 8e 	R7=DEPOSIT\(R7,R7\);
  48:	0a c6 08 82 	R1=DEPOSIT\(R0,R1\);
  4c:	0a c6 0f 84 	R2=DEPOSIT\(R7,R1\);
  50:	0a c6 3f 86 	R3=DEPOSIT\(R7,R7\);
  54:	0a c6 08 ce 	R7=DEPOSIT\(R0,R1\)\(X\);
  58:	0a c6 0f ce 	R7=DEPOSIT\(R7,R1\)\(X\);
  5c:	0a c6 3f ce 	R7=DEPOSIT\(R7,R7\)\(X\);
  60:	0a c6 08 c2 	R1=DEPOSIT\(R0,R1\)\(X\);
  64:	0a c6 0f c4 	R2=DEPOSIT\(R7,R1\)\(X\);
  68:	0a c6 3f c6 	R3=DEPOSIT\(R7,R7\)\(X\);
  6c:	0a c6 08 0e 	R7=EXTRACT\(R0,R1.L\) \(Z\);
  70:	0a c6 0f 0e 	R7=EXTRACT\(R7,R1.L\) \(Z\);
  74:	0a c6 3f 0e 	R7=EXTRACT\(R7,R7.L\) \(Z\);
  78:	0a c6 08 02 	R1=EXTRACT\(R0,R1.L\) \(Z\);
  7c:	0a c6 0f 04 	R2=EXTRACT\(R7,R1.L\) \(Z\);
  80:	0a c6 3f 06 	R3=EXTRACT\(R7,R7.L\) \(Z\);
  84:	0a c6 08 4e 	R7=EXTRACT\(R0,R1.L\)\(X\);
  88:	0a c6 0f 4e 	R7=EXTRACT\(R7,R1.L\)\(X\);
  8c:	0a c6 3f 4e 	R7=EXTRACT\(R7,R7.L\)\(X\);
  90:	0a c6 08 42 	R1=EXTRACT\(R0,R1.L\)\(X\);
  94:	0a c6 0f 44 	R2=EXTRACT\(R7,R1.L\)\(X\);
  98:	0a c6 3f 46 	R3=EXTRACT\(R7,R7.L\)\(X\);
  9c:	08 c6 01 00 	BITMUX \(R0,R1,A0 \)\(ASR\);
  a0:	08 c6 02 00 	BITMUX \(R0,R2,A0 \)\(ASR\);
  a4:	08 c6 0b 00 	BITMUX \(R1,R3,A0 \)\(ASR\);
  a8:	08 c6 01 40 	BITMUX \(R0,R1,A0 \)\(ASL\);
  ac:	08 c6 0a 40 	BITMUX \(R1,R2,A0 \)\(ASL\);
  b0:	06 c6 00 c0 	R0.L=ONES R0;
  b4:	06 c6 01 c0 	R0.L=ONES R1;
  b8:	06 c6 06 c2 	R1.L=ONES R6;
  bc:	06 c6 07 c4 	R2.L=ONES R7;

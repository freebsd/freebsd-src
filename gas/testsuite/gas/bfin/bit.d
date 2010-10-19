#objdump: -dr
#name: bit
.*: +file format .*
Disassembly of section .text:

00000000 <bitclr>:
   0:	fc 4c       	BITCLR \(R4,0x1f\);
   2:	00 4c       	BITCLR \(R0,0x0\);

00000004 <bitset>:
   4:	f2 4a       	BITSET \(R2,0x1e\);
   6:	eb 4a       	BITSET \(R3,0x1d\);

00000008 <bittgl>:
   8:	b7 4b       	BITTGL \(R7,0x16\);
   a:	86 4b       	BITTGL \(R6,0x10\);

0000000c <bittst>:
   c:	f8 49       	CC = BITTST \(R0,0x1f\);
   e:	01 49       	CC = BITTST \(R1,0x0\);
  10:	7f 49       	CC = BITTST \(R7,0xf\);

00000012 <deposit>:
  12:	0a c6 13 8a 	R5=DEPOSIT\(R3,R2\);
  16:	0a c6 37 c0 	R0=DEPOSIT\(R7,R6\)\(X\);

0000001a <extract>:
  1a:	0a c6 0a 08 	R4=EXTRACT\(R2,R1.L\) \(Z\);
  1e:	0a c6 10 04 	R2=EXTRACT\(R0,R2.L\) \(Z\);
  22:	0a c6 23 4e 	R7=EXTRACT\(R3,R4.L\)\(X\);
  26:	0a c6 0e 4a 	R5=EXTRACT\(R6,R1.L\)\(X\);

0000002a <bitmux>:
  2a:	08 c6 08 00 	BITMUX \(R1,R0,A0 \)\(ASR\);
  2e:	08 c6 13 00 	BITMUX \(R2,R3,A0 \)\(ASR\);
  32:	08 c6 25 40 	BITMUX \(R4,R5,A0 \)\(ASL\);
  36:	08 c6 3e 40 	BITMUX \(R7,R6,A0 \)\(ASL\);

0000003a <ones>:
  3a:	06 c6 00 ca 	R5.L=ONES R0;
  3e:	06 c6 02 ce 	R7.L=ONES R2;
	...

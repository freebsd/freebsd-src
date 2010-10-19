#objdump: -dr
#name: control_code
.*: +file format .*
Disassembly of section .text:

00000000 <compare_data_register>:
   0:	06 08       	CC=R6==R0;
   2:	17 08       	CC=R7==R2;
   4:	33 0c       	CC=R3==-2;
   6:	88 08       	CC=R0<R1;
   8:	a4 0c       	CC=R4<-4;
   a:	2c 09       	CC=R4<=R5;
   c:	1d 0d       	CC=R5<=0x3;
   e:	be 09       	CC=R6<R7\(IU\);
  10:	a7 0d       	CC=R7<0x4\(IU\);
  12:	1d 0a       	CC=R5<=R3\(IU\);
  14:	2a 0e       	CC=R2<=0x5\(IU\);

00000016 <compare_pointer>:
  16:	46 08       	CC=SP==P0;
  18:	47 0c       	CC=FP==0x0;
  1a:	f7 08       	CC=FP<SP;
  1c:	a1 0c       	CC=R1<-4;
  1e:	11 09       	CC=R1<=R2;
  20:	1b 0d       	CC=R3<=0x3;
  22:	b5 09       	CC=R5<R6\(IU\);
  24:	bf 0d       	CC=R7<0x7\(IU\);
  26:	08 0a       	CC=R0<=R1\(IU\);
  28:	02 0e       	CC=R2<=0x0\(IU\);

0000002a <compare_accumulator>:
  2a:	80 0a       	CC=A0==A1;
  2c:	00 0b       	CC=A0<A1;
  2e:	80 0b       	CC=A0<=A1;

00000030 <move_cc>:
  30:	00 02       	R0=CC;
  32:	ac 03       	AC0\|=CC;
  34:	80 03       	AZ=CC;
  36:	81 03       	AN=CC;
  38:	cd 03       	AC1&=CC;
  3a:	f8 03       	V\^=CC;
  3c:	98 03       	V=CC;
  3e:	b9 03       	VS\|=CC;
  40:	90 03       	AV0=CC;
  42:	d2 03       	AV1&=CC;
  44:	93 03       	AV1S=CC;
  46:	a6 03       	AQ\|=CC;
  48:	0c 02       	CC=R4;
  4a:	00 03       	CC = AZ;
  4c:	21 03       	CC\|=AN;
  4e:	4c 03       	CC&=AC0;
  50:	6d 03       	CC\^=AC1;
  52:	18 03       	CC = V;
  54:	39 03       	CC\|=VS;
  56:	50 03       	CC&=AV0;
  58:	72 03       	CC\^=AV1;
  5a:	13 03       	CC = AV1S;
  5c:	26 03       	CC\|=AQ;

0000005e <negate_cc>:
  5e:	18 02       	CC=!CC;

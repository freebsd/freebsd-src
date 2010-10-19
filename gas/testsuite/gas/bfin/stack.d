#objdump: -dr
#name: stack
.*: +file format .*
Disassembly of section .text:

00000000 <push>:
   0:	7a 01       	\[--SP\] = SYSCFG;
   2:	70 01       	\[--SP\] = LC0;
   4:	47 01       	\[--SP\] = R7;
   6:	61 01       	\[--SP\] = A0.w;
   8:	76 01       	\[--SP\] = CYCLES;
   a:	5a 01       	\[--SP\] = B2;
   c:	55 01       	\[--SP\] = M1;
   e:	48 01       	\[--SP\] = P0;

00000010 <push_multiple>:
  10:	d0 05       	\[--SP\] = \(R7:2, P5:0\);
  12:	70 05       	\[--SP\] = \(R7:6\);
  14:	c2 04       	\[--SP\] = \(P5:2\);

00000016 <pop>:
  16:	38 01       	USP = \[SP\+\+\];
  18:	3b 01       	RETI = \[SP\+\+\];
  1a:	10 01       	I0 = \[SP\+\+\];
  1c:	39 01       	SEQSTAT = \[SP\+\+\];
  1e:	1e 01       	L2 = \[SP\+\+\];
  20:	35 90       	R5=\[SP\+\+\];
  22:	77 90       	FP=\[SP\+\+\];

00000024 <pop_multiple>:
  24:	a8 05       	\(R7:5, P5:0\) = \[SP\+\+\];
  26:	30 05       	\(R7:6\) = \[SP\+\+\];
  28:	84 04       	\(P5:4\) = \[SP\+\+\];

0000002a <link>:
  2a:	00 e8 02 00 	LINK 0x8;
  2e:	00 e8 ff ff 	LINK 0x3fffc;
  32:	00 e8 01 80 	LINK 0x20004;

00000036 <unlink>:
  36:	01 e8 00 00 	UNLINK;
	...

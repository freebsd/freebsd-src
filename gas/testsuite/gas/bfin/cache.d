#objdump: -dr
#name: cache
.*: +file format .*
Disassembly of section .text:

00000000 <prefetch>:
   0:	45 02       	PREFETCH\[P5\];
   2:	67 02       	PREFETCH\[FP\+\+\];
   4:	46 02       	PREFETCH\[SP\];

00000006 <flush>:
   6:	52 02       	FLUSH\[P2\];
   8:	76 02       	FLUSH\[SP\+\+\];

0000000a <flushinv>:
   a:	6c 02       	FLUSHINV\[P4\+\+\];
   c:	4f 02       	FLUSHINV\[FP\];

0000000e <iflush>:
   e:	5b 02       	IFLUSH\[P3\];
  10:	7f 02       	IFLUSH\[FP\+\+\];
	...

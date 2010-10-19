#objdump: -dr
#name: event2
.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	20 00       	IDLE;
   2:	23 00       	CSYNC;
   4:	24 00       	SSYNC;
   6:	25 00       	EMUEXCPT;
   8:	30 00       	CLI  R0;
   a:	31 00       	CLI  R1;
   c:	32 00       	CLI  R2;
   e:	40 00       	STI  R0;
  10:	41 00       	STI  R1;
  12:	42 00       	STI  R2;
  14:	90 00       	RAISE  0x0;
  16:	94 00       	RAISE  0x4;
  18:	9f 00       	RAISE  0xf;
  1a:	a0 00       	EXCPT  0x0;
  1c:	a1 00       	EXCPT  0x1;
  1e:	af 00       	EXCPT  0xf;
  20:	b0 00       	TESTSET  \(P0\);
  22:	b1 00       	TESTSET  \(P1\);
  24:	b2 00       	TESTSET  \(P2\);
  26:	00 00       	NOP;
  28:	03 c0 00 18 	mnop;

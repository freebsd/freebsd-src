#objdump: -dr
#name: event
.*: +file format .*

Disassembly of section .text:

00000000 <idle>:
   0:	20 00       	IDLE;

00000002 <csync>:
   2:	23 00       	CSYNC;

00000004 <ssync>:
   4:	24 00       	SSYNC;

00000006 <emuexcpt>:
   6:	25 00       	EMUEXCPT;

00000008 <cli>:
   8:	37 00       	CLI  R7;
   a:	30 00       	CLI  R0;

0000000c <sti>:
   c:	41 00       	STI  R1;
   e:	42 00       	STI  R2;

00000010 <raise>:
  10:	9f 00       	RAISE  0xf;
  12:	90 00       	RAISE  0x0;

00000014 <excpt>:
  14:	af 00       	EXCPT  0xf;
  16:	a0 00       	EXCPT  0x0;

00000018 <testset>:
  18:	b5 00       	TESTSET  \(P5\);
  1a:	b0 00       	TESTSET  \(P0\);

0000001c <nop>:
  1c:	00 00       	NOP;
  1e:	03 c0 00 18 	mnop;
	...

#objdump: -dr
#name: ia64 ldxmov-1

.*: +file format .*

Disassembly of section \.text:

0+000 <\.text>:
   0:	18 10 00 06 18 10 	\[MMB\]       ld8 r2=\[r3\]
			0: LDXMOV	foo
			1: LDXMOV	\.data
   6:	40 00 14 30 20 00 	            ld8 r4=\[r5\]
   c:	00 00 00 20       	            nop\.b 0x0
  10:	19 30 00 0e 18 10 	\[MMB\]       ld8 r6=\[r7\]
			10: LDXMOV	foo\+0x64
			11: LDXMOV	\.data\+0x64
  16:	80 00 24 30 20 00 	            ld8 r8=\[r9\]
  1c:	00 00 00 20       	            nop.b 0x0;;

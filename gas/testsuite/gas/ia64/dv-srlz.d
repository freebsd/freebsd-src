# as: -xauto -mtune=itanium1
# objdump: -d
# name ia64 dv-srlz

.*: +file format .*

Disassembly of section \.text:

0+000 <start>:
   0:	0a 00 00 02 34 04 	\[MMI\]       ptc\.e r1;;
   6:	00 00 00 60 00 00 	            srlz\.d
   c:	00 00 04 00       	            nop\.i 0x0
  10:	1d 08 00 04 18 10 	\[MFB\]       ld8 r1=\[r2\]
  16:	00 00 00 02 00 00 	            nop\.f 0x0
  1c:	00 00 20 00       	            rfi;;
  20:	0b 00 00 02 34 04 	\[MMI\]       ptc\.e r1;;
  26:	00 00 00 62 00 00 	            srlz\.i
  2c:	00 00 04 00       	            nop\.i 0x0;;
  30:	17 00 00 00 10 00 	\[BBB\]       epc
  36:	00 00 00 00 10 00 	            nop\.b 0x0
  3c:	00 00 00 20       	            nop\.b 0x0;;
  40:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  46:	00 00 00 02 00 00 	            nop\.f 0x0
  4c:	00 00 20 00       	            rfi;;

# as: -xnone -mtune=itanium1
# objdump: -d --disassemble-zeroes
# name: ia64 operand-or

.*: +file format .*

Disassembly of section \.text:

0+000 <_start>:
   0:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
   6:	30 20 80 09 28 00 	            fclass\.m p3,p4=f4,0x180
   c:	00 00 00 20       	            nop\.b 0x0
  10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  16:	30 20 c0 09 28 00 	            fclass\.m p3,p4=f4,0x1c0
  1c:	00 00 00 20       	            nop\.b 0x0
  20:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  26:	30 20 c0 89 28 00 	            fclass\.m p3,p4=f4,0x1c1
  2c:	00 00 00 20       	            nop\.b 0x0
  30:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  36:	30 20 c0 89 29 00 	            fclass\.m p3,p4=f4,0x1c3
  3c:	00 00 00 20       	            nop\.b 0x0
  40:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  46:	30 20 c8 89 29 00 	            fclass\.m p3,p4=f4,0x1cb
  4c:	00 00 00 20       	            nop\.b 0x0
  50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  56:	30 20 d8 89 29 00 	            fclass\.m p3,p4=f4,0x1db
  5c:	00 00 00 20       	            nop\.b 0x0
  60:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  66:	30 20 f8 89 29 00 	            fclass\.m p3,p4=f4,0x1fb
  6c:	00 00 00 20       	            nop\.b 0x0;;

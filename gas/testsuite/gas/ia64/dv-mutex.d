# as: -xexplicit -mtune=itanium1
# objdump: -d
# name ia64 dv-mutex

.*: +file format .*

Disassembly of section \.text:

0+000 <start>:
   0:	20 20 08 00 00 a4 	\[MII\] \(p01\) mov r4=2
   6:	40 28 00 00 c8 81 	      \(p02\) mov r4=5
   c:	70 00 00 90       	      \(p03\) mov r4=7
  10:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  16:	00 00 00 02 00 00 	            nop\.f 0x0
  1c:	00 00 20 00       	            rfi;;
  20:	0a 08 04 04 02 78 	\[MMI\]       cmp\.eq p1,p2=r1,r2;;
  26:	40 10 00 00 48 81 	      \(p01\) mov r4=2
  2c:	40 00 00 90       	      \(p02\) mov r4=4
  30:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  36:	00 00 00 02 00 00 	            nop\.f 0x0
  3c:	00 00 20 00       	            rfi;;
  40:	6a 08 06 04 02 78 	\[MMI\] \(p03\) cmp\.eq\.unc p1,p2=r1,r2;;
  46:	40 10 00 00 48 81 	      \(p01\) mov r4=2
  4c:	40 00 00 90       	      \(p02\) mov r4=4
  50:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  56:	00 00 00 02 00 00 	            nop\.f 0x0
  5c:	00 00 20 00       	            rfi;;
  60:	0a 08 04 04 02 78 	\[MMI\]       cmp\.eq p1,p2=r1,r2;;
  66:	40 10 00 00 48 81 	      \(p01\) mov r4=2
  6c:	40 00 00 90       	      \(p02\) mov r4=4
  70:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  76:	00 00 00 02 00 00 	            nop\.f 0x0
  7c:	00 00 20 00       	            rfi;;
  80:	6a 08 04 04 02 78 	\[MMI\] \(p03\) cmp\.eq p1,p2=r1,r2;;
  86:	40 10 00 00 48 81 	      \(p01\) mov r4=2
  8c:	40 00 00 90       	      \(p02\) mov r4=4
  90:	1d 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  96:	00 00 00 02 00 00 	            nop\.f 0x0
  9c:	00 00 20 00       	            rfi;;

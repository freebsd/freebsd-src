# as: -xexplicit -mtune=itanium1
# objdump: -d
# name ia64 dv-mutex

.*: +file format .*

Disassembly of section \.text:

0+000 <L-0xc0>:
   0:	3c 20 08 00 00 24 	\[MFB\] \(p01\) mov r4=2
   6:	00 00 00 02 00 01 	            nop\.f 0x0
   c:	c0 00 00 40       	      \(p02\) br\.cond\.sptk\.few c0 <L>
  10:	1d 20 1c 00 00 24 	\[MFB\]       mov r4=7
  16:	00 00 00 02 00 00 	            nop\.f 0x0
  1c:	00 00 20 00       	            rfi;;
  20:	1c 20 08 00 00 24 	\[MFB\]       mov r4=2
  26:	00 00 00 02 00 01 	            nop\.f 0x0
  2c:	a0 00 00 40       	      \(p02\) br\.cond\.sptk\.few c0 <L>
  30:	3d 20 1c 00 00 24 	\[MFB\] \(p01\) mov r4=7
  36:	00 00 00 02 00 00 	            nop\.f 0x0
  3c:	00 00 20 00       	            rfi;;
  40:	6a 08 06 04 02 78 	\[MMI\] \(p03\) cmp\.eq\.unc p1,p2=r1,r2;;
  46:	40 10 00 00 48 00 	      \(p01\) mov r4=2
  4c:	00 00 04 00       	            nop\.i 0x0
  50:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  56:	00 00 00 02 80 01 	            nop\.f 0x0
  5c:	70 00 00 40       	      \(p03\) br\.cond\.sptk\.few c0 <L>
  60:	1d 20 1c 00 00 24 	\[MFB\]       mov r4=7
  66:	00 00 00 02 00 00 	            nop\.f 0x0
  6c:	00 00 20 00       	            rfi;;
  70:	62 08 06 04 02 38 	\[MII\] \(p03\) cmp\.eq\.unc p1,p2=r1,r2
  76:	30 28 18 88 e8 80 	            cmp\.eq\.or p3,p4=r5,r6;;
  7c:	20 00 00 90       	      \(p01\) mov r4=2
  80:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  86:	00 00 00 02 80 01 	            nop\.f 0x0
  8c:	40 00 00 40       	      \(p03\) br\.cond\.sptk\.few c0 <L>
  90:	1d 20 1c 00 00 24 	\[MFB\]       mov r4=7
  96:	00 00 00 02 00 00 	            nop\.f 0x0
  9c:	00 00 20 00       	            rfi;;
  a0:	10 08 16 0c 42 70 	\[MIB\]       cmp\.ne\.and p1,p2=r5,r6
  a6:	40 10 00 00 c8 01 	      \(p01\) mov r4=2
  ac:	20 00 00 40       	      \(p03\) br\.cond\.sptk\.few c0 <L>
  b0:	1d 20 1c 00 00 24 	\[MFB\]       mov r4=7
  b6:	00 00 00 02 00 00 	            nop\.f 0x0
  bc:	00 00 20 00       	            rfi;;

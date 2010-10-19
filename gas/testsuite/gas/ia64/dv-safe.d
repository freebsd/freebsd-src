# as: -xexplicit -mtune=itanium1
# objdump: -d
# name ia64 dv-safe

.*: +file format .*

Disassembly of section \.text:

0+000 <start>:
   0:	02 08 04 04 02 38 	\[MII\]       cmp\.eq p1,p2=r1,r2
   6:	30 18 10 08 70 00 	            cmp\.eq p3,p4=r3,r4;;
   c:	00 00 04 00       	            nop\.i 0x0
  10:	1c 00 00 00 01 00 	\[MFB\]       nop\.m 0x0
  16:	00 00 00 02 80 21 	            nop\.f 0x0
  1c:	30 00 00 50       	      \(p03\) br\.call\.sptk\.few b1=40 <L>
  20:	20 20 08 00 00 a4 	\[MII\] \(p01\) mov r4=2
  26:	40 28 00 00 c8 a1 	      \(p02\) mov r4=5
  2c:	00 30 00 84       	      \(p03\) mov r5=r6
  30:	9d 28 00 0e 00 21 	\[MFB\] \(p04\) mov r5=r7
  36:	00 00 00 02 00 00 	            nop\.f 0x0
  3c:	00 00 00 20       	            nop\.b 0x0;;

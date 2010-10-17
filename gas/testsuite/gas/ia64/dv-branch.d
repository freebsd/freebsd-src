# as: -xexplicit
# objdump: -d
# name ia64 dv-branch

.*: +file format .*

Disassembly of section \.text:

0+000 <\.text>:
   0:	d0 08 00 10 18 90 	\[MIB\] \(p06\) ld8 r1=\[r8\]
   6:	61 10 04 80 03 03 	      \(p06\) mov b6=r2
   c:	68 00 80 10       	      \(p06\) br\.call\.sptk\.many b0=b6
  10:	11 08 00 3c 00 21 	\[MIB\]       mov r1=r30
  16:	00 00 00 02 00 03 	            nop\.i 0x0
  1c:	f0 ff ff 48       	      \(p06\) br\.cond\.sptk\.few 0x0;;

#name: s390x operands
#objdump: -dr

.*: +file format .*

Disassembly of section .text:

.* <foo>:
   0:	ec 12 00 00 00 45 [ 	]*brxlg	%r1,%r2,0 <foo>
[ 	]*2: R_390_PC16DBL	test_rie\+0x2
   6:	c0 e5 00 00 00 00 [ 	]*brasl	%r14,6 <foo\+0x6>
[ 	]*8: R_390_PC32DBL	test_ril\+0x2
   c:	eb 12 40 03 00 0d [ 	]*sllg	%r1,%r2,3\(%r4\)
  12:	07 07 [ 	]*bcr	0,%r7

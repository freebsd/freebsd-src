#name: s390 operands
#objdump: -dr

.*: +file format .*

Disassembly of section .text:

.* <foo>:
   0:	01 01 [ 	]*pr
   2:	a7 1a 80 01 [ 	]*ahi	%r1,-32767
   6:	18 12 [ 	]*lr	%r1,%r2
   8:	b2 5e 00 12 [ 	]*srst	%r1,%r2
   c:	b3 5b 93 12 [ 	]*didbr	%f1,%f9,%f2,3
  10:	ba 12 40 03 [ 	]*cs	%r1,%r2,3\(%r4\)
  14:	84 12 00 00 [ 	]*brxh	%r1,%r2,14 <foo\+0x14>
[ 	]*16: R_390_PC16DBL	test_rsi\+0x2
  18:	58 13 40 02 [ 	]*l	%r1,2\(%r3,%r4\)
  1c:	ed 10 30 02 00 1a [ 	]*adb	%f1,2\(%r3\)
  22:	ed 24 50 03 10 1e [ 	]*madb	%f1,%f2,3\(%r4,%r5\)
  28:	b2 33 20 01 [ 	]*ssch	1\(%r2\)
  2c:	92 03 20 01 [ 	]*mvi	1\(%r2\),3
  30:	d2 26 30 01 50 04 [ 	]*mvc	1\(39,%r3\),4\(%r5\)
  36:	e5 01 20 01 40 03 [ 	]*tprot	1\(%r2\),3\(%r4\)

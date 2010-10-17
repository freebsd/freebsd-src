#as:
#objdump: -d
#name: i860 dir-align01

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 20 a6 90 	adds	%r4,%r5,%r6
   4:	00 00 00 a0 	shl	%r0,%r0,%r0
   8:	00 00 00 a0 	shl	%r0,%r0,%r0
   c:	00 00 00 a0 	shl	%r0,%r0,%r0
  10:	00 50 6c 91 	adds	%r10,%r11,%r12
  14:	a1 b1 1a 4b 	fmlow.dd	%f22,%f24,%f26
  18:	30 74 f0 49 	pfadd.ss	%f14,%f15,%f16
  1c:	b0 8c 54 4a 	pfadd.sd	%f17,%f18,%f20

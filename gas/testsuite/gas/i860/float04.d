#as:
#objdump: -dr
#name: i860 float04

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	40 08 03 48 	fxfr	%f1,%fp
   4:	40 40 1e 48 	fxfr	%f8,%r30
   8:	40 f8 12 48 	fxfr	%f31,%r18
   c:	00 48 1f 08 	ixfr	%r9,%f31
  10:	00 b8 10 08 	ixfr	%r23,%f16
  14:	00 00 00 08 	ixfr	%r0,%f0
  18:	49 00 22 48 	fiadd.ss	%f0,%f1,%f2
  1c:	c9 31 0a 49 	fiadd.dd	%f6,%f8,%f10
  20:	4d 28 c7 48 	fisub.ss	%f5,%f6,%f7
  24:	cd 61 d0 49 	fisub.dd	%f12,%f14,%f16
  28:	49 74 f0 49 	pfiadd.ss	%f14,%f15,%f16
  2c:	c9 b5 1a 4b 	pfiadd.dd	%f22,%f24,%f26
  30:	4d a4 b6 4a 	pfisub.ss	%f20,%f21,%f22
  34:	cd e5 c2 4b 	pfisub.dd	%f28,%f30,%f2
  38:	49 02 22 48 	d.fiadd.ss	%f0,%f1,%f2
  3c:	00 00 00 a0 	shl	%r0,%r0,%r0
  40:	c9 33 0a 49 	d.fiadd.dd	%f6,%f8,%f10
  44:	00 00 00 a0 	shl	%r0,%r0,%r0
  48:	4d 2a c7 48 	d.fisub.ss	%f5,%f6,%f7
  4c:	00 00 00 a0 	shl	%r0,%r0,%r0
  50:	cd 63 d0 49 	d.fisub.dd	%f12,%f14,%f16
  54:	00 00 00 a0 	shl	%r0,%r0,%r0
  58:	49 76 f0 49 	d.pfiadd.ss	%f14,%f15,%f16
  5c:	00 00 00 a0 	shl	%r0,%r0,%r0
  60:	c9 b7 1a 4b 	d.pfiadd.dd	%f22,%f24,%f26
  64:	00 00 00 a0 	shl	%r0,%r0,%r0
  68:	4d a6 b6 4a 	d.pfisub.ss	%f20,%f21,%f22
  6c:	00 00 00 a0 	shl	%r0,%r0,%r0
  70:	cd e7 c2 4b 	d.pfisub.dd	%f28,%f30,%f2
  74:	00 00 00 a0 	shl	%r0,%r0,%r0

#as:
#objdump: -dr
#name: i860 float01

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	30 00 22 48 	fadd.ss	%f0,%f1,%f2
   4:	b0 10 64 48 	fadd.sd	%f2,%f3,%f4
   8:	b0 31 0a 49 	fadd.dd	%f6,%f8,%f10
   c:	31 28 c7 48 	fsub.ss	%f5,%f6,%f7
  10:	b1 40 2a 49 	fsub.sd	%f8,%f9,%f10
  14:	b1 61 d0 49 	fsub.dd	%f12,%f14,%f16
  18:	20 58 8d 49 	fmul.ss	%f11,%f12,%f13
  1c:	a0 70 f0 49 	fmul.sd	%f14,%f15,%f16
  20:	a0 91 96 4a 	fmul.dd	%f18,%f20,%f22
  24:	a1 b1 1a 4b 	fmlow.dd	%f22,%f24,%f26
  28:	30 74 f0 49 	pfadd.ss	%f14,%f15,%f16
  2c:	b0 8c 54 4a 	pfadd.sd	%f17,%f18,%f20
  30:	b0 b5 1a 4b 	pfadd.dd	%f22,%f24,%f26
  34:	31 a4 b6 4a 	pfsub.ss	%f20,%f21,%f22
  38:	b1 bc 1a 4b 	pfsub.sd	%f23,%f24,%f26
  3c:	b1 e5 c2 4b 	pfsub.dd	%f28,%f30,%f2
  40:	20 dc 9d 4b 	pfmul.ss	%f27,%f28,%f29
  44:	a0 f4 e4 4b 	pfmul.sd	%f30,%f31,%f4
  48:	a0 35 08 48 	pfmul.dd	%f6,%f0,%f8
  4c:	a4 15 9e 48 	pfmul3.dd	%f2,%f4,%f30
  50:	30 02 22 48 	d.fadd.ss	%f0,%f1,%f2
  54:	00 00 00 a0 	shl	%r0,%r0,%r0
  58:	b0 12 64 48 	d.fadd.sd	%f2,%f3,%f4
  5c:	00 00 00 a0 	shl	%r0,%r0,%r0
  60:	b0 33 0a 49 	d.fadd.dd	%f6,%f8,%f10
  64:	00 00 00 a0 	shl	%r0,%r0,%r0
  68:	31 2a c7 48 	d.fsub.ss	%f5,%f6,%f7
  6c:	00 00 00 a0 	shl	%r0,%r0,%r0
  70:	b1 42 2a 49 	d.fsub.sd	%f8,%f9,%f10
  74:	00 00 00 a0 	shl	%r0,%r0,%r0
  78:	b1 63 d0 49 	d.fsub.dd	%f12,%f14,%f16
  7c:	00 00 00 a0 	shl	%r0,%r0,%r0
  80:	20 5a 8d 49 	d.fmul.ss	%f11,%f12,%f13
  84:	00 00 00 a0 	shl	%r0,%r0,%r0
  88:	a0 72 f0 49 	d.fmul.sd	%f14,%f15,%f16
  8c:	00 00 00 a0 	shl	%r0,%r0,%r0
  90:	a0 93 96 4a 	d.fmul.dd	%f18,%f20,%f22
  94:	00 00 00 a0 	shl	%r0,%r0,%r0
  98:	a1 43 4c 49 	d.fmlow.dd	%f8,%f10,%f12
  9c:	00 00 00 a0 	shl	%r0,%r0,%r0
  a0:	30 76 f0 49 	d.pfadd.ss	%f14,%f15,%f16
  a4:	00 00 00 a0 	shl	%r0,%r0,%r0
  a8:	b0 8e 54 4a 	d.pfadd.sd	%f17,%f18,%f20
  ac:	00 00 00 a0 	shl	%r0,%r0,%r0
  b0:	b0 b7 1a 4b 	d.pfadd.dd	%f22,%f24,%f26
  b4:	00 00 00 a0 	shl	%r0,%r0,%r0
  b8:	31 a6 b6 4a 	d.pfsub.ss	%f20,%f21,%f22
  bc:	00 00 00 a0 	shl	%r0,%r0,%r0
  c0:	b1 be 1a 4b 	d.pfsub.sd	%f23,%f24,%f26
  c4:	00 00 00 a0 	shl	%r0,%r0,%r0
  c8:	b1 e7 c2 4b 	d.pfsub.dd	%f28,%f30,%f2
  cc:	00 00 00 a0 	shl	%r0,%r0,%r0
  d0:	20 de 9d 4b 	d.pfmul.ss	%f27,%f28,%f29
  d4:	00 00 00 a0 	shl	%r0,%r0,%r0
  d8:	a0 f6 e4 4b 	d.pfmul.sd	%f30,%f31,%f4
  dc:	00 00 00 a0 	shl	%r0,%r0,%r0
  e0:	a0 37 08 48 	d.pfmul.dd	%f6,%f0,%f8
  e4:	00 00 00 a0 	shl	%r0,%r0,%r0
  e8:	a4 17 9e 48 	d.pfmul3.dd	%f2,%f4,%f30
  ec:	00 00 00 a0 	shl	%r0,%r0,%r0

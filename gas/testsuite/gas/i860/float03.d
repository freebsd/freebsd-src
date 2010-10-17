#as:
#objdump: -dr
#name: i860 float03

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	b2 10 04 48 	fix.sd	%f2,%f4
   4:	b2 31 08 48 	fix.dd	%f6,%f8
   8:	ba 40 0a 48 	ftrunc.sd	%f8,%f10
   c:	ba 61 0e 48 	ftrunc.dd	%f12,%f14
  10:	b2 f4 0e 48 	pfix.sd	%f30,%f14
  14:	b2 c5 02 48 	pfix.dd	%f24,%f2
  18:	ba 44 0a 48 	pftrunc.sd	%f8,%f10
  1c:	ba 65 0e 48 	pftrunc.dd	%f12,%f14
  20:	34 04 22 48 	pfgt.ss	%f0,%f1,%f2
  24:	34 35 0a 49 	pfgt.dd	%f6,%f8,%f10
  28:	b4 2c c7 48 	pfle.ss	%f5,%f6,%f7
  2c:	b4 65 d0 49 	pfle.dd	%f12,%f14,%f16
  30:	35 5c 8d 49 	pfeq.ss	%f11,%f12,%f13
  34:	35 95 96 4a 	pfeq.dd	%f18,%f20,%f22
  38:	b2 12 1e 48 	d.fix.sd	%f2,%f30
  3c:	00 00 00 a0 	shl	%r0,%r0,%r0
  40:	b2 33 08 48 	d.fix.dd	%f6,%f8
  44:	00 00 00 a0 	shl	%r0,%r0,%r0
  48:	ba 42 18 48 	d.ftrunc.sd	%f8,%f24
  4c:	00 00 00 a0 	shl	%r0,%r0,%r0
  50:	ba 63 0e 48 	d.ftrunc.dd	%f12,%f14
  54:	00 00 00 a0 	shl	%r0,%r0,%r0
  58:	b2 16 1e 48 	d.pfix.sd	%f2,%f30
  5c:	00 00 00 a0 	shl	%r0,%r0,%r0
  60:	b2 37 08 48 	d.pfix.dd	%f6,%f8
  64:	00 00 00 a0 	shl	%r0,%r0,%r0
  68:	ba 46 18 48 	d.pftrunc.sd	%f8,%f24
  6c:	00 00 00 a0 	shl	%r0,%r0,%r0
  70:	ba 67 0e 48 	d.pftrunc.dd	%f12,%f14
  74:	00 00 00 a0 	shl	%r0,%r0,%r0
  78:	34 06 22 48 	d.pfgt.ss	%f0,%f1,%f2
  7c:	00 00 00 a0 	shl	%r0,%r0,%r0
  80:	34 37 0a 49 	d.pfgt.dd	%f6,%f8,%f10
  84:	00 00 00 a0 	shl	%r0,%r0,%r0
  88:	b4 2e c7 48 	d.pfle.ss	%f5,%f6,%f7
  8c:	00 00 00 a0 	shl	%r0,%r0,%r0
  90:	b4 67 d0 49 	d.pfle.dd	%f12,%f14,%f16
  94:	00 00 00 a0 	shl	%r0,%r0,%r0
  98:	35 5e 8d 49 	d.pfeq.ss	%f11,%f12,%f13
  9c:	00 00 00 a0 	shl	%r0,%r0,%r0
  a0:	35 97 96 4a 	d.pfeq.dd	%f18,%f20,%f22
  a4:	00 00 00 a0 	shl	%r0,%r0,%r0

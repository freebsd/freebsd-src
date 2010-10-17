#as:
#objdump: -dr
#name: i860 float02

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	22 00 01 48 	frcp.ss	%f0,%f1
   4:	a2 00 44 48 	frcp.sd	%f2,%f4
   8:	a2 01 c8 48 	frcp.dd	%f6,%f8
   c:	23 00 a6 48 	frsqr.ss	%f5,%f6
  10:	a3 00 0a 49 	frsqr.sd	%f8,%f10
  14:	a3 01 8e 49 	frsqr.dd	%f12,%f14
  18:	33 08 1f 48 	famov.ss	%f1,%f31
  1c:	33 11 1e 48 	famov.ds	%f2,%f30
  20:	b3 38 10 48 	famov.sd	%f7,%f16
  24:	b3 c1 0c 48 	famov.dd	%f24,%f12
  28:	22 02 01 48 	d.frcp.ss	%f0,%f1
  2c:	00 00 00 a0 	shl	%r0,%r0,%r0
  30:	a2 02 5e 48 	d.frcp.sd	%f2,%f30
  34:	00 00 00 a0 	shl	%r0,%r0,%r0
  38:	a2 03 c8 48 	d.frcp.dd	%f6,%f8
  3c:	00 00 00 a0 	shl	%r0,%r0,%r0
  40:	23 02 a6 48 	d.frsqr.ss	%f5,%f6
  44:	00 00 00 a0 	shl	%r0,%r0,%r0
  48:	a3 02 18 49 	d.frsqr.sd	%f8,%f24
  4c:	00 00 00 a0 	shl	%r0,%r0,%r0
  50:	a3 03 8e 49 	d.frsqr.dd	%f12,%f14
  54:	00 00 00 a0 	shl	%r0,%r0,%r0
  58:	33 2a 0d 48 	d.famov.ss	%f5,%f13
  5c:	00 00 00 a0 	shl	%r0,%r0,%r0
  60:	33 f3 15 48 	d.famov.ds	%f30,%f21
  64:	00 00 00 a0 	shl	%r0,%r0,%r0
  68:	b3 ba 16 48 	d.famov.sd	%f23,%f22
  6c:	00 00 00 a0 	shl	%r0,%r0,%r0
  70:	b3 03 0c 48 	d.famov.dd	%f0,%f12
  74:	00 00 00 a0 	shl	%r0,%r0,%r0

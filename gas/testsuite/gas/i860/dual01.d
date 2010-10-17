#as:
#objdump: -d
#name: i860 dual01

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 00 00 a0 	shl	%r0,%r0,%r0
   4:	00 00 00 a0 	shl	%r0,%r0,%r0
   8:	b0 47 4c 49 	d.pfadd.dd	%f8,%f10,%f12
   c:	00 28 c6 90 	adds	%r5,%r6,%r6
  10:	b0 47 4c 49 	d.pfadd.dd	%f8,%f10,%f12
  14:	10 00 58 25 	fld.d	16\(%r10\),%f24
  18:	00 02 00 b0 	d.shrd	%r0,%r0,%r0
  1c:	08 00 48 25 	fld.d	8\(%r10\),%f8
  20:	00 02 00 b0 	d.shrd	%r0,%r0,%r0
  24:	00 00 50 25 	fld.d	0\(%r10\),%f16
  28:	00 00 00 a0 	shl	%r0,%r0,%r0
  2c:	00 00 00 a0 	shl	%r0,%r0,%r0

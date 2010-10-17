#as: -mintel-syntax
#objdump: -d
#name: i860 dir-intel01

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	00 00 00 a0 	shl	%r0,%r0,%r0
   4:	00 00 00 a0 	shl	%r0,%r0,%r0
   8:	30 02 22 48 	d.fadd.ss	%f0,%f1,%f2
   c:	00 00 00 a0 	shl	%r0,%r0,%r0
  10:	b0 12 64 48 	d.fadd.sd	%f2,%f3,%f4
  14:	00 00 00 a0 	shl	%r0,%r0,%r0
  18:	b0 33 0a 49 	d.fadd.dd	%f6,%f8,%f10
  1c:	00 00 00 a0 	shl	%r0,%r0,%r0
  20:	00 00 00 a0 	shl	%r0,%r0,%r0
  24:	00 00 00 a0 	shl	%r0,%r0,%r0

#as:
#objdump: -d
#name: i860 pseudo-ops01

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	49 28 06 48 	fiadd\.ss	%f5,%f0,%f6
   4:	c9 41 0a 48 	fiadd\.dd	%f8,%f0,%f10
   8:	b3 18 14 48 	famov\.sd	%f3,%f20
   c:	33 c1 09 48 	famov\.ds	%f24,%f9
  10:	33 e5 03 48 	pfamov\.ds	%f28,%f3

#as:
#objdump: -d
#source: nop.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
	...
   c:	80008000 	nop
  10:	8254e010 	add		r18, r20, r24
	...
  28:	80008000 	nop
  2c:	8254e026 	xor		r18, r20, r24

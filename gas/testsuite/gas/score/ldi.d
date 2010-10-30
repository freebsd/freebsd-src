#as:
#objdump: -d
#source: ldi.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	5200      	ldiu!		r2, 0
   2:	5200      	ldiu!		r2, 0
   4:	53ff      	ldiu!		r3, 255
   6:	53ff      	ldiu!		r3, 255
   8:	5409      	ldiu!		r4, 9
   a:	5409      	ldiu!		r4, 9
   c:	53ff      	ldiu!		r3, 255
   e:	53ff      	ldiu!		r3, 255
  10:	85188006 	ldi		r8, 0x3\(3\)
  14:	87388006 	ldi		r25, 0x3\(3\)
	...
  20:	84588000 	ldi		r2, 0x0\(0\)
  24:	87388000 	ldi		r25, 0x0\(0\)
  28:	847881fe 	ldi		r3, 0xff\(255\)
  2c:	86f88002 	ldi		r23, 0x1\(1\)
  30:	5fff      	ldiu!		r15, 255
  32:	5fff      	ldiu!		r15, 255
  34:	5803      	ldiu!		r8, 3
  36:	5803      	ldiu!		r8, 3
#pass

#as:
#objdump: -d
#source: br.s

.*: +file format .*

Disassembly of section \.text:

00000000 <.text>:
   0:	0f04      	br!		r0
   2:	0f04      	br!		r0
   4:	0ff4      	br!		r15
   6:	0ff4      	br!		r15
   8:	0f34      	br!		r3
   a:	0f34      	br!		r3
   c:	0f54      	br!		r5
   e:	0f54      	br!		r5
  10:	8003bc08 	br		r3
  14:	801fbc08 	br		r31
	...
  20:	0f0c      	brl!		r0
  22:	0f0c      	brl!		r0
  24:	0ffc      	brl!		r15
  26:	0ffc      	brl!		r15
  28:	0f3c      	brl!		r3
  2a:	0f3c      	brl!		r3
  2c:	0f5c      	brl!		r5
  2e:	0f5c      	brl!		r5
  30:	8003bc09 	brl		r3
  34:	801fbc09 	brl		r31
	...
  40:	8000bc08 	br		r0
  44:	8017bc08 	br		r23
  48:	800fbc08 	br		r15
  4c:	801bbc08 	br		r27
  50:	0f64      	br!		r6
  52:	0f64      	br!		r6
  54:	0f34      	br!		r3
  56:	0f34      	br!		r3
	...
  60:	8000bc09 	brl		r0
  64:	8017bc09 	brl		r23
  68:	800fbc09 	brl		r15
  6c:	801bbc09 	brl		r27
  70:	0f6c      	brl!		r6
  72:	0f6c      	brl!		r6
  74:	0f3c      	brl!		r3
  76:	0f3c      	brl!		r3
#pass

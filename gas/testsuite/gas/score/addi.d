#as:
#objdump: -d
#source: addi.s

.*: +file format .*

Disassembly of section \.text:

00000000 <.text>:
   0:	84008003 	addi.c		r0, 1
   4:	84008003 	addi.c		r0, 1
   8:	85e08021 	addi.c		r15, 16
   c:	85e08021 	addi.c		r15, 16
  10:	85e18001 	addi.c		r15, 16384
  14:	85e18001 	addi.c		r15, 16384
  18:	6818      	addei!		r8, 3
  1a:	6818      	addei!		r8, 3
  1c:	6f78      	addei!		r15, 15
  1e:	0000      	nop!
  20:	85e1ffff 	addi.c		r15, 32767
	...
  30:	8403ffff 	addi.c		r0, -1
  34:	8403ffff 	addi.c		r0, -1
  38:	85e3ffe1 	addi.c		r15, -16
  3c:	85e3ffe1 	addi.c		r15, -16
  40:	85e38001 	addi.c		r15, -16384
  44:	85e38001 	addi.c		r15, -16384
  48:	6898      	subei!		r8, 3
  4a:	6898      	subei!		r8, 3
  4c:	6ff8      	subei!		r15, 15
  4e:	0000      	nop!
  50:	85e1ffff 	addi.c		r15, 32767
#pass

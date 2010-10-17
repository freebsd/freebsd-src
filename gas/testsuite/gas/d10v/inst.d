#objdump: -dr
#name: D10V basic instruction test output
#as:

.*: +file format elf32-d10v

Disassembly of section .text:

00000000 <start>:
   0:	a9 04 c2 29 	sac	r0, a0	<-	sachi	r1, a0
   4:	a9 06 c2 2d 	rac	r0, a0, -0x2	<-	rachi	r1, a0, -0x2
   8:	2f 00 32 26 	nop		||	slae	a0, r3
   c:	f2 11 08 00 	ld	r1, @0x800
  10:	f3 01 08 00 	ld2w	r0, @0x800
  14:	f7 01 08 00 	st2w	r0, @0x800
  18:	f6 11 08 00 	st	r1, @0x800
  1c:	6f 00 5e 00 	nop		->	nop	
  20:	6f 00 5e 00 	nop		->	nop	
  24:	2f 00 5e 00 	nop		||	nop	
  28:	af 00 5e 00 	nop		<-	nop	
  2c:	23 11 de 00 	not	r1	||	nop	
  30:	63 21 de 00 	not	r2	->	nop	

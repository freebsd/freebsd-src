#objdump: -D
#source: instruction_packing-007.s
#as: -gstabs --no-gstabs-packing

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	e0 00 00 00 	ldi.l	r0, 0x0
   4:	e0 10 10 00 	ldi.l	r1, 0x1000
   8:	20 22 de 00 	ldi.s	r2, 0x2	||	nop	
   c:	20 33 de 00 	ldi.s	r3, 0x3	||	nop	
  10:	e0 40 40 00 	ldi.l	r4, 0x4000
  14:	20 55 de 00 	ldi.s	r5, 0x5	||	nop	
  18:	26 0d 5e 00 	jmp	r13	||	nop	
#pass

#objdump: -D
#source: instruction_packing-007.s
#as: -gstabs --no-gstabs-packing --gstabs-packing

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	e0 00 00 00 	ldi.l	r0, 0x0
   4:	e0 10 10 00 	ldi.l	r1, 0x1000
   8:	60 22 c0 67 	ldi.s	r2, 0x2	->	ldi.s	r3, 0x3
   c:	e0 40 40 00 	ldi.l	r4, 0x4000
  10:	60 55 cc 1a 	ldi.s	r5, 0x5	->	jmp	r13
#pass

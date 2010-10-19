#objdump: -D
#source: instruction_packing-005.s
#as: -O

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	36 df de 00 	st	r13, @-sp	||	nop	
   4:	f0 0e 00 00 	ld	r0, @\(0x0, r14\)
   8:	e4 80 00 02 	bl.l	10 <func_a>
   c:	70 df cc 1a 	ld	r13, @sp\+	->	jmp	r13

00000010 <func_a>:
  10:	20 20 40 61 	mv	r2, r0	||	ldi.s	r3, 0x0
  14:	c6 12 80 00 	and3	r1, r2, -0x8000
  18:	01 31 a2 43 	addi	r3, 0x1	||	slli	r2, 0x1
  1c:	c2 10 80 00 	cmpeqi.l	r1, -0x8000
  20:	60 02 0a 1f 	mv	r0, r2	->	bnoti	r0, 0xf
  24:	22 20 de 00 	mvf0t	r2, r0	||	nop	
  28:	e3 30 00 08 	cmpui	r3, 0x8
  2c:	65 fd 40 04 	brf0t.s	14 <func_a\+0x4>	->	mv	r0, r2
  30:	26 0d 5e 00 	jmp	r13	||	nop	
Disassembly of section .data:

00000000 <in_data>:
   0:	Address 0x0+ is out of bounds.


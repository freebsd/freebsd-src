#objdump: -D
#source: instruction_packing-001.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	e0 10 30 00 	ldi.l	r1, 0x3000
   4:	26 81 40 03 	jl	r1	||	ldi.s	r0, 0x1

00000008 <L0>:
   8:	2f 80 40 03 	trap	0x0	||	ldi.s	r0, 0x1
   c:	24 ff c0 03 	bl.s	8 <L0>	||	ldi.s	r0, 0x1
  10:	26 01 40 03 	jmp	r1	||	ldi.s	r0, 0x1
  14:	2f f0 40 03 	stop		||	ldi.s	r0, 0x1
  18:	2f e0 40 03 	sleep		||	ldi.s	r0, 0x1
  1c:	2f c0 40 03 	wait		||	ldi.s	r0, 0x1

00000020 <L1>:
  20:	2f 90 40 03 	dbt		||	ldi.s	r0, 0x1
  24:	24 7f c0 03 	bra.s	20 <L1>	||	ldi.s	r0, 0x1
  28:	2f a0 40 03 	rte		||	ldi.s	r0, 0x1
  2c:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

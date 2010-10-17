#objdump: -Dr
#source: instruction_packing-006.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <test0>:
   0:	24 81 5e 00 	bl.s	8 <test1>	||	nop	
   4:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000008 <test1>:
   8:	26 81 5e 00 	jl	r1	||	nop	
   c:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000010 <test2>:
  10:	2f 81 5e 00 	trap	0x1	||	nop	
  14:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000018 <test3>:
  18:	2f e0 5e 00 	sleep		||	nop	
  1c:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000020 <test4>:
  20:	2f c0 5e 00 	wait		||	nop	
  24:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000028 <test5>:
  28:	2f f0 5e 00 	stop		||	nop	
  2c:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000030 <test6>:
  30:	2f 90 5e 00 	dbt		||	nop	
  34:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000038 <test7>:
  38:	24 7e 5e 00 	bra.s	28 <test5>	||	nop	
  3c:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000040 <test8>:
  40:	26 01 5e 00 	jmp	r1	||	nop	
  44:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000048 <test9>:
  48:	2f a0 5e 00 	rte		||	nop	
  4c:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

00000050 <test10>:
  50:	2f b0 5e 00 	rtd		||	nop	
  54:	20 01 de 00 	ldi.s	r0, 0x1	||	nop	

#objdump: -dr --disassemble-zeroes
#as: -march=mips2 -mabi=32
#name: noreorder test

.*: +file format .*mips.*

Disassembly of section .text:

00000000 <per_cpu_trap_init>:
   0:	00000000 	nop
   4:	00000000 	nop
   8:	0c000000 	jal	0 <per_cpu_trap_init>
			8: R_MIPS_26	cpu_cache_init
   c:	00000000 	nop
  10:	8fbf0010 	lw	ra,16\(sp\)
  14:	08000000 	j	0 <per_cpu_trap_init>
			14: R_MIPS_26	tlb_init
  18:	27bd0018 	addiu	sp,sp,24
  1c:	00000000 	nop
  20:	00000000 	nop
  24:	1000fff6 	b	0 <per_cpu_trap_init>
  28:	00000000 	nop
  2c:	00000000 	nop

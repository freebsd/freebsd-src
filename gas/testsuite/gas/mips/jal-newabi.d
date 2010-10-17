#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS ELF NewABI jal
#as: -n32 -KPIC -xgot

.*: +file format elf32-n.*mips.*

Disassembly of section \.text:
00000000 <label> 3c041234 	lui	a0,0x1234
00000004 <label\+0x4> 34845678 	ori	a0,a0,0x5678
00000008 <label\+0x8> 8f990000 	lw	t9,0\(gp\)
			8: R_MIPS_GOT_PAGE	.text
0000000c <label\+0xc> 27390000 	addiu	t9,t9,0
			c: R_MIPS_GOT_OFST	.text
00000010 <label\+0x10> 0320f809 	jalr	t9
			10: R_MIPS_JALR	.text
00000014 <label\+0x14> 00000000 	nop
	...

# name: ELF e_flags: -m4650
# source: elf_e_flags.s
# as: -m4650
# objdump: -fd

.*:.*file format.*mips.*
architecture: mips:4650, flags 0x00000011:
HAS_RELOC, HAS_SYMS
start address 0x0+00

Disassembly of section .text:

0+00 <foo>:
   0:	70851002 	mul	v0,a0,a1
   4:	03e00008 	jr	ra
   8:	24420001 	addiu	v0,v0,1

0+0c <main>:
   c:	27bdffd8 	addiu	sp,sp,-40
  10:	afbf0020 	sw	ra,32\(sp\)
  14:	0c000000 	jal	0 <foo>
  18:	00000000 	nop
  1c:	0000102[1d] 	move	v0,zero
  20:	8fbf0020 	lw	ra,32\(sp\)
  24:	00000000 	nop
  28:	03e00008 	jr	ra
  2c:	27bd0028 	addiu	sp,sp,40
	...

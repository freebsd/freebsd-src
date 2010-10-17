#name: MIPS 32-bit ELF embedded-pic relocs #2-rev-1 (0x8004 backward edge case)
#as: -membedded-pic -mips3
#source: empic2-rev-tgt.s
#source: empic2-space.s
#source: empic2-ref.s
#objdump: --prefix-addresses -tdr --show-raw-insn -mmips:4000
#ld: -Ttext 0x400000 -e 0x400000

.*:     file format elf.*mips.*

#...
0+410000 g     F .text	[0-9a-f]+ foo
#...
0+407ffc g     F .text	[0-9a-f]+ bar
#...

Disassembly of section \.text:
	...
0+407ffc <[^>]*> 00000000 	nop
	...
0+410000 <[^>]*> 3c02ffff 	lui	v0,0xffff
0+410004 <[^>]*> 64427ffc 	daddiu	v0,v0,32764
	...
#pass

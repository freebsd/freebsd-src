#name: MIPS 32-bit ELF embedded-pic relocs #2-fwd-1 (0xfffc forward edge case)
#as: -membedded-pic -mips3
#source: empic2-ref.s
#source: empic2-space.s
#source: empic2-fwd-tgt.s
#objdump: --prefix-addresses -tdr --show-raw-insn -mmips:4000
#ld: -Ttext 0x400000 -e 0x400000

.*:     file format elf.*mips.*

#...
0+400000 g     F .text	[0-9a-f]+ foo
#...
0+40fffc g     F .text	[0-9a-f]+ bar
#...

Disassembly of section \.text:
0+400000 <[^>]*> 3c020001 	lui	v0,0x1
0+400004 <[^>]*> 6442fffc 	daddiu	v0,v0,-4
	...
#pass

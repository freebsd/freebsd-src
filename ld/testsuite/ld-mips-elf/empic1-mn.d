#name: MIPS 32-bit ELF embedded-pic relocs #1-mn (medium negative)
#as: -membedded-pic -mips3
#source: empic1-tgt.s
#source: empic1-space.s
#source: empic1-ref.s
#objdump: --prefix-addresses -tdr --show-raw-insn -mmips:4000
#ld: -Ttext 0x400000 -e 0x400000

.*:     file format elf.*mips.*

SYMBOL TABLE:
#...
0+408020 g     F .text	[0-9a-f]+ foo
#...
0+400000 g     F .text	[0-9a-f]+ bar
#...

Disassembly of section \.text:
	...
	...
0+408020 <[^>]*> 00000000 	nop
0+408024 <[^>]*> 3c02ffff 	lui	v0,0xffff
0+408028 <[^>]*> 64427fe0 	daddiu	v0,v0,32736
	...
#pass

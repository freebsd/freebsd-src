#name: MIPS ELF lo16 merge
#source: reloc-merge-lo16.s
#ld: -Treloc-merge-lo16.ld
#objdump: -td --prefix-addresses --show-raw-insn

# Test lo16 reloc calculation with string merging.

.*: +file format .*mips.*
#...
0+80fe70 l       .rodata	0+000000 g
0+400000 g     F .text	0+00000c __start
#...
0+400000 <[^>]*> 3c020081 	lui	v0,0x81
0+400004 <[^>]*> 2443fe70 	addiu	v1,v0,-400
0+400008 <[^>]*> 2442fe70 	addiu	v0,v0,-400
	\.\.\.

#name: MIPS multi-got-no-shared
#as: -EB -32 -KPIC -mno-shared
#source: multi-got-no-shared-1.s
#source: multi-got-no-shared-2.s
#ld: -melf32btsmip --entry func1
#objdump: -D -j .text --prefix-addresses --show-raw-insn

.*: +file format.*

Disassembly of section \.text:
004000b0 <[^>]*> 3c1c0046 	lui	gp,0x46
004000b4 <[^>]*> 279c9ff0 	addiu	gp,gp,-24592
004000b8 <[^>]*> afbc0008 	sw	gp,8\(sp\)
#...
00408d60 <[^>]*> 3c1c0047 	lui	gp,0x47
00408d64 <[^>]*> 279cb960 	addiu	gp,gp,-18080
00408d68 <[^>]*> afbc0008 	sw	gp,8\(sp\)
#pass

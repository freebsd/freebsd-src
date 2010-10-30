#name: R_MIPS16_GPREL reloc
#source: ../../../gas/testsuite/gas/mips/elf-rel6.s
#objdump: --prefix-addresses -tdr --show-raw-insn
#ld: -Ttext 0x20000000 -e 0x20000000 -N

.*:     file format elf.*mips.*

#...

Disassembly of section \.text:
0*20000000 <[^>]*> f010 8352 	lb	v0,-32750\(v1\)
0*20000004 <[^>]*> f010 8353 	lb	v0,-32749\(v1\)
0*20000008 <[^>]*> f252 8346 	lb	v0,-28090\(v1\)
0*2000000c <[^>]*> 6500      	nop
0*2000000e <[^>]*> 6500      	nop
#pass

#objdump: --prefix-addresses -dr
#name: MIPS constant hi/lo

.*: +file format elf.*mips.*

Disassembly of section \.text:
0+00 <.*> lui	a0,0xdeae
0+04 <.*> jr	ra
0+08 <.*> lb	v0,-16657\(a0\)
#pass

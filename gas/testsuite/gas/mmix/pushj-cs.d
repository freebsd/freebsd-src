#as: -x
#source: pushj-c.s
#objdump: -tdr

.*:     file format elf64-mmix
SYMBOL TABLE:
0+ l    d  .text	0+ (|\.text)
0+ l    d  .data	0+ (|\.data)
0+ l    d  .bss	0+ (|\.bss)
ffff0000ffff0000 l       \*ABS\*	0+ i1
ffff0000ffff0000 l       \*ABS\*	0+ i2
0+ g     F .text	0+ Main
Disassembly of section \.text:
0+ <Main>:
   0:	f2010000 	pushj \$1,0 <Main>
			0: R_MMIX_PUSHJ_STUBBABLE	\*ABS\*\+0xffff0000ffff0000
   4:	f2020000 	pushj \$2,4 <Main\+0x4>
			4: R_MMIX_PUSHJ_STUBBABLE	i2

#source: err-bpo5.s
#as: -linker-allocated-gregs
#objdump: -drt

# The -linker-allocated-gregs option validates omissions of GREG.
# Note the inconsequence in relocs regarding forward and backward
# references (not specific to this functionality); they may change.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ 
0+ l    d  \.data	0+ 
0+ l    d  \.bss	0+ 
0+2a l       \*ABS\*	0+ a
0+70 l       \*ABS\*	0+ b
0+48 l       \*ABS\*	0+ c
0+3 l       \*ABS\*	0+ d

Disassembly of section \.text:

0+ <\.text>:
   0:	8d2b0000 	ldo \$43,\$0,0
			2: R_MMIX_BASE_PLUS_OFFSET	\*ABS\*\+0x5e
   4:	232f0000 	addu \$47,\$0,0
			6: R_MMIX_BASE_PLUS_OFFSET	\*ABS\*\+0x9a
   8:	23300000 	addu \$48,\$0,0
			a: R_MMIX_BASE_PLUS_OFFSET	\*ABS\*\+0x86
   c:	8d2b0000 	ldo \$43,\$0,0
			e: R_MMIX_BASE_PLUS_OFFSET	c\+0x2
  10:	232f0000 	addu \$47,\$0,0
			12: R_MMIX_BASE_PLUS_OFFSET	d\+0xd4
  14:	23300000 	addu \$48,\$0,0
			16: R_MMIX_BASE_PLUS_OFFSET	c\+0x15

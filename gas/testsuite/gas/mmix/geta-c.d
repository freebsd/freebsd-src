#as: -x
#objdump: -tdr

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  .text	0+ 
0+ l    d  .data	0+ 
0+ l    d  .bss	0+ 
ffff0000ffff0000 l       \*ABS\*	0+ i1
ffff0000ffff0000 l       \*ABS\*	0+ i2
0+ g     F .text	0+ Main

Disassembly of section .text:

0+ <Main>:
   0:	f4ff0000 	geta \$255,0 <Main>
			0: R_MMIX_GETA	\*ABS\*\+0xffff0000ffff0000
   4:	fd000000 	swym 0,0,0
   8:	fd000000 	swym 0,0,0
   c:	fd000000 	swym 0,0,0
  10:	f4ff0000 	geta \$255,10 <Main\+0x10>
			10: R_MMIX_GETA	i2
  14:	fd000000 	swym 0,0,0
  18:	fd000000 	swym 0,0,0
  1c:	fd000000 	swym 0,0,0

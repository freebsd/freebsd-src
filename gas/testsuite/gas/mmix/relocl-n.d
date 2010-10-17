#objdump: -dr
#source: reloclab.s
#as: -no-expand
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	f0000000 	jmp 0 <Main>
			0: R_MMIX_ADDR27	foo\+0x8
   4:	f0000004 	jmp 14 <here>
   8:	f4080003 	geta \$8,14 <here>
   c:	46630002 	bod \$99,14 <here>
  10:	fd000000 	swym 0,0,0

0000000000000014 <here>:
  14:	42de0000 	bz \$222,14 <here>
			14: R_MMIX_ADDR19	bar\+0x10

0000000000000018 <there>:
  18:	f4040000 	geta \$4,18 <there>
			18: R_MMIX_ADDR19	baz
  1c:	f2070000 	pushj \$7,1c <there\+0x4>
			1c: R_MMIX_ADDR19	foobar
  20:	f1fffffe 	jmp 18 <there>
  24:	f558fffd 	geta \$88,18 <there>
  28:	476ffffc 	bod \$111,18 <there>

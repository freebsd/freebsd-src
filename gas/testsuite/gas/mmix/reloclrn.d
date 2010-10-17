# objdump: -dr
# as: -linkrelax -no-expand
# source: reloclab.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	f0000000 	jmp 0 <Main>
			0: R_MMIX_ADDR27	foo\+0x8
   4:	f0000000 	jmp 4 <Main\+0x4>
			4: R_MMIX_ADDR27	\.text\+0x14
   8:	f4080000 	geta \$8,8 <Main\+0x8>
			8: R_MMIX_ADDR19	\.text\+0x14
   c:	46630000 	bod \$99,c <Main\+0xc>
			c: R_MMIX_ADDR19	\.text\+0x14
  10:	fd000000 	swym 0,0,0

0000000000000014 <here>:
  14:	42de0000 	bz \$222,14 <here>
			14: R_MMIX_ADDR19	bar\+0x10

0000000000000018 <there>:
  18:	f4040000 	geta \$4,18 <there>
			18: R_MMIX_ADDR19	baz
  1c:	f2070000 	pushj \$7,1c <there\+0x4>
			1c: R_MMIX_ADDR19	foobar
  20:	f0000000 	jmp 20 <there\+0x8>
			20: R_MMIX_ADDR27	\.text\+0x18
  24:	f4580000 	geta \$88,24 <there\+0xc>
			24: R_MMIX_ADDR19	\.text\+0x18
  28:	466f0000 	bod \$111,28 <there\+0x10>
			28: R_MMIX_ADDR19	\.text\+0x18

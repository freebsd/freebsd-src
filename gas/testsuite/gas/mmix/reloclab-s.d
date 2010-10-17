#objdump: -dr
#source: reloclab.s
#as: -x

.*:     file format elf64-mmix
Disassembly of section \.text:
0000000000000000 <Main>:
   0:	f0000000 	jmp 0 <Main>
			0: R_MMIX_JMP	foo\+0x8
   4:	fd000000 	swym 0,0,0
   8:	fd000000 	swym 0,0,0
   c:	fd000000 	swym 0,0,0
  10:	fd000000 	swym 0,0,0
  14:	f0000004 	jmp 24 <here>
  18:	f4080003 	geta \$8,24 <here>
  1c:	46630002 	bod \$99,24 <here>
  20:	fd000000 	swym 0,0,0
0000000000000024 <here>:
  24:	42de0000 	bz \$222,24 <here>
			24: R_MMIX_CBRANCH	bar\+0x10
  28:	fd000000 	swym 0,0,0
  2c:	fd000000 	swym 0,0,0
  30:	fd000000 	swym 0,0,0
  34:	fd000000 	swym 0,0,0
  38:	fd000000 	swym 0,0,0
000000000000003c <there>:
  3c:	f4040000 	geta \$4,3c <there>
			3c: R_MMIX_GETA	baz
  40:	fd000000 	swym 0,0,0
  44:	fd000000 	swym 0,0,0
  48:	fd000000 	swym 0,0,0
  4c:	f2070000 	pushj \$7,4c <there\+0x10>
			4c: R_MMIX_PUSHJ_STUBBABLE	foobar
  50:	f1fffffb 	jmp 3c <there>
  54:	f558fffa 	geta \$88,3c <there>
  58:	476ffff9 	bod \$111,3c <there>

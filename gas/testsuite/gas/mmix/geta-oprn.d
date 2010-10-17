# objdump: -dr
# as: -linkrelax -no-expand
# source: geta-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0

0000000000000004 <here>:
   4:	fd000000 	swym 0,0,0
   8:	f4190000 	geta \$25,8 <here\+0x4>
			8: R_MMIX_ADDR19	\.text\+0x4

000000000000000c <at>:
   c:	f4200000 	geta \$32,c <at>
			c: R_MMIX_ADDR19	\.text\+0xc
  10:	424e0000 	bz \$78,10 <at\+0x4>
			10: R_MMIX_ADDR19	\.text\+0x30
  14:	f25b0000 	pushj \$91,14 <at\+0x8>
			14: R_MMIX_ADDR19	\.text\+0x4
  18:	f2870000 	pushj \$135,18 <at\+0xc>
			18: R_MMIX_ADDR19	\.text\+0x4
  1c:	f4870000 	geta \$135,1c <at\+0x10>
			1c: R_MMIX_ADDR19	\.text\+0x30
  20:	f2870000 	pushj \$135,20 <at\+0x14>
			20: R_MMIX_ADDR19	\.text\+0x30
  24:	f2490000 	pushj \$73,24 <at\+0x18>
			24: R_MMIX_ADDR19	\.text\+0x30
  28:	f2380000 	pushj \$56,28 <at\+0x1c>
			28: R_MMIX_ADDR19	\.text\+0x30
  2c:	5e870000 	pbev \$135,2c <at\+0x20>
			2c: R_MMIX_ADDR19	\.text\+0x4

0000000000000030 <there>:
  30:	fd000000 	swym 0,0,0

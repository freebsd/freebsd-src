# objdump: -dr
# as: -linkrelax
# source: geta-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0

0000000000000004 <here>:
   4:	fd000000 	swym 0,0,0
   8:	f519ffff 	geta \$25,4 <here>
			8: R_MMIX_ADDR19	\.text\+0x4

000000000000000c <at>:
   c:	f4200000 	geta \$32,c <at>
			c: R_MMIX_ADDR19	\.text\+0xc
  10:	424e0008 	bz \$78,30 <there>
			10: R_MMIX_ADDR19	\.text\+0x30
  14:	f35bfffc 	pushj \$91,4 <here>
			14: R_MMIX_ADDR19	\.text\+0x4
  18:	f387fffb 	pushj \$135,4 <here>
			18: R_MMIX_ADDR19	\.text\+0x4
  1c:	f4870005 	geta \$135,30 <there>
			1c: R_MMIX_ADDR19	\.text\+0x30
  20:	f2870004 	pushj \$135,30 <there>
			20: R_MMIX_ADDR19	\.text\+0x30
  24:	f2490003 	pushj \$73,30 <there>
			24: R_MMIX_ADDR19	\.text\+0x30
  28:	f2380002 	pushj \$56,30 <there>
			28: R_MMIX_ADDR19	\.text\+0x30
  2c:	5f87fff6 	pbev \$135,4 <here>
			2c: R_MMIX_ADDR19	\.text\+0x4

0000000000000030 <there>:
  30:	fd000000 	swym 0,0,0

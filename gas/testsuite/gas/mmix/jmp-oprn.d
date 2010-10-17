# objdump: -dr
# as: -linkrelax -no-expand
# source: jmp-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0

0000000000000004 <here>:
   4:	fd000000 	swym 0,0,0
   8:	f0000000 	jmp 8 <here\+0x4>
			8: R_MMIX_ADDR27	\.text\+0x4

000000000000000c <at>:
   c:	f0000000 	jmp c <at>
			c: R_MMIX_ADDR27	\.text\+0xc
  10:	f0000000 	jmp 10 <at\+0x4>
			10: R_MMIX_ADDR27	\.text\+0x20
  14:	f0000000 	jmp 14 <at\+0x8>
			14: R_MMIX_ADDR27	\.text\+0x4
  18:	f0000000 	jmp 18 <at\+0xc>
			18: R_MMIX_ADDR27	\.text\+0x20
  1c:	f0000000 	jmp 1c <at\+0x10>
			1c: R_MMIX_ADDR27	\.text\+0x4

0000000000000020 <there>:
  20:	fd000000 	swym 0,0,0

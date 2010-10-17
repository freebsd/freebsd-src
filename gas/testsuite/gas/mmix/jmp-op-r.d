# objdump: -dr
# as: -linkrelax
# source: jmp-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0

0000000000000004 <here>:
   4:	fd000000 	swym 0,0,0
   8:	f1ffffff 	jmp 4 <here>
			8: R_MMIX_ADDR27	\.text\+0x4

000000000000000c <at>:
   c:	f0000000 	jmp c <at>
			c: R_MMIX_ADDR27	\.text\+0xc
  10:	f0000004 	jmp 20 <there>
			10: R_MMIX_ADDR27	\.text\+0x20
  14:	f1fffffc 	jmp 4 <here>
			14: R_MMIX_ADDR27	\.text\+0x4
  18:	f0000002 	jmp 20 <there>
			18: R_MMIX_ADDR27	\.text\+0x20
  1c:	f1fffffa 	jmp 4 <here>
			1c: R_MMIX_ADDR27	\.text\+0x4

0000000000000020 <there>:
  20:	fd000000 	swym 0,0,0

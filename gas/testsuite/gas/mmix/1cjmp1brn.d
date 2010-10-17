# objdump: -dr
# source: 1cjmp1b.s
# as: -linkrelax -no-expand
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0
   4:	f0000000 	jmp 4 <Main\+0x4>
			4: R_MMIX_ADDR27	\.text\+0x8
   8:	f0000000 	jmp 8 <Main\+0x8>
			8: R_MMIX_ADDR27	\.text\+0x8
   c:	f0000000 	jmp c <Main\+0xc>
			c: R_MMIX_ADDR27	\.text\+0x8

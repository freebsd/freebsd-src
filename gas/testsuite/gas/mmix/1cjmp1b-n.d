# objdump: -dr
# source: 1cjmp1b.s
# as: -no-expand
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0
   4:	f0000001 	jmp 8 <Main\+0x8>
   8:	f0000000 	jmp 8 <Main\+0x8>
   c:	f1ffffff 	jmp 8 <Main\+0x8>

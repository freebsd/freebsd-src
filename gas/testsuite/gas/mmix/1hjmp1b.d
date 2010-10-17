# objdump: -dr
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0
   4:	f0000001 	jmp 8 <Main\+0x8>
   8:	f1ffffff 	jmp 4 <Main\+0x4>
   c:	f1ffffff 	jmp 8 <Main\+0x8>

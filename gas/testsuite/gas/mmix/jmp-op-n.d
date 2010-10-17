# objdump: -dr
# source: jmp-op.s
# as: -no-expand
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0

0000000000000004 <here>:
   4:	fd000000 	swym 0,0,0
   8:	f1ffffff 	jmp 4 <here>

000000000000000c <at>:
   c:	f0000000 	jmp c <at>
  10:	f0000004 	jmp 20 <there>
  14:	f1fffffc 	jmp 4 <here>
  18:	f0000002 	jmp 20 <there>
  1c:	f1fffffa 	jmp 4 <here>

0000000000000020 <there>:
  20:	fd000000 	swym 0,0,0

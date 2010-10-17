# objdump: -dr
# as: -linkrelax
# source: put-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	f604007b 	put rJ,\$123
   4:	f613002d 	put rG,\$45
   8:	f61f00f5 	put rZZ,\$245
   c:	f604006f 	put rJ,\$111
  10:	f713002d 	put rG,45
  14:	f71f00f5 	put rZZ,245
  18:	f7040000 	put rJ,0
  1c:	f7130000 	put rG,0
  20:	f71f0000 	put rZZ,0

# objdump: -dr
# as: -linkrelax
# source: round2-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	088700f4 	flot \$135,\$244
   4:	0a8700e9 	flotu \$135,\$233
   8:	0d85005b 	sflot \$133,91
   c:	177b00f4 	fint \$123,\$244
  10:	0c8500f4 	sflot \$133,\$244
  14:	0987005b 	flot \$135,91
  18:	0f7b005b 	sflotu \$123,91
  1c:	05ad00e9 	fix \$173,\$233
  20:	0bad00d5 	flotu \$173,213
  24:	078700f4 	fixu \$135,\$244
  28:	0b870077 	flotu \$135,119

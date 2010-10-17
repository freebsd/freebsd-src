# objdump: -dr
# as: -linkrelax
# source: roundr-op.s
.*:     file format elf64-mmix

Disassembly of section .text:

0000000000000000 <Main>:
   0:	178701f4 	fint \$135,ROUND_OFF,\$244
   4:	058702e9 	fix \$135,ROUND_UP,\$233
   8:	178500f4 	fint \$133,\$244
   c:	157b04f4 	fsqrt \$123,ROUND_NEAR,\$244
  10:	17ad02e9 	fint \$173,ROUND_UP,\$233
  14:	058700f4 	fix \$135,\$244
  18:	078700f4 	fixu \$135,\$244

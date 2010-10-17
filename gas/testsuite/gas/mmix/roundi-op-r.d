# objdump: -dr
# as: -linkrelax
# source: roundi-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	088701f4 	flot \$135,ROUND_OFF,\$244
   4:	0a8702e9 	flotu \$135,ROUND_UP,\$233
   8:	0d85005b 	sflot \$133,91
   c:	0e7b04f4 	sflotu \$123,ROUND_NEAR,\$244
  10:	0c8500f4 	sflot \$133,\$244
  14:	0987005b 	flot \$135,91
  18:	0f7b045b 	sflotu \$123,ROUND_NEAR,91
  1c:	0987015b 	flot \$135,ROUND_OFF,91
  20:	0aad02e9 	flotu \$173,ROUND_UP,\$233
  24:	0bad02d5 	flotu \$173,ROUND_UP,213
  28:	0c8700f4 	sflot \$135,\$244
  2c:	0b870277 	flotu \$135,ROUND_UP,119
  30:	0d87005b 	sflot \$135,91
  34:	088700f4 	flot \$135,\$244

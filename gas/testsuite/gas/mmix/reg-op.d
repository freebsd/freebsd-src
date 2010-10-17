# objdump: -dr
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	10170c43 	fmul \$23,\$12,\$67
   4:	01200c43 	fcmp \$32,\$12,\$67
   8:	040c2043 	fadd \$12,\$32,\$67
   c:	02e88543 	fun \$232,\$133,\$67
  10:	03170c49 	feql \$23,\$12,\$73
  14:	161f0ce9 	frem \$31,\$12,\$233
  18:	061726d4 	fsub \$23,\$38,\$212
  1c:	1304afb5 	feqle \$4,\$175,\$181

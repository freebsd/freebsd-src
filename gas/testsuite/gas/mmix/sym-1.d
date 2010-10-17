#objdump: -dt

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  .text	0+ 
0+ l    d  .data	0+ 
0+ l    d  .bss	0+ 
0+10 l       .text	0+ scl1
0+14 l       .text	0+ :scl2
0+20 l       .text	0+ endcl1
0+24 l       .text	0+ endcl2:
0+ g     F .text	0+ Main
0+4 g       .text	0+ scg1
0+8 g       .text	0+ scg2
0+c g       .text	0+ :scg3
0+18 g       .text	0+ endcg1
0+1c g       .text	0+ endcg2:


Disassembly of section .text:

0+ <Main>:
   0:	fd000410 	swym 0,4,16

0+4 <scg1>:
   4:	fd100400 	swym 16,4,0

0+8 <scg2>:
   8:	fda12a1e 	swym 161,42,30

0+c <:scg3>:
   c:	fda32a14 	swym 163,42,20

0+10 <scl1>:
  10:	fd010203 	swym 1,2,3

0+14 <:scl2>:
  14:	fd010204 	swym 1,2,4

0+18 <endcg1>:
  18:	fd030201 	swym 3,2,1

0+1c <endcg2:>:
  1c:	fd030201 	swym 3,2,1

0+20 <endcl1>:
  20:	fd040302 	swym 4,3,2

0+24 <endcl2:>:
  24:	fd040302 	swym 4,3,2

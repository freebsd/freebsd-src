# objdump: -dr
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	fd000000 	swym 0,0,0

0000000000000004 <here>:
   4:	fd000000 	swym 0,0,0
   8:	f519ffff 	geta \$25,4 <here>

000000000000000c <at>:
   c:	f4200000 	geta \$32,c <at>
  10:	424e0008 	bz \$78,30 <there>
  14:	f35bfffc 	pushj \$91,4 <here>
  18:	f387fffb 	pushj \$135,4 <here>
  1c:	f4870005 	geta \$135,30 <there>
  20:	f2870004 	pushj \$135,30 <there>
  24:	f2490003 	pushj \$73,30 <there>
  28:	f2380002 	pushj \$56,30 <there>
  2c:	5f87fff6 	pbev \$135,4 <here>

0000000000000030 <there>:
  30:	fd000000 	swym 0,0,0

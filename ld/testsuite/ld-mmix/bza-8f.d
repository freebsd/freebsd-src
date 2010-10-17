#source: start.s
#source: bza.s
#source: a.s
#as: -no-expand
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
   0+:	e3fd0001 	setl \$253,0x1

0+4 <bza>:
   4:	e3fd0002 	setl \$253,0x2
   8:	42ea0002 	bz \$234,10 <a>
   c:	e3fd0003 	setl \$253,0x3

0+10 <a>:
  10:	e3fd0004 	setl \$253,0x4

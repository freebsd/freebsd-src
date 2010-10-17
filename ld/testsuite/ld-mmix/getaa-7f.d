#source: start.s
#source: getaa.s
#source: a.s
#as: -x
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
   0+:	e3fd0001 	setl \$253,0x1

0+4 <getaa>:
   4:	e3fd0002 	setl \$253,0x2
   8:	e37b001c 	setl \$123,0x1c
   c:	e67b0000 	incml \$123,0x0
  10:	e57b0000 	incmh \$123,0x0
  14:	e47b0000 	inch \$123,0x0
  18:	e3fd0003 	setl \$253,0x3

0+1c <a>:
  1c:	e3fd0004 	setl \$253,0x4

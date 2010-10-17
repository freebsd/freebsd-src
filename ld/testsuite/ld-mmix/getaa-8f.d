#source: start.s
#source: getaa.s
#source: a.s
#as: -no-expand
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
   0+:	e3fd0001 	setl \$253,0x1

0+4 <getaa>:
   4:	e3fd0002 	setl \$253,0x2
   8:	f47b0002 	geta \$123,10 <a>
   c:	e3fd0003 	setl \$253,0x3

0+10 <a>:
  10:	e3fd0004 	setl \$253,0x4

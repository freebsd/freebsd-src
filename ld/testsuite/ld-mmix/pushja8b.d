#source: start.s
#source: a.s
#source: pushja.s
#as: -no-expand
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
   0:	e3fd0001 	setl \$253,0x1

0+4 <a>:
   4:	e3fd0004 	setl \$253,0x4

0+8 <pushja>:
   8:	e3fd0002 	setl \$253,0x2
   c:	f30cfffe 	pushj \$12,4 <a>
  10:	e3fd0003 	setl \$253,0x3

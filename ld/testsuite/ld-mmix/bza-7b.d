#source: start.s
#source: a.s
#source: bza.s
#as: -x
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
   0+:	e3fd0001 	setl \$253,0x1

0+4 <a>:
   4:	e3fd0004 	setl \$253,0x4

0+8 <bza>:
   8:	e3fd0002 	setl \$253,0x2
   c:	5aea0006 	pbnz \$234,24 <bza\+0x1c>
  10:	e3ff0004 	setl \$255,0x4
  14:	e6ff0000 	incml \$255,0x0
  18:	e5ff0000 	incmh \$255,0x0
  1c:	e4ff0000 	inch \$255,0x0
  20:	9fffff00 	go \$255,\$255,0
  24:	e3fd0003 	setl \$253,0x3

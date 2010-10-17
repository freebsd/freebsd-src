#source: start.s
#source: a.s
#source: pushja.s
#as: -x --no-pushj-stubs
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
   c:	e3ff0004 	setl \$255,0x4
  10:	e6ff0000 	incml \$255,0x0
  14:	e5ff0000 	incmh \$255,0x0
  18:	e4ff0000 	inch \$255,0x0
  1c:	bf0cff00 	pushgo \$12,\$255,0
  20:	e3fd0003 	setl \$253,0x3

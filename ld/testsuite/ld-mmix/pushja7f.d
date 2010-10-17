#source: start.s
#source: pushja.s
#source: a.s
#as: -x --no-pushj-stubs
#ld: -m mmo
#objdump: -dr

.*:     file format mmo

Disassembly of section \.text:

0+ <(Main|_start)>:
   0:	e3fd0001 	setl \$253,0x1

0+4 <pushja>:
   4:	e3fd0002 	setl \$253,0x2
   8:	e3ff0020 	setl \$255,0x20
   c:	e6ff0000 	incml \$255,0x0
  10:	e5ff0000 	incmh \$255,0x0
  14:	e4ff0000 	inch \$255,0x0
  18:	bf0cff00 	pushgo \$12,\$255,0
  1c:	e3fd0003 	setl \$253,0x3

0+20 <a>:
  20:	e3fd0004 	setl \$253,0x4

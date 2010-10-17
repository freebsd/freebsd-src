#source: greg-1.s
#source: gregpsj1.s
#source: start.s
#source: a.s
#as: -x --no-pushj-stubs
#ld: -m mmo
#objdump: -dt

# Like greg-3, but a different expanding insn.

.*:     file format mmo

SYMBOL TABLE:
0+14 g       \.text Main
0+14 g       \.text _start
0+fe g       \*REG\* areg
0+18 g       \.text a

Disassembly of section \.text:

0+ <(Main|_start)-0x14>:
   0:	e3ff0018 	setl \$255,0x18
   4:	e6ff0000 	incml \$255,0x0
   8:	e5ff0000 	incmh \$255,0x0
   c:	e4ff0000 	inch \$255,0x0
  10:	bffeff00 	pushgo areg,\$255,0

0+14 <(Main|_start)>:
  14:	e3fd0001 	setl \$253,0x1

0+18 <a>:
  18:	e3fd0004 	setl \$253,0x4

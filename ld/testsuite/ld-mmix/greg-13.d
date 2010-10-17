#source: greg-1.s
#source: gregbza1.s
#source: start.s
#source: a.s
#as: -x
#ld: -m mmo
#objdump: -dt

# Like greg-3, but a different expanding insn, mmo.

.*:     file format mmo

SYMBOL TABLE:
0+18 g       \.text Main
0+18 g       \.text _start
0+fe g       \*REG\* areg
0+1c g       \.text a

Disassembly of section \.text:

0+ <(Main|_start)-0x18>:
   0:	5afe0006 	pbnz areg,18 <(Main|_start)>
   4:	e3ff001c 	setl \$255,0x1c
   8:	e6ff0000 	incml \$255,0x0
   c:	e5ff0000 	incmh \$255,0x0
  10:	e4ff0000 	inch \$255,0x0
  14:	9fffff00 	go \$255,\$255,0

0+18 <(Main|_start)>:
  18:	e3fd0001 	setl \$253,0x1

0+1c <a>:
  1c:	e3fd0004 	setl \$253,0x4

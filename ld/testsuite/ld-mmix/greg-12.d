#source: greg-1.s
#source: gregget1.s
#source: start.s
#source: a.s
#as: -x
#ld: -m mmo
#objdump: -dt

# A greg usage with an expanding insn.  The register reloc must be
# evaluated before the expanding reloc.  Here, it doesn't appear in the
# wrong order, and it doesn't seem like they would naturally appear in the
# wrong order, but anyway; mmo.

.*:     file format mmo

SYMBOL TABLE:
0+10 g       \.text Main
0+10 g       \.text _start
0+fe g       \*REG\* areg
0+14 g       \.text a

Disassembly of section \.text:

0+ <(Main|_start)-0x10>:
   0:	e3fe0014 	setl areg,0x14
   4:	e6fe0000 	incml areg,0x0
   8:	e5fe0000 	incmh areg,0x0
   c:	e4fe0000 	inch areg,0x0

0+10 <(Main|_start)>:
  10:	e3fd0001 	setl \$253,0x1

0+14 <a>:
  14:	e3fd0004 	setl \$253,0x4

#source: gregget1.s
#source: greg-1.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-5.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: greg-4.s
#source: a.s
#source: start.s
#as: -x
#ld: -m mmo
#objdump: -dt

# Allocating the maximum number of gregs and referring to one at the
# *other* end still works, mmo.

.*:     file format mmo

SYMBOL TABLE:
0+14 g       \.text Main
0+14 g       \.text _start
0+20 g       \*REG\* areg
0+10 g       \.text a

Disassembly of section \.text:

0+ <a-0x10>:
   0:	e3200010 	setl areg,0x10
   4:	e6200000 	incml areg,0x0
   8:	e5200000 	incmh areg,0x0
   c:	e4200000 	inch areg,0x0

0+10 <a>:
  10:	e3fd0004 	setl \$253,0x4

0+14 <(Main|_start)>:
  14:	e3fd0001 	setl \$253,0x1

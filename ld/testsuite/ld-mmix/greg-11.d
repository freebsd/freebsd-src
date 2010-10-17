#source: greg-1.s
#source: gregldo1.s
#source: gregget2.s
#source: a.s
#source: greg-3.s
#source: start.s
#source: greg-2.s
#as: -x
#ld: -m mmo
#objdump: -dt

# Have two used gregs and one unused, mmo.

.*:     file format mmo

SYMBOL TABLE:
0+20 g       \.text Main
0+fe g       \*REG\* b
0+20 g       \.text _start
0+fc g       \*REG\* areg
0+fd g       \*REG\* c
0+1c g       \.text a

Disassembly of section \.text:

0+ <a-0x1c>:
   0:	8c0c20fc 	ldo \$12,\$32,areg
   4:	8d7bfc22 	ldo \$123,areg,34
   8:	8dfcea38 	ldo areg,\$234,56
   c:	e3fe001c 	setl b,0x1c
  10:	e6fe0000 	incml b,0x0
  14:	e5fe0000 	incmh b,0x0
  18:	e4fe0000 	inch b,0x0

0+1c <a>:
  1c:	e3fd0004 	setl c,0x4

0+20 <(Main|_start)>:
  20:	e3fd0001 	setl c,0x1

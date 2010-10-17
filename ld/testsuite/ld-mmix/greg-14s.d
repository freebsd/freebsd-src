#source: greg-1.s
#source: gregpsj1.s
#source: start.s
#source: a.s
#as: -x
#ld: -m mmo
#objdump: -dt

# Like greg-14, but using PUSHJ stubs.

.*:     file format mmo
SYMBOL TABLE:
0+4 g       \.text Main
0+4 g       \.text _start
0+fe g       \*REG\* areg
0+8 g       \.text a
Disassembly of section \.text:
0+ <(Main|_start)-0x4>:
   0:	f2fe0002 	pushj areg,8 <a>
0+4 <(Main|_start)>:
   4:	e3fd0001 	setl \$253,0x1
0+8 <a>:
   8:	e3fd0004 	setl \$253,0x4

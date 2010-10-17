#source: greg-1.s
#source: gregldo1.s
#source: start.s
#ld: -m mmo
#objdump: -dt

# Most simple greg usage: relocate to each possible location within an
# insn; mmo.

.*:     file format mmo

SYMBOL TABLE:
0+c g       \.text Main
0+c g       \.text _start
0+fe g       \*REG\* areg

Disassembly of section \.text:

0+ <Main-0xc>:
   0:	8c0c20fe 	ldo \$12,\$32,areg
   4:	8d7bfe22 	ldo \$123,areg,34
   8:	8dfeea38 	ldo areg,\$234,56

0+c <(Main|_start)>:
   c:	e3fd0001 	setl \$253,0x1

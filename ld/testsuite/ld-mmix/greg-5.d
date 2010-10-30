#source: greg-1.s
#source: gregpsj1.s
#source: start.s
#source: a.s
#as: -x --no-pushj-stubs
#ld: -m elf64mmix
#objdump: -dt

# Like greg-3, but a different expanding insn.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+14 g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...
0+18 g       \.text	0+ a

Disassembly of section \.text:

0+ <_start-0x14>:
   0:	e3ff0018 	setl \$255,0x18
   4:	e6ff0000 	incml \$255,0x0
   8:	e5ff0000 	incmh \$255,0x0
   c:	e4ff0000 	inch \$255,0x0
  10:	bffeff00 	pushgo \$254,\$255,0

0+14 <_start>:
  14:	e3fd0001 	setl \$253,0x1

0+18 <a>:
  18:	e3fd0004 	setl \$253,0x4

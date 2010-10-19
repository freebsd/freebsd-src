#source: greg-1.s
#source: gregget1.s
#source: start.s
#source: a.s
#as: -x
#ld: -m elf64mmix
#objdump: -dt

# A greg usage with an expanding insn.  The register reloc must be
# evaluated before the expanding reloc.  Here, it doesn't appear in the
# wrong order, and it doesn't seem like they would naturally appear in the
# wrong order, but anyway.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+10 g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...
0+14 g       \.text	0+ a

Disassembly of section \.text:

0+ <_start-0x10>:
   0:	e3fe0014 	setl \$254,0x14
   4:	e6fe0000 	incml \$254,0x0
   8:	e5fe0000 	incmh \$254,0x0
   c:	e4fe0000 	inch \$254,0x0

0+10 <_start>:
  10:	e3fd0001 	setl \$253,0x1

0+14 <a>:
  14:	e3fd0004 	setl \$253,0x4

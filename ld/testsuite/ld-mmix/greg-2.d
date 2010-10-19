#source: greg-1.s
#source: gregldo1.s
#source: gregget2.s
#source: a.s
#source: greg-3.s
#source: start.s
#source: greg-2.s
#as: -x
#ld: -m elf64mmix
#objdump: -dt

# Have two used gregs and one unused.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7e0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+fe g       \*REG\*	0+ b
0+20 g       \.text	0+ _start
0+fc g       \*REG\*	0+ areg
0+fd g       \*REG\*	0+ c
#...
0+1c g       \.text	0+ a

Disassembly of section \.text:

0+ <a-0x1c>:
   0:	8c0c20fc 	ldo \$12,\$32,\$252
   4:	8d7bfc22 	ldo \$123,\$252,34
   8:	8dfcea38 	ldo \$252,\$234,56
   c:	e3fe001c 	setl \$254,0x1c
  10:	e6fe0000 	incml \$254,0x0
  14:	e5fe0000 	incmh \$254,0x0
  18:	e4fe0000 	inch \$254,0x0

0+1c <a>:
  1c:	e3fd0004 	setl \$253,0x4

0+20 <_start>:
  20:	e3fd0001 	setl \$253,0x1

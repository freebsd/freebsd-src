#source: greg-1.s
#source: gregpsj1.s
#source: start.s
#source: a.s
#as: -x
#ld: -m elf64mmix
#objdump: -dt

# Like greg-3, but a different expanding insn.

.*:     file format elf64-mmix
SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+4 g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...
0+8 g       \.text	0+ a
Disassembly of section \.text:
0+ <_start-0x4>:
   0:	f2fe0002 	pushj \$254,8 <a>
0+4 <_start>:
   4:	e3fd0001 	setl \$253,0x1
0+8 <a>:
   8:	e3fd0004 	setl \$253,0x4

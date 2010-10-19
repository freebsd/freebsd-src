# as: -no-predefined-syms
# objdump: -dtr
# source: builtin1.s

# Make sure we don't look at the symbol table when parsing special
# register names.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+14 l       \*ABS\*	0+ rJ
0+ g     F \.text	0+ Main


Disassembly of section \.text:

0+ <Main>:
   0:	fe050004 	get \$5,rJ
   4:	fe060004 	get \$6,rJ
   8:	f6040007 	put rJ,\$7
   c:	f6040008 	put rJ,\$8

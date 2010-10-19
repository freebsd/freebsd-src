# objdump: -dtr

# Make sure we can override a built-in symbol with a known constant, like
# with mmixal.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+14 l       \*ABS\*	0+ rJ
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ g     F \.text	0+ Main


Disassembly of section \.text:

0+ <Main>:
   0:	fe050014 	get \$5,rL
   4:	fe060014 	get \$6,rL
   8:	f6140007 	put rL,\$7
   c:	f6140008 	put rL,\$8

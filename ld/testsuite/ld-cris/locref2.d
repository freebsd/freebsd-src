#as: --no-underscore --pic
#source: gotrel1.s
#source: locref2.s
#ld: -m crislinux
#objdump: -dt

# Referencing an undefined weak (non-hidden) symbol with a local-only
# PIC relocation is ok when building an executable.

.*:     file format elf32-cris

SYMBOL TABLE:
#...
0+  w      \*UND\*	0+ expfn
0+  w      \*UND\*	0+ expobj
#...
0+820a0 g     O \.got	0+ _GLOBAL_OFFSET_TABLE_
#...
Disassembly of section \.text:
#...
0+8007c <y>:
   8007c:	6fae 60df f7ff  .*
   80082:	6fbe 60df f7ff  .*

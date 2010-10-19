#as: --no-underscore --pic --em=criself
#source: gotrel1.s
#source: locref2.s
#ld: -m crislinux
#objdump: -dt

# Referencing an undefined weak (non-hidden) symbol with a local-only
# PIC relocation is ok when building an executable.

.*:     file format elf32-cris

SYMBOL TABLE:
#...
0+82088 l     O \.got	0+ \.hidden _GLOBAL_OFFSET_TABLE_
0+  w      \*UND\*	0+ expfn
0+  w      \*UND\*	0+ expobj
#...
Disassembly of section \.text:
#...
0+8007c <y>:
   8007c:	6fae 78df f7ff  .*
   80082:	6fbe 78df f7ff  .*

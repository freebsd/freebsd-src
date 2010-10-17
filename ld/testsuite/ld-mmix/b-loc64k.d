#source: b-loc64k.s
#source: b-goodmain.s
#ld: --oformat binary
#objcopy_linked_file:
#objdump: -dht

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+10004  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
SYMBOL TABLE:
0+4 g       \.text Main
0+4 g       \.text a

Disassembly of section \.text:

0+ <Main-0x4>:
       0:	e3fd0001 	setl \$253,0x1

0+4 <Main>:
	\.\.\.
   10000:	e3fd0004 	setl \$253,0x4

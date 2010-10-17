#source: start.s
#source: a.s
#ld: -T $srcdir/$subdir/mmohdr1.ld
#objdump: -sht

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+8  0+100  0+100  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
SYMBOL TABLE:
0+100 g       \.text Main
0+100 g       \.text _start
0+104 g       \.text a


Contents of section \.text:
 0100 e3fd0001 e3fd0004                    .*

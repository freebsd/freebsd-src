#source: sec-2.s
#source: start.s
#source: data1.s
#ld: -m mmo -T $srcdir/$subdir/mmosec1.ld
#objdump: -sh

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+4  0+100  0+100  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.data         0+4  2000000000000004  2000000000000004  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
  2 \.other        0+c  1000000000000000  1000000000000000  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
Contents of section \.text:
 0100 e3fd0001                             .*
Contents of section \.data:
 2000000000000004 0000012c                             .*
Contents of section \.other:
 1000000000000000 0000000c 00000022 00000001           .*

#source: sec-1.s
#source: start.s
#source: data1.s
#ld: -m mmo -T $srcdir/$subdir/mmosec1.ld
#objdump: -sh

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+4  0+100  0+100  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.other        0+50  1000000000000000  1000000000000000  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE, DATA
  2 \.data         0+4  2000000000000004  2000000000000004  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
Contents of section \.text:
 0100 e3fd0001                             .*
Contents of section \.other:
 1000000000000000 00000001 00000002 00000003 00000004  .*
 1000000000000010 ffffffff fffff827 50000000 0000000a  .*
 1000000000000020 00000009 00000008 00000007 25272900  .*
 1000000000000030 00030d41 000186a2 26280000 00000000  .*
 1000000000000040 00000000 0087a238 00000000 302a55a8  .*
Contents of section \.data:
 2000000000000004 0000012c                             .*

#source: start.s
#source: sec-7a.s
#source: sec-7b.s
#source: sec-7c.s
#source: sec-7d.s
#source: sec-7e.s
#ld: -m mmo
#objcopy_linked_file:
#objdump: -hs

# When producing mmo output: sections with an input length not a
# multiple of 4, and whose total length in linked output
# exceeded the "chunk size" (32768), would get to-4-padding
# inserted at each chunk division.  Also check that section
# sizes aren't rounded up at objcopy.

.*:     file format mmo

Sections:
Idx Name[ ]+Size[ ]+VMA[ ]+LMA[ ]+File off  Algn
  0 \.text[ ]+0+4  0+  0+  0+  2\*\*2
[ ]+CONTENTS, ALLOC, LOAD, CODE
  1 \.di           0+27ffb  2000000000000000  2000000000000000  0+  2\*\*2
[ ]+CONTENTS, READONLY
Contents of section \.text:
 0000 e3fd0001[ ]+.*
Contents of section \.di:
 2000000000000000 2a000000 00000000 00000000 00000000  .*
#...
 2000000000007ff0 00000000 00000000 00000000 2b2c0000  .*
#...
 200000000000fff0 00000000 00000000 00002d2e 00000000  .*
#...
 2000000000017ff0 00000000 00000000 002f3000 00000000  .*
#...
 200000000001fff0 00000000 00000000 00313200 00000000  .*
#...
 2000000000027ff0 00000000 00000000 000033[ ]+.*

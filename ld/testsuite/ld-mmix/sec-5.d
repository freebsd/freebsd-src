#source: b-twoinsn.s
#source: b-offloc.s
#source: b-post1.s
#source: b-goodmain.s
#ld: --oformat binary
#objdump: -sh

# Check that sections are automatically created to cope with contents at
# unexpected addresses when an mmo is read in.  We used to do this by
# e.g. linking .text at an unexpected address, like in sec-9.d.  That no
# longer works, because .text and .data now gets section descriptors at
# mmo output when the address and contents doesn't trivially reflect the
# section contents at link time.  To test, we instead read in an mmo
# formed from a link to binary format, like the b-*.d tests for mmo
# execution paths.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+8  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.MMIX\.sec\.0   0+10  789abcdef0123458  789abcdef0123458  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
Contents of section \.text:
 0+ e3fd0001 e3fd0004                  .*
Contents of section \.MMIX\.sec\.0:
 789abcdef0123458 b045197d 2c1b03b2 e4dbf877 0fc766fb  .*

#source: b-twoinsn.s
#source: b-fixo2.s
#source: b-post1.s
#source: b-goodmain.s
#ld: --oformat binary
#objcopy_linked_file:
#objdump: -sht

# Note that we "optimize" out the high tetrabyte of 0 written to
# 2068098510aa5560, hence only the low part is left.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+8  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.data         0+4  2068098510aa5564  2068098510aa5564  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
SYMBOL TABLE:
0+4 g       \.text Main
0+4 g       \.text a

Contents of section \.text:
 0000 e3fd0001 e3fd0004                    .*
Contents of section \.data:
 2068098510aa5564 00000008                             .*

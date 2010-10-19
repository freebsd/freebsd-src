#source: start.s
#ld: -m mmo -T $srcdir/$subdir/mmosec2.ld
#objdump: -sh

# This is based on sec-5.d which used to link .text at an unexpected
# address to check that a special section was created in objdump when
# reading in contents at an unusual location without a proper section
# descriptor.  As .text (like .data) now gets a section descriptor when
# linked to an unexpected location, the old test is transformed into a
# specific check that the section description for .text works.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+4  1000000000000000  1000000000000000  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
Contents of section \.text:
 1000000000000000 e3fd0001                             .*

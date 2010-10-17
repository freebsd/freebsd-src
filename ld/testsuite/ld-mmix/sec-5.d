#source: start.s
#ld: -m mmo -T $srcdir/$subdir/mmosec2.ld
#objdump: -sh

# Check that sections are automatically created to cope with contents at
# unexpected addresses.  We do this by linking .text at an unexpected
# address.  As .text (like .data) does not get a section descriptor, the
# output gets a LOP_LOC at an unexpected address, and a unique section is
# created.  This test will have to be changed if .text gets a section
# descriptor if linked to an unexpected address.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+  0+  0+  0+  2\*\*2
                  
  1 \.MMIX\.sec\.0   0+4  1000000000000000  1000000000000000  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
Contents of section \.MMIX\.sec\.0:
 1000000000000000 e3fd0001                             .*

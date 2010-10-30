#PROG: objcopy
#objdump: -h
#objcopy: --set-section-flags foo=contents,alloc,load,code
#name: copy with setting section flags 2
#source: copytest.s
#not-target: *-*-aout
# Note - we use copytest.s and a section named "foo" rather
# than .text because for some file formats (eg PE) the .text
# section has a fixed set of flags and these cannot be changed.

.*: +file format .*

Sections:
Idx.*
#...
  [0-9]* foo.*
                  CONTENTS, ALLOC, LOAD, CODE
#...

#PROG: objcopy
#objdump: -h
#objcopy: --set-section-flags .post_text_reserve=contents,alloc,load,readonly,code
#name: copy with setting section flags 1

.*: +file format .*

Sections:
Idx.*
#...
  [0-9]* .post_text_reserve.*
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
#...

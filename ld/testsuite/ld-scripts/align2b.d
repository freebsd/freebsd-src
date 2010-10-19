# ld: --defsym data_align=32 -T align2.t
# objdump: --section-headers

[^:]+: +file +format.*

Sections:
Idx +Name +Size +VMA +LMA +File off +Algn
 +0 +\.text +[^ ]* +0+ +0+ .*
 +CONTENTS, +ALLOC, +LOAD,.* CODE
 +1 +\.data +[^ ]* +0+20 +0+20 .*
 +CONTENTS, +ALLOC, +LOAD, +DATA
#pass

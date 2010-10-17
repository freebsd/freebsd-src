 .text
 .macro foo
a:
 .long 42
 .endm
 .include "app4b.s"
 foo
b:
 .long 56

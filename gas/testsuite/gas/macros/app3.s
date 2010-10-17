#NO_APP
 .text
 .macro foo
a:
 .long 42
 .endm
#APP
 foo
b:
 .long 56
#NO_APP
c:
 .long 78

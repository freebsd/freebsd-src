! The implicit equation from a datalabel to the main symbol was incorrect
! at one time.  This is reasonably close to the original testcase.

 .mode SHcompact
start:
 nop
 nop
 nop
 nop
 nop
 nop
 nop
 nop
 .set dd,d
 .long   b
 .long   datalabel b
 .long   datalabel dd
 .word   0x1234
 .local  a
 .comm   a,4,4
 .local  b
 .comm   b,4,4
 .local  c
 .comm   c,4,4
 .local  d
 .comm   d,4,4

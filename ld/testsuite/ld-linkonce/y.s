;# Library file y.s has linkonce entries for fn and fn2.  Note
;# that this version of fn has different code, as if compiled
;# with different optimization flags than the one in x.s (not
;# important for this test, though).  The reference from
;# .gcc_except_table to the linkonce-excluded fn2 must be zero, or g++
;# EH will not work.

 .section .gnu.linkonce.t.fn2,"ax",@progbits
 .weak fn2
 .type fn2,@function
fn2:
.Lc:
 .long 3
.Ld:
 .size fn2,.Ld-.Lc

 .section .gnu.linkonce.t.fn,"ax",@progbits
 .weak fn
 .type fn,@function
fn:
.Le:
 .long 4
.Lf:
 .size fn,.Lf-.Le

 .section .gcc_except_table,"aw",@progbits
 .long 7
 .long .Lc
 .long .Ld-.Lc

 .long 0x6066
 .long .Le
 .long .Lf-.Le

 .section .eh_frame,"aw",@progbits
.Lframe1:
 .long .LECIE1-.LSCIE1
.LSCIE1:
 .long 0x0
 .byte 0x1
 .byte 0
 .uleb128 0x1
 .sleb128 -4
 .byte 0
 .p2align 2
.LECIE1:

.LSFDE1:
 .long .LEFDE1-.LASFDE1
.LASFDE1:
 .long .LASFDE1-.Lframe1
 .long .Lc
 .long .Ld-.Lc
 .p2align 2
.LEFDE1:

.LSFDE2:
 .long .LEFDE2-.LASFDE2
.LASFDE2:
 .long .LASFDE2-.Lframe1
 .long .Le
 .long .Lf-.Le
 .p2align 2
.LEFDE2:

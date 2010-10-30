;# Main file, x.s, with the program (_start) referring to two
;# linkonce functions fn and fn2.  The functions fn and fn2 are
;# supposed to be equivalent of C++ template instantiations; the
;# main file instantiates fn.

 .text
 .global _start
_start:
 .long fn
 .long fn2

 .section .gnu.linkonce.t.fn,"ax",@progbits
 .weak fn
 .type fn,@function
fn:
.La:
 .long 1
 .long 2
.Lb:
 .size fn,.Lb-.La

 .section .gcc_except_table,"aw",@progbits
 .long 2
 .long .La
 .long .Lb-.La

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
 .long .La
 .long .Lb-.La
 .p2align 2
.LEFDE1:

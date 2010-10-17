;# Main file, x.s, with the program (_start) referring to two
;# linkonce functions fn and fn2.  The functions fn and fn2 are
;# supposed to be equivalent of C++ template instantiations; the
;# main file instantiates fn.  There's the equivalent of an FDE
;# entry in .eh_frame, referring to fn via a local label.

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

 .section .eh_frame,"aw",@progbits
 .long 2
 .long .La
 .long .Lb-.La

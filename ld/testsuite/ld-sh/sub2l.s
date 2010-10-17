! "The subtraction of two symbols".
 .little
 .text
 .align 5
f:
 rts
f2:
 nop

 .section .text.foo,"ax",@progbits
 .align 5
 .global ff
ff:
 nop
 nop
 nop
 nop
L:
 rts
 nop
 .align 2
 .long f-L
 .long f2-L
 .long f2
 .long L
 .long ff+4

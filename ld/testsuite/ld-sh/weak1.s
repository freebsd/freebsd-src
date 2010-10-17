 .data
 .align 2
d0:
 .long 1
 .global w0
w0:
 .long 0
 .weak w0
 .text
 .align 5
f:
 mov.l .L3,r1
 mov.l @r1,r0
 rts 
 nop
 .align 2
.L3:
 .long w0


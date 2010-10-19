 moveq -1,r10
a:
 ba b
 moveq 1,r5

 .section .text.2,"ax"
 moveq 8,r2
b:
 moveq 2,r3
 ba a
 moveq 4,r7

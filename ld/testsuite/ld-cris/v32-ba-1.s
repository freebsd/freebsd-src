 .global a
a:
 ba b
 moveq 30,r8

 .section .text.2,"ax"
 .global b
b:
 moveq 3,r1
 ba a
 moveq 13,r11

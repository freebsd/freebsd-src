 .text
 .global _start
_start:
1:
 bl far
 nop
 bl far2far
 nop
 bl huge
 nop
 .long 0
 b 1b
 .space 0x1bf0000

 .section .text.pad1,"ax"
 .space 0x1bf0000

 .section .text.far,"ax"
far:
 blr

 .section .text.pad2,"ax"
 .space 0x40ffd8

 .section .text.far2far,"ax"
far2far:
 blr

 .section .text.pad3,"ax"
 .space 0x1bf0000

 .section .text.huge,"ax"
huge:
 blr

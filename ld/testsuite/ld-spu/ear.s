 .text
 .global _start
_start:
 br _start

#test old-style toe _EAR_ syms
 .section .toe,"a",@nobits
_EAR_:
 .space 16
_EAR_bar:
 .space 16

#test new-style _EAR_ syms
 .data
_EAR_main:
 .space 16

#new ones don't need to be 16 bytes apart
 .space 16
_EAR_foo:
 .space 16

 .section .data.blah,"aw",@progbits
_EAR_blah:
 .space 16

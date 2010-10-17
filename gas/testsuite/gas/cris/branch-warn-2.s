; Test warning for expansion of branches.

;  { dg-do assemble { target cris-*-* } }
;  { dg-options "-N" }

 .text
start:
 ba long_forward ; { dg-warning "32-bit conditional branch generated" }
 .space 32768,0
long_forward:
 nop

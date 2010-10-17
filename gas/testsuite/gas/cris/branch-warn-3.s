; Test warning for expansion of branches.

;  { dg-do assemble { target cris-*-* } }
;  { dg-options "-N" }

 .text
start:
 nop
 .space 32768,0
 ba start ; { dg-warning "32-bit conditional branch generated" }
 nop

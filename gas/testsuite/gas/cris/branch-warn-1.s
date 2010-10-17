; Test warning for expansion of branches.

;  { dg-do assemble { target cris-*-* } }
;  { dg-options "-N" }

 .text
start:
 ba external_symbol ; { dg-warning "32-bit conditional branch generated" }
 nop
